# Developer Guide — LogSquirl Custom Footer Plugin

This guide explains how the Custom Footer plugin works, how the
LogSquirl Plugin SDK is used, and how to extend or adapt the code.

## Plugin SDK Overview

The LogSquirl Plugin SDK is a **pure C ABI** interface.  A plugin is a
shared library (`.dylib` / `.so` / `.dll`) that exports four C functions:

| Symbol | Purpose |
|--------|---------|
| `logsquirl_plugin_get_info()` | Return static metadata (called before init) |
| `logsquirl_plugin_init()` | Receive host API, set up the plugin |
| `logsquirl_plugin_shutdown()` | Tear down, release all resources |
| `logsquirl_plugin_configure()` | Open a settings dialog (optional) |

The host provides a function-pointer table (`LogSquirlHostApi`) that the
plugin uses to interact with the application.

## Plugin Lifecycle

```mermaid
sequenceDiagram
    participant Host as LogSquirl Host
    participant Plugin as Custom Footer Plugin

    Host->>Plugin: logsquirl_plugin_get_info()
    Plugin-->>Host: &kPluginInfo (id, name, version, type, …)

    Host->>Plugin: logsquirl_plugin_init(api, handle)
    Plugin->>Plugin: Store api + handle in g_state
    Plugin->>Host: api->register_menu_action("Plugins", "Custom Footer…", callback)
    Plugin->>Host: api->register_footer_widget(footerWidget)
    Plugin->>Host: api->register_active_file_callback(onActiveFileChanged)
    Plugin->>Plugin: FooterController loads and compiles the rules
    Plugin->>Plugin: FooterController::setActiveFile(current file)
    Plugin-->>Host: return 0 (success)

    Note over Host,Plugin: User opens or switches to a log file

    Host->>Plugin: onActiveFileChanged(filePath)
    Plugin->>Plugin: FooterController::setActiveFile(filePath)
    Plugin->>Plugin: FooterScanner::scanFile() on a worker thread
    Plugin->>Plugin: FooterDisplayWidget::updateValues(ordered), if still current

    Note over Host,Plugin: User clicks "Custom Footer…" in Plugins menu

    Host->>Plugin: onEditorMenuAction() callback
    Plugin->>Plugin: Open FooterEditor dialog over the main window
    Plugin->>Plugin: User edits rules, clicks OK
    Plugin->>Plugin: FooterConfig::saveEntries()
    Plugin->>Plugin: FooterController::reloadConfig()

    Note over Host,Plugin: Host unloads the plugin

    Host->>Plugin: logsquirl_plugin_shutdown()
    Plugin->>Plugin: Cancel and wait for a running scan
    Plugin->>Host: api->unregister_footer_widget(footerWidget)
```

## Threading

Scans run on a single worker thread owned by `FooterController`. Every scan
request increments a generation counter; a finished scan is shown only if no
newer one was requested, so switching files quickly never shows stale
values. The result reaches the widget through a `QFutureWatcher` on the GUI
thread. `logsquirl_plugin_shutdown()` deletes the controller first, which
cancels the running scan and waits for the worker: after it returns, no code
of the plugin runs, and the host may unload the library.

No exception may leave an entry point or host callback; they run their work
through `guarded()`, which logs the failure instead.

## Module Overview

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| **Plugin entry** | `plugin.cpp`, `plugin.h` | C ABI exports, global state, lifecycle |
| **FooterEntry** | `footerentry.h` | Data model: key, linePattern, valuePattern, mappings |
| **FooterController** | `footercontroller.h/.cpp` | Cached rules, background scans, file watching |
| **FooterScanner** | `footerscanner.h/.cpp` | Compiles the rules once; line-by-line scanning with two-stage matching |
| **FooterConfig** | `footerconfig.h/.cpp` | INI persistence + JSON import/export |
| **FooterEditor** | `footereditor.h/.cpp` | Rule editor dialog with inline mapping panel |
| **FooterDisplayWidget** | `footerdisplaywidget.h/.cpp` | Footer bar widget showing key-value pairs |

## Scanning Algorithm

1. Build compiled `QRegularExpression` objects for each enabled rule. A rule
   is skipped, and reported in `FooterScanner::problems()`, if a pattern is
   invalid or an earlier enabled rule already uses its key.
2. Read the log file line by line (up to `maxLines`, at most 64 MiB; longer
   lines than 64 KiB are matched against their start).
3. For each line, check every unmatched rule:
   - **Stage 1 — Line Pattern**: If `linePattern` matches the line, proceed.
   - **Stage 2 — Value Pattern** (optional): If `valuePattern` is set, apply
     it to the same line and extract capture group 1.  If not set, extract
     capture group 1 from the line pattern match.
   - **Value Mapping**: If mappings are defined for the rule, substitute the
     raw value with the matching display value (exact string comparison).
4. Early termination when all rules have matched.

## Configuration Storage

Rules are stored in `custom_footer.ini` (QSettings INI format) inside the
plugin's config directory provided by the host.

The JSON import/export uses a versioned format (`version: 1`) to allow
forward-compatible changes.  See the README for format examples.

## Adding a New Feature

1. Extend `FooterEntry` in `footerentry.h` if new per-rule data is needed.
2. Update `FooterScanner` (compiling and `scanFile()`) to use the new data.
3. Update `FooterConfig` to persist/load the new field (both INI and JSON).
4. Update `FooterEditor` to expose the field in the UI.
5. Add test scenarios in `tests/`.
