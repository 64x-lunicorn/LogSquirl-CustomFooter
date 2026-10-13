# Developer Guide — LogSquirl Custom Footer Plugin

This guide explains how the Custom Footer plugin works, how the
LogSquirl Plugin SDK is used, and how to extend or adapt the code.

## Plugin SDK Overview

The LogSquirl Plugin SDK is a **pure C ABI** interface.  A plugin is a
shared library (`.dylib` / `.so` / `.dll`) that exports these C functions:

| Symbol | Purpose |
|--------|---------|
| `logsquirl_plugin_get_info()` | Return static metadata (called before init) |
| `logsquirl_plugin_init_ex()` | Receive host API and its size, set up the plugin (LogSquirl 26.11) |
| `logsquirl_plugin_init()` | The same for older hosts: `init_ex()` with `LOGSQUIRL_HOST_API_BASE_SIZE` |
| `logsquirl_plugin_shutdown()` | Tear down, release all resources |
| `logsquirl_plugin_configure()` | Open a settings dialog (optional) |

The host provides a function-pointer table (`LogSquirlHostApi`) that the
plugin uses to interact with the application. The vendored header
`include/logsquirl_plugin_api.h` is the one of LogSquirl 26.11 (a verbatim
copy of LogSquirl's `src/plugins/include/logsquirl_plugin_api.h`). CI checks
it byte for byte against the release named by `host_ref` in
`.github/plugin-ci.json`. Until LogSquirl v26.11.0 is tagged, that is the
pre-release `v26.11.0-beta1`, which CI builds against but never releases;
before a release, set `host_ref` to `v26.11.0` and refresh the header with
LogSquirl-Plugin-CI's `scripts/sync-plugin.sh`.

**A growing table** (LogSquirl ADR 0017). Functions added to the host API
are appended to the end of the table, and `LOGSQUIRL_PLUGIN_API_VERSION`
stays 1. LogSquirl 26.11 calls `logsquirl_plugin_init_ex()` with the size
of its table; LogSquirl 26.10 knows only `logsquirl_plugin_init()`, and its
table ends where the added functions begin. `LOGSQUIRL_HOST_API_HAS( size,
member )` says whether a table of that size holds a member. The plugin keeps
the host's pointer, never copies the table (a copy would read the whole
struct of our header, past the end of an older host's table), and never
reads a member `LOGSQUIRL_HOST_API_HAS` does not report. LogSquirl 26.11
added three functions, which the footer will use (#20–#22):
`open_regex_lab`, `go_to_log_line` and `get_selected_log_lines`; see
[Host functions](#host-functions).

## Plugin Lifecycle

```mermaid
sequenceDiagram
    participant Host as LogSquirl Host
    participant Plugin as Custom Footer Plugin

    Host->>Plugin: logsquirl_plugin_get_info()
    Plugin-->>Host: &kPluginInfo (id, name, version, type, …)

    Host->>Plugin: logsquirl_plugin_init_ex(api, handle, api_size)
    Note right of Host: LogSquirl 26.10 calls logsquirl_plugin_init(api, handle),<br/>which is init_ex with LOGSQUIRL_HOST_API_BASE_SIZE
    Plugin->>Plugin: Keep api + handle, build HostFunctions from api (HostApiAdapter)
    Note right of Plugin: Only the members api_size reports are read
    Plugin->>Host: api->get_config_dir() (a warning if there is none)
    opt the host has no get_selected_log_lines (26.10)
        Plugin->>Host: api->register_menu_action("Plugins", "Custom Footer…", callback)
    end
    Plugin->>Plugin: Create the FooterDisplayWidget
    Plugin->>Host: api->register_footer_widget(footerWidget)
    Plugin->>Plugin: Create a FooterSession on it
    Note right of Plugin: The session's FooterController loads the rules<br/>and logs their problems as warnings
    opt the session has HostFeature::SelectedLogLines (26.11)
        Plugin->>Host: api->register_menu_action("Custom Footer", "Edit Rules…", callback)
        Plugin->>Host: api->register_menu_action("Custom Footer", "Rule from Selected Line…", callback)
    end
    Plugin->>Host: api->register_active_file_callback(onActiveFileChanged)
    Plugin->>Host: api->get_active_file_path() (on the UI thread)
    Plugin->>Plugin: FooterSession::setActiveFile(current file)
    Plugin->>Host: api->log_message("… ready. Host functions: …", the ones the session has)
    Plugin-->>Host: return 0 (success)

    Note over Host,Plugin: User opens or switches to a log file

    Host->>Plugin: onActiveFileChanged(filePath)
    Plugin->>Plugin: FooterSession::setActiveFile(filePath), for footer and editor
    Plugin->>Plugin: FooterScanner::scanFrom() on a worker thread
    Plugin->>Plugin: FooterDisplayWidget::updateValues(ordered), if still current

    Note over Host,Plugin: User clicks "Custom Footer…" (26.11: "Custom Footer → Edit Rules…")

    Host->>Plugin: onEditorMenuAction() callback
    Plugin->>Plugin: FooterSession::openEditor(main window): open(), and return
    Plugin->>Plugin: User edits rules, clicks OK (FooterEditor::finished)
    Plugin->>Plugin: The session saves the rules (FooterConfig::saveEntries())
    Plugin->>Plugin: and reloads them (FooterController::reloadConfig())

    Note over Host,Plugin: User clicks "Custom Footer → Rule from Selected Line…" (26.11)

    Host->>Plugin: onRuleFromLineMenuAction() callback
    Plugin->>Plugin: FooterSession::ruleFromSelectedLine(main window)
    Plugin->>Host: api->get_selected_log_lines() (on the UI thread)
    Plugin->>Plugin: Editor with a new rule and the first line, or a message: no line selected

    Note over Host,Plugin: Host unloads the plugin

    Host->>Plugin: logsquirl_plugin_shutdown()
    Plugin->>Plugin: Destroy the FooterSession:
    Note right of Plugin: delete the rule editor, cancel and wait for its preview;<br/>then cancel and wait for a running scan
    Plugin->>Host: api->unregister_footer_widget(footerWidget)
    Plugin->>Plugin: Delete the footer widget
    Plugin->>Plugin: Destroy the HostApiAdapter (drops Regex Lab answers still awaited)
```

`plugin.cpp` is only the adapter between the host's C API and the
[footer session](#footer-session). Every callback is one line into the
session, wrapped in `guarded()`.

## Threading

The footer follows the active file with a `FileFollower` (see
[File follower](#file-follower)), owned by `FooterController`, whose
worker thread is its own. Every started scan increments a generation
counter; a finished scan is shown only if no newer one was started, so
switching files quickly never shows stale values. Another active file, or
reloaded rules, cancel the running scan and scan afresh at once
(`FileFollower::follow()`). A change of the active file does not cancel:
the running scan finishes and is shown, and one follow-up scan then covers
every change made meanwhile, however many there were, no sooner than
`FollowerParts::kGapMs` (500 ms) after it finished, so the file is never
scanned back to back. Otherwise a log that changes more often than it can
be scanned would never show values. Setting the same active file again
counts as a change (`FileFollower::nudge()`). The result reaches the widget
through a `QFutureWatcher` on the GUI thread. `logsquirl_plugin_shutdown()`
destroys the footer session first, which deletes the editor and then the
controller; that stops the follower, which cancels the running scan, waits
for the worker and drops its wake-up: after it returns, no code of the
plugin runs, and the host may unload the library.

The controller keeps only its own concerns: the rules, the scan progress
and showing values. The scan progress is the follower's previous result:
the job continues `FooterScanner::scanFrom()` from the last scan shown,
which reads only what was appended to a file that only grew and scans a
truncated or replaced one from its start; `follow()` forgets it, so
another file or reloaded rules scan from the start. Kept there rather than
in the controller, the progress can never come from an outdated or
cancelled scan. A scan that throws is caught by the controller's job
itself and yields an empty scan: the footer is cleared rather than left
with stale values, and the next scan starts from the start of the file.
Where `scanFrom()` stays the one place that tracks progress, later
per-value details of a match (e.g. its line number) can travel with it
through incremental rescans. The follower's skip predicate is
`FooterScanner::fileUnchangedFor()` for the last scan: a change is not
scanned when the part of the file scanned is unchanged and either every
key has a value or a limit was reached (so appended lines are not read, as
in 0.4.0), or the file has the size it had when the scan opened it and
ends with the last complete line scanned (a last line without a line break
may have been rewritten in place).

Both workers are a `LatestJob` (`latestjob.h`): one worker thread, a
generation counter, a cancel flag, and results handed to the GUI thread
only for the latest job; `stop()` cancels, waits, and drops results not
yet handed over. Both learn of changes of the active file from a
`FileChangeSource`, which ignores queued changes of a file no longer
active and watches the file's directory while it is missing. It
reports a notification only when the file's existence, size,
modification or birth time differ from when its owner last read it:
Linux and Windows may report the owner's own reading, e.g. of the access
time, and the owner would otherwise read the file again every pause for
as long as it is open. A rewrite to the same size within the file
system's time resolution goes unnoticed, as for the footer's rescans.

The rule editor's live preview follows the active file with a
`FileFollower` (see [File follower](#file-follower)), owned by
`RulePreviewer`, whose worker thread is its own. Every request (an edit,
another selected rule, another active file) cancels the running preview,
and a result is emitted only for the latest request. An edit starts a
preview after `RulePreviewer::kDelayMs` (300 ms) without another one;
another rule or active file starts one with the next pass of the event
loop (`Start::Soon`), coalesced with the change that follows, so removing
the selected rule previews once, on the remaining rules. Until the
request starts, the follower is idle. Then the follower previews a change
on disk again after `FollowerParts::kGapMs` (500 ms), and a rotated file
once it is recreated. A running preview of the file is not cancelled for
a change but followed by one more, no sooner than the gap after it
finished, so a busy log is never scanned back to back, and not at all
when the change cannot alter it: lines appended beyond the limit the last
preview stopped at, or a file left as it was
(`FooterScanner::fileUnchangedFor()`, with the footer's identity checks
and its size rule, as the follower's skip predicate); the file is watched
on after such a skip. Like a scan, a preview takes the file's size when it
opens it, before reading (`Preview::fileSize`), so a line appended while
it reads is a change; and the size is still needed besides the end of the
last complete line read, as a file truncated to that end has lost a last
line without a line break that the preview showed. The preview always
scans the file from its start, unlike the footer, which continues where it
stopped. Setting the same active file again counts as a change
(`FileFollower::nudge()`).

The editor never runs in a nested event loop: `exec()` would keep plugin
frames on the stack, and the host may shut the plugin down, and unload
it, from within that loop. `FooterSession::openEditor()` creates it on
the heap, parented to the host's window, opens it with `open()`, and
handles OK and Apply through `finished()` and `applied()`; a closed
editor is deleted later. The host always passes its main window, even while its
application-modal Plugins dialog is open; the editor then goes over the
application-modal window that blocks its own (`placeEditor()`), and back
over its own window, content and all, when that one is hidden or
deleted (`ModalGuest`). Its Import and Export file dialogs, error messages and the
rule template dialog are opened with `open()` too, parented to the editor, never with the static
`QFileDialog` and `QMessageBox` functions, which run nested loops. The
one nested loop left is a drag in the rule list (`QDrag::exec()`); a
shutdown during a drag cannot be triggered from the UI. The session keeps a `QPointer` to it
until it is deleted, and the session's destructor deletes it at
once: the editor's destructor stops the preview (`RulePreviewer::stop()`
stops its edit timer and the follower, which cancels, waits for the
worker, drops its wake-up and deletes pending result watchers), and
deleting the objects drops their timers and posted
events. Closing the editor (`done()`) stops the preview for good; an edit
committed while it closes, such as an open mapping cell losing the
focus, starts none (`FooterEditor::closing_`). The destructor commits a
mapping cell being edited and disconnects the panel, model and list from
the editor before any child is deleted.

No exception may leave an entry point or host callback; they run their work
through `guarded()` (`hostfunctions.h`), which logs the failure instead. The
handlers the session connects to the editor's signals do the same, and so
does the Regex Lab's answer (`onRegexLabAnswer()` in `hostapiadapter.cpp`).

The host functions of LogSquirl 26.11 (`HostFunctions::openRegexLab`,
`goToLogLine`, `selectedLogLines`) must be called on the UI thread only, as
must `get_active_file_path`: the host refuses them elsewhere
(`LOGSQUIRL_LOG_LINES_NOT_ON_UI_THREAD`, or a Lab that does not open). Menu
actions, host callbacks and widget signals all run there, so the editor
calls them from its handlers; never from a scan or preview worker. The
Regex Lab's answer comes on the UI thread too, and the adapter's table of
Labs awaiting an answer is touched there only, without a lock.

## Footer Session

`FooterSession` (`footersession.h/.cpp`) owns everything one loaded
plugin needs:

- the rules and the line limit, read from and written to the config dir
  with `FooterConfig` by the session and its controller only; the editor
  gets them from the session and hands them back through its signals;
- the active file, the one the footer and the editor both see;
- the `FooterController`, which fills the footer widget;
- the rule editor, from when it opens until it is deleted: placing it over
  an application-modal window (`placeEditor()`, `ModalGuest`), saving on
  OK and Apply, deleting it once closed.

Its interface is small:

```cpp
FooterSession( const QString& configDir, FooterDisplayWidget* footer, HostFunctions host = {} );
~FooterSession();
void setActiveFile( const QString& filePath );
FooterEditor* openEditor( QWidget* over );
FooterEditor* ruleFromSelectedLine( QWidget* over );
bool has( HostFeature feature ) const noexcept;
```

The constructor loads the rules and starts the controller on the footer,
which it does not own. Without a config dir it uses the defaults and
saves nothing; the C entry points warn about that, as they know why.
`setActiveFile()` hands the file to the controller and to an open editor,
and remembers it for an editor opened later. `openEditor()` opens the editor with the rules, the line limit and
the active file, or moves the open one over the given window and raises
it; it returns at once. OK and Apply reach the session through the
editor's signals (`finished()`, `applied()`); the session saves the rules
and reloads them, and a failed save is logged as a warning and reloads
all the same. The editor never touches `FooterConfig`. All members run on
the UI thread.

`ruleFromSelectedLine()` is the second way into the editor, for
[Rule from a selected line](#rule-from-a-selected-line): it reads
`HostFunctions::selectedLogLines()` at once (`selectedLinesOrSayWhy()`),
opens the editor as `openEditor()` does (or brings the open one forward,
edits kept) and calls `FooterEditor::addRuleFromLine()` with the first line.
Without a selection it opens no editor and shows a message instead: a
`QMessageBox` opened with `open()`, never `exec()`, over the open editor or
else over the given window, tracked by the session, shown once however
often it is asked for, and deleted in the destructor right after the
editor. The host function does not tell "no selection" from "no Log File";
the session does, from its active file, and then says to open a Log File
first. Over the editor the message says to click OK first, which keeps the
rules and closes the editor (Cancel discards them), as the window-modal
editor blocks the window that takes the selection. Without the host function it
does nothing. The editor's **From selected line…** button reaches
`selectedLinesOrSayWhy()` through the editor's `ruleFromLineRequested()`
signal, which the session connects when it opens the editor; it offers
the button (`offerRuleFromLine()`) only when
`has( HostFeature::SelectedLogLines )`.

`HostFunctions` (`hostfunctions.h`) is what the session needs from the
host, as a value: `log`, and the three [host functions](#host-functions)
of LogSquirl 26.11. A member the running host does not offer stays empty,
and the session goes without it, so an older host is just the default
value. `HostApiAdapter` fills it from the host's function table; tests
fill it with lambdas. The controller gets its warning sink from the
session as a plain function, built from `log`. `has( HostFeature )` tells
whether the host offers `HostFeature::RegexLab`, `GoToLogLine` or
`SelectedLogLines`, derived from which members are set. The editor gets
what it needs of them when the session opens it: `openEditor()` hands
it the Regex Lab with `FooterEditor::setRegexLab()`, wrapped in
`guarded()` so that a throwing host function is logged and counts as a
Lab that did not open, or nothing on a host without it (see
[Regex Lab](#regex-lab)). It offers the editor's **From selected line…**
button only with `SelectedLogLines`, and the C entry points ask the same
before they register the menu for a rule from the selected line.

**Go to a value's Log Line (#21).** The session wires the footer itself;
the footer widget never learns about the host. Where `has(
HostFeature::GoToLogLine )`, the constructor calls
`footer->setGoToLineOffered( true )` and connects the footer's
`goToLineRequested( const FooterValue& )` to a handler that calls
`host.goToLogLine( value.lineNumber )`, on the UI thread like every
widget signal, inside `guarded()`. When the host returns false (no Log
File, or the line is gone since the scan, e.g. after a truncation), the
handler calls `footer->notify( value.key, "Could not go to Log Line N" )`,
a tooltip at the value like #15's "Copied", and no dialog. Without the
host function the footer is told it is not offered, and nothing is
connected. The destructor, after the controller is gone, disconnects and
withdraws the offer, so the footer offers nothing that no one handles.

On the widget side, `FooterDisplayWidget::setGoToLineOffered()` passes
the offer to every `FooterValueItem`, also to those made later. An item
can go to its line when it is offered and its value's `lineNumber` is
known (`canGoToLine()`); then its context menu shows "Go to Log Line"
(object name `goToLogLine`, hidden otherwise), its tooltip and accessible
description say how, and a left double-click emits
`goToLineRequested()`. Keyboard users open the context menu with the
context menu key. **Click versus double-click:** a double-click arrives
as press, release (a click, which copies, as #15 wants), double-click,
release. The item takes the double-click event itself and does not pass
it to `QToolButton`, so the second press does not press the button: no
second copy and no second click. So a double-click copies once and then
asks to go to the line; no timer tells clicks and double-clicks apart,
which would delay the copy and make tests depend on time. Without the
offer the double-click goes to `QToolButton` as before (two copies of
the same value). `notify()` shows a message at a key's value through
`FooterValueItem::showNotice()`, which "Copied" uses as well.

### Host functions

LogSquirl 26.11 appended three functions to the host API (LogSquirl
#662 and #663). `HostApiAdapter` (`hostapiadapter.h/.cpp`, part of the C
ABI adapter) turns each into a C++ member of `HostFunctions` that hides
the C details, and sets it only when `LOGSQUIRL_HOST_API_HAS( api_size,
… )` reports it. All three run on the UI thread only (see
[Threading](#threading)) and are valid until shutdown.

| Member | Host function | Behaviour |
|--------|---------------|-----------|
| `bool openRegexLab( const RegexLabPattern& pattern, const QObject* context, RegexLabDone done )` | `open_regex_lab` | Opens the Regex Lab with the pattern and its Match case flag and returns at once: true when it opened. `done` is called later, at most once: with the applied pattern on Apply, with `std::nullopt` on Cancel or when the Lab's window closes. Never once `context` (the editor that asked) is destroyed, nor after shutdown. |
| `bool goToLogLine( std::uint64_t lineNumber )` | `go_to_log_line` | Selects the line in the tab in front and scrolls it into view, as Go to line does. Lines count from 1, as LogSquirl shows them. False without a Log File, or for a line it does not have (yet). |
| `std::optional<SelectedLogLines> selectedLogLines()` | `get_selected_log_lines` | The selected Log Lines, copied at once (the host's buffer lasts only until the next call), split at `'\n'`, `truncated` when the host handed over only the first ones (1000 lines, 1 MiB). `std::nullopt` without a selection or a Log File. |

**The Regex Lab's answer.** The host calls back once, on the UI thread,
with the `user_data` the plugin passed. The adapter owns what the answer
needs (`done`, a `QPointer` to `context`) in a table of Labs awaiting an
answer, and gives the host a number from that table as `user_data`, never
a pointer, and numbers are never reused. An answer finds its entry, takes
it out, and calls `done` only while `context` lives; a second answer, or
one after the adapter is gone, finds nothing and does nothing. A Lab that
does not open is taken out at once. `logsquirl_plugin_shutdown()` destroys
the adapter after the session, which drops the entries still there, and
their `done` functions with them. The host does its part as well: it never
answers after shutdown and closes a Lab still open then (LogSquirl ADR
0017, #662). An editor feature only has to call
`host.openRegexLab( { pattern, matchesCase }, this, [this]( auto applied ) { … } )`.

At the end of `init_ex()` the plugin logs which host functions the session
has: `Custom Footer plugin ready. Host functions: Regex Lab, Go to line,
selected Log Lines.` on LogSquirl 26.11, `… Host functions: none.` on
26.10. Users see no change. `tests/hostapiadapter_test.cpp` drives the
adapter with a fake table that records calls, answers the Lab with Apply
or Cancel, and answers late, after the editor or the adapter is gone;
`tests/pluginlifecycle_test.cpp` loads the plugin through `init_ex()` with
a 26.11-sized table and through `init()` and `init_ex()` with a base table
in a heap block of exactly `LOGSQUIRL_HOST_API_BASE_SIZE` bytes, so that a
read past the table is a real out-of-bounds read, which an
AddressSanitizer build (`-DCMAKE_CXX_FLAGS="-fsanitize=address
-fno-omit-frame-pointer"`) reports.

**Shutdown postcondition.** The destructor deletes the editor at once,
open or closed but not yet deleted, with its dialogs, its preview
(stopped and waited for), its timers and its pending events; then it
destroys the controller, which cancels a running scan and waits for it;
then it disconnects from the footer.
When the destructor returns, no plugin code runs: no thread, timer, queued
event or callback reaches the session, the editor or the controller.
`tests/footersession_test.cpp` checks this with the editor open while its
preview and the footer's scan run, with a scan whose result would change
the footer running at destruction, with its import or template dialog
open, with a closed editor waiting to be deleted, and with the editor over
an application-modal window.

**What stays in the C ABI adapter.** `plugin.cpp` keeps file-local state
only: the host's table and handle, the `HostApiAdapter` with the
`HostFunctions` built from it, the footer widget (owned there, as the
host must unregister it after the session is gone) and the session.
`init_ex()` refuses a table smaller than `LOGSQUIRL_HOST_API_BASE_SIZE`;
`init()` is `init_ex()` with that size. Either calls the host in the same
order as 0.4.0 did: it reads the config dir (warning if there is none),
registers the menu action, creates and registers the footer widget,
creates the session on it (which logs the rules' problems), registers
the active-file callback, and pushes `get_active_file_path()` into the
session. Where `HostFunctions::selectedLogLines` is set (LogSquirl 26.11;
the same test as `has( HostFeature::SelectedLogLines )`, used for both
menus), "Custom Footer…" is not registered; instead, right after creating
the session, it registers `( "Custom Footer", "Edit Rules…" )` and
`( "Custom Footer", "Rule from Selected Line…" )`, which LogSquirl puts
into a submenu *Plugins → Custom Footer* (the menu path is a
slash-separated path of submenus since 26.10). A base table sees exactly
the calls of 0.4.0, which the FakeHost call-order tests pin; a 26.11
table's order is pinned too. If any step throws, `init_ex()` logs it,
shuts down and fails. The active file is read only there and in the
host's active-file callback, both on the UI thread, which LogSquirl 26.11
requires; the session never asks the host. `shutdown()` destroys the
session, then unregisters and deletes the footer widget, and destroys the
`HostApiAdapter` last, as the session's host functions call it. The
FakeHost tests in `tests/pluginlifecycle_test.cpp` drive these entry
points and observe only what a host and a user see: registrations, the
editor's window, saved files and log messages.

**Why this design.** The session has two verbs, `setActiveFile()` and
`openEditor()` (with `ruleFromSelectedLine()` as a second way to open the
editor, #22), and everything else (saving, reloading, the line limit,
placing the editor) happens behind them; public `save()`/`reload()`
methods or a `RuleStore` port would be shallow pass-throughs with the
editor as their only caller. Config I/O, the editor's lifetime and the
shutdown order sit in one module, so the adapter only translates. The
host seam is a value with two adapters, the C table and test lambdas;
virtual host ports with one-method sub-ports would have no second real
adapter. Host features that later versions add become optional members
of `HostFunctions`, and a query of which are set, without changing the
session's shape.

## File follower

`FileFollower<Result>` (`filefollower.h`) follows the active file for an
owner that needs the latest result of reading it: the editor's preview
(`RulePreviewer`) and the footer (`FooterController`). The owner gives it
a sink for results, and per set of inputs a job (file, previous result,
cancel flag → result) and optionally a predicate saying whether a change
can matter to the last result. It has three commands and a query:

- `follow( file, job, matters )`: the inputs changed. Every earlier run
  becomes outdated and its cancel flag is set, a pending rerun or wake-up
  is dropped, the previous result forgotten, a new file handed to the
  change source, and the job starts at once. `follow( {}, nullptr )` is
  idle: nothing runs and nothing is followed.
- `nudge()`: only the file changed, as when the change source reports a
  change. During a run, one rerun is flagged and nothing cancelled; while
  idle, a wake-up is set a gap from now; while a wake-up is pending,
  nothing changes, so a busy log never postpones it. A flagged rerun
  wakes up a gap after its run finished. When the wake-up comes, the
  file is watched again first; then, if the predicate says the change
  cannot matter, nothing runs, and otherwise the job runs. So a change
  made while the predicate or the job reads the file is always reported.
- `stop()`: cancel, wait for the worker, and deliver nothing more.
- `status()`: busy, waiting for the file, runs started and finished.

Its invariants, documented in the header and tested in
`tests/filefollower_test.cpp`: the change source is rearmed once per run,
before anything reads the file (for a wake-up before the predicate, never
between it and the job), so the file is watched whenever it is read; the
job's previous result is the last one delivered since `follow()`, never
one of an outdated or failed run, even one already on its way; the sink
gets only results of runs that are not outdated, never from within a
command; a job that throws counts as finished without a result, and the
gap and a rerun still apply; everything but the job runs on the
context's thread. The gap is defined once, `FollowerParts::kGapMs`
(500 ms), for the preview and the footer alike.

**Seams.** Two things are injected through the constructor
(`FollowerParts`), each with two adapters:

| Seam | Production adapter | Test adapter (`tests/followerfakes.h`) |
|------|--------------------|-----------------------------------------|
| `FollowerClock` (`followerclock.h`): `now()`, one `wakeAt()` | `QtFollowerClock`: `QElapsedTimer` and a single-shot `QTimer` | `ManualClock`: `setNow()` / `advance()` fire due wake-ups synchronously, in order |
| `ChangeSource` (`changesource.h`): `changed`, `setFile()`, `rearm()`, `waitingForFile()` | `FileChangeSource`: `QFileSystemWatcher`, the stamp filter, the directory while the file is missing | `FakeChangeSource`: `fire()`, counts rearms, settable `waiting` |

Null parts are the production adapters. The worker (`LatestJob`) is not a
seam: it stays inside, so results can never arrive synchronously. Owners
pass the parts through, `RulePreviewer( parent, parts )` and
`FooterController( widget, configDir, warn, parent, parts )`; the session
keeps the default. Tests of the follower, the preview and the footer
controller move the clock and fire changes themselves, without real
waiting and without reaching into the watcher; one smoke test of the
controller runs on a real file with the production parts.
`FileChangeSource` is tested with real files
(`tests/filechangesource_test.cpp`).

**Shutdown.** After `stop()` or the destructor no job code runs, the sink
is not called, no wake-up is pending, and the change source follows
nothing. An owner declares the follower after what the sink touches, or
calls `stop()` in its destructor, as `RulePreviewer` and
`FooterController` do (the controller declares it last as well).
`follow()` starts afresh after `stop()`.

**Why this design.** Three designs were compared: three commands
(follow, nudge, stop), a ports-and-adapters design with four ports
(clock, alarm, change source, worker) and seven methods, and a trivial
default whose test fakes come from an ambient, globally overridable
environment. The three commands are the two verbs of the ticket plus
shutdown, and hide the whole state machine (rerun, gap, skip, watch on
read, previous result, shutdown order): the module is deep, and every
timing rule is in one place. The seams are exactly the ones the ticket
names, clock and change source, injected through the constructor; an
injected worker would only have a test adapter and weaken the promise
that results never arrive synchronously, and an ambient override is
hidden global test state.

## Module Overview

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| **Plugin entry** | `plugin.cpp`, `plugin.h` | C ABI exports: translate between the host API and the session |
| **FooterSession** | `footersession.h/.cpp` | Rules and line limit in the config dir, active file, controller, editor lifetime, shutdown order |
| **HostFunctions** | `hostfunctions.h` | What the plugin needs from the host, as a value, its 26.11 functions optional; `guarded()` |
| **HostApiAdapter** | `hostapiadapter.h/.cpp` | Turns the host's C table into `HostFunctions`, reading only the members its size reports; owns the Regex Lab answers awaited |
| **FooterEntry** | `footerentry.h` | Data model: key, linePattern, valuePattern, mappings |
| **FooterController** | `footercontroller.h/.cpp` | Cached rules, scan progress and the shown values; scans through a file follower |
| **FooterScanner** | `footerscanner.h/.cpp` | Compiles the rules once; line-by-line scanning with two-stage matching |
| **FooterConfig** | `footerconfig.h/.cpp` | INI persistence + JSON import/export |
| **RuleCheck** | `rulecheck.h/.cpp` | Decides whether a rule is usable and, if not, why, for the scanner and the editor |
| **FooterEditor** | `footereditor.h/.cpp` | Rule editor dialog: rule list, detail panel, the rule check's problems |
| **RuleListModel** | `rulelistmodel.h/.cpp` | The editor's rules, one row each with its mappings and the rule check's problems |
| **RuleListView** | `rulelistview.h/.cpp` | The rule list: drag & drop and Ctrl+Shift+Up/Down to reorder rules |
| **RuleDetailPanel** | `ruledetailpanel.h/.cpp` | Form for the selected rule: fields, mappings, problem marks |
| **FileFollower** | `filefollower.h`, `followerclock.h/.cpp`, `changesource.h` | Follows the active file for an owner: runs its job, reruns after changes with a gap, skips changes that cannot matter |
| **FileChangeSource** | `filechangesource.h/.cpp` | The file system's changes of the active file, or of its directory while it is missing, without the owner's own reads |
| **LatestJob** | `latestjob.h` | One worker thread whose latest job alone hands over its result |
| **RulePreviewer** | `rulepreviewer.h/.cpp` | Runs the live preview: debounced, through a file follower, outdated results dropped |
| **RulePreviewView** | `rulepreviewview.h/.cpp` | The preview section of the detail panel: first match with highlights, values, count |
| **FooterValue** | `footervalue.h` | A shown value: key, displayed and raw value, the rule that supplied it, and the Log Line it was found in |
| **FooterDisplayWidget** | `footerdisplaywidget.h/.cpp` | Footer bar widget; one `FooterValueItem` per value, which copies it on a click and, where offered, asks to go to its Log Line on a double-click |
| **SimpleRule** | `simplerule.h/.cpp` | Simple mode: generates a rule's patterns from the text before its value, and classifies patterns |
| **RuleFromLine** | `rulefromline.h/.cpp` | Makes a simple rule and a key from a sample line and the value marked in it |
| **SelectedLinePicker** | `selectedlinepicker.h/.cpp` | The editor's view of the Log Line a rule is made from: click or mark the value |
| **RuleTemplate** | `ruletemplate.h/.cpp`, `ruletemplatedialog.h/.cpp` | Ready-made rules for common values in groups, and the dialog checking those to add |

## Scanning Algorithm

1. Build compiled `QRegularExpression` objects for each enabled rule, once
   per set of rules. The `RuleCheck` decides which: a rule without a line
   pattern is incomplete and ignored (`RuleCheck::isUsed()`); one with a
   problem, such as no key or an invalid line or value pattern, is skipped
   and reported in `FooterScanner::problems()` with the lines the editor
   lists for it (`RuleCheck::listing()`). Each pattern is compiled once:
   the check tells the problems from the scanner's compiled patterns.
2. Rules sharing a key are **alternatives**: the key's value comes from the
   first line that any of them matches; if several match that line, the
   rule higher in the list wins. The key is shown once, at the position of
   its first rule (`FooterScanner::footerValues()`).
3. Read the log file line by line, up to `maxLines` lines and at most
   64 MiB, even inside a single huge line. Lines longer than 64 KiB are
   matched against their first 64 KiB.
4. For each line, check every rule whose key has no value yet:
   - **Stage 1 — Line Pattern**: If `linePattern` matches the line, proceed.
   - **Stage 2 — Value Pattern** (optional): If `valuePattern` is set, apply
     it to the same line and extract capture group 1.  If not set, extract
     capture group 1 from the line pattern match. Without a capture group,
     the whole match is the value.
   - **Value Mapping**: If mappings are defined for the rule, substitute the
     raw value with the matching display value (exact string comparison).
5. Each found value is a `FooterValue`: key, shown value, raw value, the
   index of the rule that supplied it, and `lineNumber`, the Log Line it
   was found in, counting from 1 as LogSquirl does (a line ends at `\n`,
   so `\r\n` ends one line and a lone `\r` does not end one, as in the
   host; a UTF-8 byte order mark belongs to the first line; a last line
   without a line break counts too; 0 where unknown). As a key keeps the first value found, its line is the
   line of the shown value, also for rules sharing a key.
   `FooterScanner::footerValues()` hands them to the widget in rule order,
   for display, tooltips and going to the line. The preview's values carry
   their line as well (`Preview::lineNumber`, `keyLineNumber`).
6. Stop early once every key has a value.
7. **Incremental rescans**: `scanFrom()` returns the scan's progress
   (offset and count of the complete lines scanned, the values found, and
   bytes identifying the file). When the followed file changes, the next
   scan continues from there if the file only grew, and does not run at
   all once every key has a value or a limit was reached
   (`fileUnchangedFor( path, scan )`, the follower's skip predicate). A
   file that shrank, whose start or scanned end differs, or with another
   birth time, is scanned from its start. A last line without a line break
   is matched but not remembered, as it may still be being written.
   **Line numbers across rescans:** the progress counts the complete lines
   scanned, and a continued scan numbers its lines on from that count, so
   a value found in the appended part has its line in the whole file,
   and values kept from before keep theirs. A last line without a line
   break gets the number it will have, and is read again with the same
   number once complete. Where the 64 MiB limit stops the scan inside a
   line, that line's start is matched with that line's number. A file
   scanned from its start numbers afresh, so after a truncation or rotation
   the lines come from the new scan. `tests/footerscanner_linenumber_test.cpp`
   covers each case; `tests/footercontroller_test.cpp` checks the lines
   the footer shows as a file grows and is truncated.

## Rule Editor

`FooterEditor` shows the rules in a list on the left and the selected rule
in a detail panel on the right.

- **`RuleListModel`** holds the rules, in a `RuleCheck`. Each row is a
  whole `FooterEntry`, mappings included, plus its unfinished simple rule
  and its `RuleProblems`, which the check updates in the same step, so a
  rule's data and validation marks stay together whichever way rows are
  changed, inserted, removed or moved (`moveRows()`), and nothing that
  reacts to a signal of the model sees problems of other rows. The list shows
  the enabled state as the check box of the key column, the key, and the
  line pattern elided to one line; everything else is edited in the panel.
- **`RuleDetailPanel`** edits a copy of the selected rule and emits
  `edited()` on every change; the editor stores `entry()` into the selected
  row, which updates the list at once. `showEntry()` never emits `edited()`,
  so showing a rule cannot change it. Return in a field only confirms it
  and Escape reverts it, instead of closing the dialog. Actions that do not
  take the focus (the tool buttons, OK and Apply) first call
  `commitPendingEdit()`, so a mapping cell still being typed is kept. The
  fields take patterns of any length. The panel is a column of sections
  (fields, mappings), so a live preview can be added as a further section.
- **Reordering**: ↑/↓, Ctrl+Shift+Up/Down in the list (`RuleListView`
  emits `moveUpRequested()` / `moveDownRequested()`; the keypad modifier,
  which macOS sets on every arrow key, is ignored) and drag & drop all
  move whole rows. A drag carries the dragged row numbers and a random
  token of the model they come from (`mimeData()`), so no other list takes
  them. `dropMimeData()` accepts only `Qt::MoveAction` and a row between
  rules (none means the end), and moves the rows with `moveRows()`, so a
  rule's mappings, enabled state and problems move with it, and the
  current index, and so the panel, follows it. `RuleListView::startDrag()`
  replaces `QAbstractItemView`'s, which would remove the selected rows
  after a drag ending in a move, and those are then the moved rule itself.
  Before a drop moves anything, the model emits `aboutToDropRules()`, on
  which the editor commits the panel's pending edit. Tests
  (`tests/ruledragdrop_test.cpp`) simulate drags with `mimeData()` and
  `dropMimeData()`, and send drag and drop events to the view's viewport.
  Offscreen, `QDrag::exec()` returns at once, so those events have no
  source and the `InternalMove` view ignores them; the tests switch the
  view to `DragDrop` to get past that check to the same drop handling.
- **The rule check** (`RuleCheck`, `rulecheck.h`) is the one place that
  decides whether a rule is usable and, if not, why; the editor only shows
  what it returns, and the scanner skips rules by the same predicate, so
  they never disagree. It takes the rules and each row's unfinished simple
  rule, and returns each row's problems per field (`RuleProblems`), the
  "Rule N: …" listing, whether the rules can be saved, the keys in use
  (of the rules the scanner uses, for the template dialog) and, per rule,
  the first problem that keeps it from matching anything, which the
  preview shows instead of a result (`RuleProblems::preventingMatches()`).
  An invalid pattern, also of a disabled rule, an enabled rule with a line
  pattern but no key, and an unfinished simple rule's end character are
  problems, always listed in that order: key, line pattern, value pattern,
  end character. The editor's `RuleCheck` is where `RuleListModel` keeps
  its rows, the only copy of the rules and their problems: an edit
  re-checks only the edited rule (`setRule()`), new rules are checked when
  they are added, and removing or moving rules checks nothing and only
  renumbers the listing. It owns the cache of pattern errors, so each
  pattern is compiled once per dialog; tests inject a counting compiler
  (`tests/rulecheck_test.cpp`, table-driven, without widgets). Problems are
  marked at the field with the reason, on the rule's row in the list
  (`problemColor()`), and below the list with the rule number; OK and
  Apply stay disabled while there are any.
- **Live preview**: the panel's last section, a `RulePreviewView`, shows
  `FooterScanner::preview()` of the selected rule against the active file,
  which the plugin passes in with `FooterEditor::setActiveFile()` when the
  editor opens and whenever the host switches files, along with the
  footer's line limit (`setMaxLines()`). `preview()` uses the scanner's
  own compiling, matching (`valueOf()`) and line reading: `forEachLine()`
  reads, counts and limits lines for `scanFrom()` and `preview()` alike,
  so the preview cannot disagree with the footer. Only the first match
  is matched again with `matchOf()`, for its offsets, which the footer's
  scan never computes; a rule that is also one of its key's rules is
  matched once per line. It
  reads on after the first match to the limits, to count the matching
  lines, and reports the first match's line number (from 1), its line
  (at most 64 KiB) with the spans of the line pattern's match and of the
  value, which limit stopped the scan before the end of the file, and
  which enabled rule of the key supplies the key's value, and from which
  line. A disabled rule is previewed too, but never supplies the key.
  Without an active file, an invalid pattern or no line pattern, it
  returns a status that the view explains, an invalid pattern in the rule
  check's words; without a file this is decided on the GUI thread, as
  nothing is read. In the editor, a rule with a problem that keeps it
  from matching, such as an invalid pattern or an unfinished simple rule,
  shows the rule check's problem and is not scanned; a rule only missing
  its key is previewed. Each change schedules one preview
  (`FooterEditor::schedulePreview()`): an edit where it is stored, a
  rule enabled in the list, and rows inserted, removed or moved; not
  every `dataChanged()`, which validation marks emit too. The long line
  around the first match is cut between whole characters, never inside a
  surrogate pair. Tests set the pause after edits to 0
  (`setEditDelay()`) or start a preview with `flush()`, pass the
  follower a clock and change source of their own through the
  constructor, so they wait for no real time, and replace the preview
  function (`setPreviewFunction()`) to hold a preview on the worker
  (`tests/rulepreview_test.cpp`).

### Simple mode

`simplerule.h` holds free functions, independent of the editor, so rule
templates can build simple rules too:

- **`simpleLinePattern(SimpleRule)`** generates the line pattern:
  the text before the value, `\s*`, and the value as capture group 1 —
  `(\S+)` up to whitespace, `(.*\S)` up to the end of the line, or
  `([^c]*[^c\s])` up to the character `c`, so the value never starts or
  ends with whitespace. Escaping is minimal, so generated patterns read like
  hand-written ones: the text escapes only `\ ^ $ . | ? * + ( ) [ {`
  (`]` and `}` are literal outside a class once `[` and `{` are escaped,
  and patterns never use extended mode, so spaces and `#` stay literal),
  and writes NUL as `\x{0}`; `c` escapes only `\ ] ^ -`. `c` is one code
  point, kept in a `QString` because it may be a surrogate pair; the field
  takes one code point, not one UTF-16 unit. A simple rule has no value
  pattern, so the scanner's single-stage path applies. `\s` and `\S` are
  ASCII-only, as the scanner compiles without
  `UseUnicodePropertiesOption`; changing that would change every
  hand-written rule, so a no-break space is part of a value, and a test
  pins it. `applySimpleRule()` sets both patterns of a `FooterEntry`.
- **`endCharacterProblem()`** says why the end character cannot be used:
  none, more than one code point, or a control character such as NUL or a
  tab. Then, as without a text, the pattern is empty.
- **`simpleRuleOf()`** classifies: it parses the text and the end back out
  of the line pattern, generates the patterns again, and accepts the rule
  only if they are the stored ones byte for byte. Empty patterns are the
  empty simple rule. Everything else is advanced, so nothing is ever
  rewritten into another form.

`RuleDetailPanel` shows a rule in simple mode exactly when `simpleRuleOf()`
accepts it, or when it is an unfinished simple rule; the mode is not
stored anywhere. A simple rule with an `endCharacterProblem()` has no
patterns to keep its fields in: its `entry()` has empty patterns, and the
panel hands the fields to the editor as `unfinishedSimpleRule()`. That is
editor state, not part of the domain type `FooterEntry`: `RuleListModel`
keeps it per row, next to the rule's problems, so it moves with the rule
(also by drag & drop) and never reaches `entries()`, a save or an export.
The rule check turns it into a problem at the end character field that blocks
OK and Apply, and the editor passes it to `showEntry()` again, so switching rules loses
nothing. In simple mode the simple
fields regenerate the patterns on every edit and the pattern fields are
read-only; `entry()` always reads the pattern fields. **Advanced** makes them
editable and keeps their text. Switching back is enabled only while
`simpleRuleOf()` accepts the current patterns, and keeps the simple fields
if they still generate those patterns. Escape never reverts a read-only
pattern field, and a pattern field that becomes editable reverts to the
pattern it became editable with. The value-end list and the Advanced switch
handle Return and Escape like the line edits: Return confirms, Escape goes
back to the state at focus or at the last Return, and neither closes the
dialog. Enabling or disabling the rule in
the list updates only the panel's check box (`showEnabled()`), so the mode
chosen for the selected rule is kept. Tests: `tests/simplerule_test.cpp`,
`tests/simplemode_test.cpp`, and the simple rules in
`tests/configroundtrip_test.cpp`.

The editor never changes rules it only shows: a config saved by 0.3.0 is
saved back byte for byte (`tests/configroundtrip_test.cpp`).

### Regex Lab

**Test…** (`testLinePatternButton`, `testValuePatternButton`) sits next
to each pattern field of `RuleDetailPanel`. The panel only emits
`patternTestRequested( PatternField )` and takes an applied pattern back
with `setPattern( field, pattern )`, which sets the field's text as typing
does, so `edited()` follows and the editor stores the rule, re-checks it
and schedules the preview. `setPatternTestUnavailable( reason )` disables
both buttons with the reason as tooltip; otherwise the panel enables them
by mode (below).

`FooterEditor` opens the Lab through the `OpenRegexLab` function that
`setRegexLab()` gave it (`HostFunctions::openRegexLab`, see
[Host functions](#host-functions)); empty, as by default and on LogSquirl
26.10, Test… is disabled with "Needs LogSquirl 26.11 or later". The
editor is the Lab's `context`, so the host never answers once the editor
is destroyed: closing the plugin or the session with a Lab open needs
nothing else. Decisions:

- **Which rule gets the answer.** `testPattern()` keeps a
  `QPersistentModelIndex` of the rule's row. It follows the row through
  `moveRows()` (↑/↓, drag & drop) and becomes invalid when the row is
  removed. An Apply for the selected rule goes through
  `panel_->setPattern()`; for another rule, the editor sets the pattern in
  the model with `setEntry()` (no longer an unfinished simple rule), then
  shows the problems and schedules a preview, as an edit does. An answer
  for a removed rule, a Cancel, or one after `done()` (`closing_`: the
  rules are saved or dropped already) changes nothing, and so does an
  Apply of exactly the pattern and Match case the Lab was opened with, so
  edits made meanwhile (e.g. to a simple rule's fields) are kept. Edits to
  the pattern while the Lab is open are overwritten by an Apply of a
  changed pattern, the user's last word.
- **One Lab at a time.** From opening until the answer (or until
  `openRegexLab` returns false) both buttons are disabled with "The Regex
  Lab is open: apply or cancel it there first". The host always answers
  an open Lab once (Cancel when its window closes), so they come back; a
  second Lab with a second pending target is never needed.
- **Match case.** `FooterEntry` has no case flag, and the scanner compiles
  patterns as they are (case-sensitive). So the Lab gets `matchesCase =
  true`, except for a pattern starting with `(?i)` followed by more,
  which the Lab gets without the `(?i)` and with `matchesCase = false`.
  An applied pattern with Match case off gets `(?i)` at its start, unless
  it already has it there or nothing would follow it. "Start" means after
  any leading start-of-pattern items such as `(*CRLF)` or `(*UTF)`, which
  PCRE accepts only before everything else (`(?i)(*CRLF)x` is invalid).
  Only that exact leading `(?i)` maps to Match case: `(?is)`, `(?i:…)` or a
  `(?i)` further in stay in the pattern, with Match case on, and still
  ignore case there. A pattern that is just `(?i)` is passed on as it is.
  The mapping round-trips every pattern a user is likely to write, except
  that a doubled leading `(?i)(?i)` comes back as one; configs need no new
  field.
- **Simple mode.** Test… of the line pattern is enabled and opens the
  generated pattern: the Lab is a good way to see what a simple rule
  matches. `setPattern()` does nothing for an unchanged pattern, so the
  rule stays simple; a changed one switches the panel to advanced first,
  as checking Advanced and typing would, since simple mode's patterns are
  read-only and only advanced mode edits them. The value pattern's Test…
  is disabled in simple mode, which has no value pattern ("check Advanced
  to write one").

Tests: `tests/regexlab_test.cpp` drives the editor with a fake
`OpenRegexLab` that keeps `done` and, like the host, never answers once
the context is gone (Apply, Cancel, validation and preview following,
Match case, simple mode, unavailable or failing Lab, the rule selected
away, moved, removed or edited, the editor closed or destroyed);
`tests/footersession_test.cpp` covers the session's wiring, a throwing
host function, and the session destroyed with a Lab open;
`tests/pluginlifecycle_test.cpp` goes through `init_ex()` and the
FakeHost's `open_regex_lab` (flags, Apply, Cancel, an answer after
shutdown) and checks the disabled buttons after `init()`.

### Rule from a selected line

`rulefromline.h` holds two free functions, independent of the editor:

- **`ruleFromLine( line, start, length )`** makes the `SimpleRule` that
  finds the value marked in a sample line, with `suggestedKey()` of its
  text and the value itself, or says why it makes none (`MarkProblem`).
  The mark loses the whitespace around it; a length of 0 is a click and
  marks the whole word it lands in (not the rest of it from the click), or
  the next word when it lands in whitespace. A
  start on a combining mark (BMP) or on the second half of a surrogate pair
  moves back to its character, an end inside one moves forward.
  Whitespace is ASCII whitespace, as `\s` in the patterns. The text before
  the value is only the word directly before it, plus the word before that
  when it has no letter or digit (`id =`), and at most its last
  `kMaxTextBefore` (64) units. It never reaches back over more words: that
  would make rules out of variable data (`08:15 a: 1 a: 2 a: 3` marking `3`
  would give `2 a:`). Candidate value ends are whitespace, the code point
  right after the value (if `endCharacterProblem()` accepts it), and the
  end of the line. Each candidate is **verified** as the scanner would run
  it: compiled the same way, `isValid()` checked, and matched once against
  the part of the line the scanner reads (its first
  `FooterScanner::kMaxLineBytes` in UTF-8); its first match must capture
  exactly the marked value at its position. If a match starts before the
  text's position, the text occurs earlier and the rule would find an
  earlier value: `TextBeforeEarlier`, which the picker explains. Other
  outcomes: `NothingMarked`, `NoTextBefore` (value at the start of the
  line), `BeyondLineLimit` (the value ends past what the scanner reads),
  `NoSimpleRule`. The work is at most three matches of a pattern of at most
  64 literal units in at most 64 KiB, so it is bounded and runs on the UI
  thread on every click and mark; a test pins 1 MiB lines (the host's
  maximum) to under two seconds (about a millisecond here), without
  warnings. Escaping is
  `simpleLinePattern()`'s, so special characters, tabs and Unicode before
  the value are matched literally.
- **`suggestedKey( textBefore )`** is the text's longest word, the last of
  equally long ones, where a word is letters and digits of any script and
  `_`, with `.` and `-` inside it: `VIN:` → `VIN`, `[sensor.temp](°C):` →
  `sensor.temp`. It walks the UTF-16 text by code point and slices it, so
  letters outside the BMP count. Empty without a letter or digit; the rule
  check then marks the missing key once a pattern exists.

In the editor, `addRuleFromLine( line, selectedLines )` appends an empty
`FooterEntry` (no pattern yet, so it blocks nothing), selects it, and shows
the line in a `SelectedLinePicker` above the detail panel: a read-only,
fixed-font `QLineEdit` (no 32767-character limit) and a hint. Of a long
line it shows only the part the footer reads, `footerReadLength()` (the
first 64 KiB in UTF-8, cut at a character boundary), with a note that the
rest is cut off: the rest can make no rule, and a `QLineEdit` lays out and
paints its whole text on every click, which took about half a second per
click with the host's 1 MiB. The shown text is the start of the line, so
positions in it are positions in the line. The picker emits `marked( start, length )` on every
click (`cursorPositionChanged`) and mark (`selectionChanged`), each mark
once. The editor remembers the rule's row as a `QPersistentModelIndex`
(`isLineRule()`), so the line moves with the rule (also by drag & drop)
and is dropped when the rule is removed or the rules are replaced; the
picker is shown only while that row is selected. On a mark the editor runs
`ruleFromLine()`, writes the patterns with `applySimpleRule()` into the row
and shows the rule again, which puts the panel into simple mode and
previews it soon (not after the typing pause). The key is replaced only
while it is empty or still the key suggested last, so a key the user typed
is kept. A mark that makes no rule leaves the rule as it was, and the hint
says why. One line at a time: a second `addRuleFromLine()` makes a new rule
and moves the picker to it; the earlier rule keeps what it got. The line is
editor state only; nothing of it reaches `entries()`, a save or an export,
so the config format is unchanged.

**First match.** A rule takes its key's value from the first matching line
of the file, which need not be the selected one. When the preview of the
rule made from the line (`RulePreviewer::previewed()`) finds its first
match in a line other than the picker's (compared without a trailing `\r`,
and as a prefix, as the preview keeps only the start of a long line), the
picker adds "In the active file, Log Line N matches first: the footer shows
its value …" to its hint (`showEarlierMatch()`). The editor sets or clears
this note with every preview of that rule, and clears it when the rule
cannot be previewed, so it always describes the rule as it is: it stays
beside a mark that makes no rule (the rule is unchanged) and goes once
the rule is changed so that the selected line matches first. When the
user changes the rule's patterns by hand, the hint about the last mark
gives way to how to make the rule from the line again
(`showRuleEdited()`).

**Regex Lab.** While a Regex Lab is open for the editor (`regexLabOpen_`),
marks are not taken: the Lab's answer may write a pattern into this very
rule, and the editor leaves the rule to it, as it disables Test….
`showMarkIgnored()` says so and forgets the mark, so the same mark is taken
once the Lab has answered.

Several selected lines: the rule is made from the first, the one the user
most likely meant at the top of the selection, and the hint says how many
were selected. The host's `truncated` flag does not matter for it.

Tests: `tests/rulefromline_test.cpp` (the functions, through the scanner,
including the problems, combining marks and surrogate pairs, and 1 MiB
lines under a message handler that counts warnings),
`tests/rulefromselectedline_test.cpp` (editor and session with fake
`HostFunctions`: marking, clicking, key suggestion, moving and removing
the rule, several lines, an earlier Log Line matching first, the Regex Lab
open, no selection, no Log File, a host without the function, destruction
with the message open), and the 26.11 FakeHost scenarios in
`tests/pluginlifecycle_test.cpp` (the Custom Footer submenu only on a
26.11 table, the selection read on click, saving the rule).

### Rule templates

`ruletemplate.h` is a data table, `ruleTemplates()`: each `RuleTemplate`
has a name, a one-line description, a key, either a `SimpleRule` or an
advanced `linePattern` with the value as group 1, and its group.
`makeTemplates()` builds the table from one function per group
(`generalTemplates()`, `networkTemplates()`, …), each returning its
templates in order; `appendGroup()` sets their `group`, so the rows of a
group's function leave it out. The order of the `appendGroup()` calls is
the order of the groups in the dialog, and the templates of a group are
next to each other in `ruleTemplates()`. `entry()` builds the
rule: a simple template goes through `applySimpleRule()`, so its patterns
are exactly what simple mode generates and it opens in simple mode; an
advanced one keeps its pattern and opens in advanced mode. A template
with an empty key asks for it (`asksForKey()`): `%1` in its pattern is the
`givenKey()` (trimmed, one trailing `=` dropped) escaped with
`QRegularExpression::escape()`, and without a key `entry()` is empty. Only
values with a shape or boundaries a simple rule cannot express are
advanced: Version, Log start (ISO 8601 timestamps), MAC and IPv6 address
(text before, then the shape: one MAC separator throughout via the
backreference `\2`, IPv6 as RFC 3986's `IPv6address` built from `h16` and
`ls32` pieces), and `key=value`, whose key
must not end a longer key (`(?<![\w.-])`, before up to two dashes so
flags like `--user=` match but `my-user=` does not) and whose value starts
right after `=`. The patterns guard their edges with lookarounds rather than
anchors or `\b`, as a value may be anywhere in the line and `_` counts as
a word character; `\d` and `\w` are ASCII, like everything the scanner
compiles. Version takes `version` after `_` or as a camel-case `Version`,
but not after another letter, trading a missed `appversion` for no match
in `conversion` or `server`, and skips an XML declaration's version
(`(?<!<\?xml\s)`); its SemVer suffix needs a letter in its first
identifier, and a number shaped like `\d{4}-\d{2}` is not a version, so
dates are never taken. The timestamp's seconds, fraction and offset are an
atomic group followed by `(?!\d|:\d)`, so a truncated field (`10:30:5`,
`+01:0`) fails instead of backtracking to a shorter match, while a `:`
after a complete timestamp, as in `10:30:00: started`, is fine. Each template is tested
with lines it must find and lines it must not, through `FooterScanner`
(`tests/ruletemplate_test.cpp`). A new template is a row in its group's
function plus such samples; a new group is a function returning its
templates and one `appendGroup()` line in `makeTemplates()`.

Every template follows one rule: **a template finds a value that holds
for the whole file.** The footer shows one value per key, taken from the
first line that matches, so a template is for a value such as a version,
a serial number or a device name, not for one that changes from line to
line. A template that matches by shape alone, without text before the
value, is only for values unique by nature (a VIN, a build fingerprint);
anything else needs text before the value, so that the first match is
the value meant and not some other value of the same shape.

The VIN template is such a shape: `kVin` in `ruletemplate.cpp` holds it
without a capturing group (17 characters of `[A-HJ-NPR-Z0-9]`, lookaheads
for at least one letter and one digit, no letter or digit on either
side), and the VIN template captures all of it. A template that takes a
part of a VIN puts `kVin` in a lookahead and captures the part, so it
finds exactly the VINs the VIN template finds. Its tests are in
`tests/ruletemplate_automotive_test.cpp`.

`RuleTemplateDialog` lists the templates in a `QTreeWidget`: a top-level
item per group, `Qt::ItemIsAutoTristate`, with a checkable child per
template showing name and description and holding its index in
`ruleTemplates()` as `Qt::UserRole`. Checking a group checks its
templates, and some of them checked shows it partly checked. A
double-click on a template toggles it; Space does so through the
delegate, and Enter reaches the default button. The key field is enabled
while a checked template asks for a key; `entries()` gives one rule per
checked template in list order, and OK is enabled once there is at least
one and each has a key. A hint says a typed trailing `=` is dropped. The
dialog gets the keys of the rules the scanner uses,
`RuleCheck::keysInUse()`, in `reset()`, which also unchecks everything,
and its note names every key of a checked template that is one of them:
those rules are still added, as alternatives.
`FooterEditor::addFromTemplate()` commits the panel's pending edit, keeps
one dialog, calls `reset()` and `open()` (not `exec()`, so tests click
through it), and on `accepted` appends `entries()` after the existing
rules and selects the first new one, like the add button, without
touching other rows.

## Configuration Storage

Rules are stored in `custom_footer.ini` (QSettings INI format) inside the
plugin's config directory provided by the host.

The JSON import/export uses a versioned format (`version: 1`) to allow
forward-compatible changes.  See the README for format examples.

## Adding a New Feature

1. Extend `FooterEntry` in `footerentry.h` if new per-rule data is needed.
2. Update `FooterScanner` (compiling and `scanFrom()`) to use the new data.
3. Update `FooterConfig` to persist/load the new field (both INI and JSON).
4. Update `FooterEditor` to expose the field in the UI.
5. Add test scenarios in `tests/`.
   The suite runs on Qt's offscreen platform: `tests/tests_main.cpp` sets
   `QT_QPA_PLATFORM=offscreen` unless it is already set, so widget tests
   (tooltips, `QFileDialog`) behave the same on every desktop and in CI.
