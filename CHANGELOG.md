# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **UTF-16 log files with a byte order mark** — a file starting with a
  UTF-16LE (`FF FE`) or UTF-16BE (`FE FF`) byte order mark is read as
  UTF-16, as LogSquirl reads it: rules find their values, and the Log Line
  of each value is LogSquirl's, also for lines with characters such as
  "Ċ" (U+010A) and with Windows line ends, as the file grows, and in the
  editor's live preview. A rule from a selected Log Line of such a file
  takes its value only from the part of a long line the footer reads, the
  first 64 KiB (32 768 UTF-16 code units), and shows only that part.
  Before, such files were read as UTF-8, so rules
  practically never matched and line numbers could differ. UTF-8 and
  single-byte files are read as before. UTF-16 files without a byte order
  mark, and encodings chosen in LogSquirl, are not covered yet.
- **Test a pattern in the Regex Lab** — **Test…** next to a rule's line
  pattern and value pattern opens it in LogSquirl's Regex Lab. Apply in
  the Lab writes the pattern into that rule as if typed, so validation and
  the live preview follow; Cancel or closing the Lab changes nothing. The
  Lab's Match case is on unless the pattern starts with `(?i)`, and
  applying with Match case off puts `(?i)` in front. In simple mode the
  generated line pattern can be tested; applying a changed one switches
  the rule to Advanced. The pattern goes to the rule the Lab was opened
  for, even if another rule is selected or the rule moved meanwhile, and
  is dropped if that rule was removed or the editor closed. One Lab is
  open at a time. Needs LogSquirl 26.11; on 26.10 the buttons are disabled
  with the tooltip "Needs LogSquirl 26.11 or later".
- **Go to the Log Line of a value** — double-click a value in the footer,
  or choose "Go to Log Line" in its context menu (also reached with the
  context menu key), and LogSquirl selects the line the value was found in
  and scrolls to it in the tab in front. For a key that several rules
  share, it is the line of the rule that supplied the shown value. The
  line stays right as a growing log is scanned on, and comes from the new
  scan after a truncation or rotation. If the line is no longer there, a
  short "Could not go to Log Line …" tooltip says so. A value's tooltip
  names its Log Line. A double-click copies the value too, as its first
  click does. Needs LogSquirl 26.11; on 26.10 the values only copy, as
  before.
- **Rule from a selected line** — on LogSquirl 26.11, select the Log Line
  with the value and choose *Plugins → Custom Footer → Rule from Selected
  Line…*, or **From selected line…** in the rule editor. The editor opens
  with a new rule in simple mode and shows the line: click where the value
  starts or mark it, and the text before the value, where the value ends
  and a key (`VIN:` → `VIN`) are filled in, with the extracted value in
  the preview at once. Special characters before the value are matched
  literally. A mark that makes no rule, e.g. because the text before the
  value also appears earlier in the line, says why, and the editor says
  when an earlier Log Line in the file supplies the value. Of several
  selected lines the first is used. Without a selection or an open Log
  File, a message says what to do. On LogSquirl 26.11 the editor moves to
  *Plugins → Custom Footer → Edit Rules…*; on LogSquirl 26.10 the action is
  not offered, and the menu stays *Plugins → Custom Footer…*.
- **Environment and User templates** — two more templates in simple mode:
  Environment, the word after `env:` (`env: prod`, `env: staging`), and
  User, the word after `user:`. Adjust the text to your log.
- **Network template group** — next to IPv4 address, two templates in
  advanced mode: MAC address, the address after `MAC:` as
  `52:54:00:12:34:56`, `00-1A-2B-3C-4D-5E` or `0050.56a1.b2c3`, with the
  same separator throughout, and IPv6 address, the address after `IPv6:`,
  in full, shortened with `::` or with an IPv4 address as its last part
  (`::ffff:192.0.2.10`). Their pattern starts with the text, which you
  adjust to your log (`link/ether`, `inet6`).
- **Automotive template group** — VIN, found by its shape alone, with or
  without text before it: 17 letters and digits without `I`, `O` and `Q`,
  with both letters and digits, not part of a longer word (no check digit
  test, which only North American VINs have). IMEI and ICCID, the word
  after `IMEI:` and `ICCID:` in simple mode; adjust the text to your log.
- **Templates with value mappings** — a template can bring value
  mappings, which the new rule has like any others: shown in the editor,
  editable and saved with it. Two Automotive templates use them, both
  finding their value in exactly the VINs the VIN template finds:
  Manufacturer, the first three characters (the WMI) mapped to the
  manufacturer for common WMIs of major brands (`WVW` → Volkswagen, `WBA`
  → BMW), an unmapped WMI shown as it is; and Model year, the 10th
  character mapped to the year in the cycle 2010 to 2039.
- **Android template group** — for logcat output and bug reports: Build
  fingerprint, found by its shape
  (`brand/product/device:version/id/incremental:type/tags`), and from
  getprop's `[key]: [value]` lines Device model (`ro.product.model`),
  Android version (`ro.build.version.release`) and Device serial number
  (`ro.serialno`, with the key `Serial number`).
- **Linux and boot template group** — templates for dmesg, boot and
  syslog output: Kernel version, the word after `Linux version`; Hardware
  (DMI), vendor and product before `, BIOS` in the `DMI:` line; Kernel
  command line, the rest of the line after `Command line:`; U-Boot
  version, from the U-Boot banner; and Syslog host, the host after the
  time of an RFC 3164 syslog line, which fits the log of one host, not
  merged ones.
- **iOS crash log template group** — four templates for the header of an
  Apple crash report: Hardware model (`Hardware Model:`), OS version
  (`OS Version:`, up to the end of the line, as in `iPhone OS 17.2
  (21C62)`) and Incident identifier (`Incident Identifier:`) in simple
  mode, and App version (`Version:`, up to the end of the line, as in
  `1.0 (1)`) in advanced mode, which does not take `OS Version:` or
  another `… Version:` when the app's version is empty.
- **Java and Spring Boot template group** — from the Spring Boot start
  line (`Starting MyApp v1.2.0 using Java 17.0.8 …`), in the wording of
  Spring Boot 2 and 3: Application, Application version, Java version
  (simple mode, after `using Java`) and Host (Spring Boot 2 only, as
  Spring Boot 3 no longer logs it). Active profile, from `The following …
  active:`, one profile without its quotes. JRE version, the runtime
  version in `(build …)` of the `# JRE version:` line of a JVM crash log.
- **Database template group** — the server version from the line a
  database server logs when it starts, all in simple mode: PostgreSQL
  version (`starting PostgreSQL 16.1 on …`), MySQL version, the quoted
  version of `Version: '8.0.36'` (MariaDB's too), and Redis version
  (`Redis version=7.2.4,`). With several starts in a file, the first
  counts.
- **CI template group** — for build logs: from GitHub Actions, Runner
  image (`Image: ubuntu-24.04`) and Runner image version
  (`Version: 20261004.327.1`, not the Runner Image Provisioner's version);
  Commit, from `HEAD is now at <sha>`, the first checkout counting;
  GitLab Runner version, from `Running with gitlab-runner <version>`; and
  from Jenkins, Started by, the user's name from `Started by user <name>`.
- **Test run template group** — for pytest output, meant for one run per
  file: Python version and pytest version from the header
  (`platform linux -- Python 3.12.3, pytest-8.3.2, …`), in simple mode,
  and Test result, the summary at the end (`3 failed, 120 passed in
  12.30s`) without the `=` around it, in advanced mode.

### Changed
- **Template groups, and several templates at once** — **From template…**
  lists the templates under their group: Version, Build number, Serial
  number, Environment, User, Log start and `key=value` under *General*,
  IPv4 address under *Network*. Templates are checked instead of picked one at
  a time; checking a group checks all its templates. **Add Rules** adds
  one new rule per checked template, after the existing rules and in the
  order the dialog lists them, and selects the first new one; existing
  rules are not changed. While `key=value` is checked, the dialog asks for
  its key and **Add Rules** stays disabled until one is given. The note
  about a key already in use names every checked template's key that an
  enabled rule with a line pattern has. The developer guide states the
  rule every template follows: it finds a value that holds for the whole
  file.
- **IPv4 address template** — finds the address after `IP:` in simple
  mode instead of the first address in a line, which in a log with clients
  and servers is an arbitrary one. Adjust the text so it says which
  address is meant. The key stays `IP address`.
- **Timestamp template renamed Log start** — with the key `Log start`, and
  a description saying what it shows: as the first match counts, the first
  date and time in the file, which is when the log starts. It matches what
  it matched before.
- Rules made from these templates earlier do not change: a template only
  makes new rules.
- **Footer session** — one module now owns the rules in the config
  directory, the active file, the footer's controller and the rule
  editor's lifetime, and its destructor defines the shutdown order: the
  editor with its dialogs and preview first, then the footer's scan. The
  plugin's C entry points only translate between LogSquirl and it. Nothing
  changes for users.
- **File follower** — the rule editor's live preview follows the active
  file through a shared file follower, which the footer uses as well.
  It behaves as before: it follows a growing, rotated or truncated file,
  leaves half a second between two previews of a changing file, and
  ignores notifications that are only the file being read.
- **The footer follows the active file like the preview** — through the
  same file follower. A file that changes while it is scanned is scanned
  again half a second after that scan, as the preview does, instead of
  right after it, so a busy log is never scanned back to back. The same
  file made active again is scanned after the same pause. A change that
  cannot alter the footer, such as a file left as it was, is not scanned.
  A growing log is still scanned on from where the last scan stopped, and
  not at all once every key has a value; a truncated or rotated file is
  still scanned from its start.
- **One rule check** decides whether a rule is usable, for the footer and
  the rule editor alike. The footer skips exactly the rules the editor
  marks, and reports them in the LogSquirl log in the editor's words, e.g.
  `Rule 2: a rule with a line pattern needs a key` instead of
  `Rule 2 (): no key`. Problems are listed in one order everywhere: key,
  line pattern, value pattern, end character. The preview of a rule with
  an invalid pattern says so in the same words, and the template dialog's
  note about an alternative no longer counts rules the footer skips.
- **Host functions of LogSquirl 26.11** — the plugin is built against the
  plugin API of LogSquirl 26.11 and learns whether the running LogSquirl
  offers the Regex Lab, Go to line and the selected Log Lines, which
  upcoming features need. It still runs on LogSquirl 26.10, without them.
  The LogSquirl log says which ones it found (`Custom Footer plugin ready.
  Host functions: …`); nothing else changes for users.

### Fixed
- **The live preview kept an old value** — once the rule editor's preview
  had skipped a change of the active file that could not alter it (e.g.
  lines appended beyond the scan limit), it stopped following the file:
  a later truncation or rotation was not previewed until the rule was
  edited or another file opened. The preview now keeps following the file.
- **The live preview missed a last line rewritten in place** — when the
  active file ends in a line without a line break and that line was
  rewritten at the same size, the preview kept showing the old line until
  the file changed again. It is now previewed again.
- **The live preview missed lines appended while it read the file** — a
  line appended to the active file just as the preview finished reading
  could be skipped as a change that cannot alter the preview, until the
  file changed again. It is now previewed.

## [0.4.0] — 2026-10-01

### Added
- **Copy a value with a click** — clicking a value in the footer copies
  exactly that value, without its key, to the clipboard, and a short
  "Copied" tooltip confirms it. Hovering a value shows its key, the rule
  that supplied it and, for a mapped value, the raw value before mapping.
  The values take the keyboard focus with Tab, copy with Space, Return or
  the copy shortcut, and carry an accessible name with their key and value
  for screen readers. The context menu of a value offers "Copy Value",
  "Copy Key and Value", and "Copy All" for every shown key and value, one
  per line.
- **Reorder rules by drag & drop** — drag a rule to a new place in the
  rule list; it takes its mappings, enabled state and validation marks
  along and stays selected in the panel. Ctrl+Shift+Up / Ctrl+Shift+Down
  (⇧⌘↑ / ⇧⌘↓ on macOS) move the selected rule as well, next to the ↑/↓
  buttons. The rules are saved in the order of the list.
- **Simple mode for rules** — a rule can be described without a regex: the
  text before the value (e.g. `VIN:`, matched literally, with any
  whitespace after it ignored) and where the value ends: at whitespace, at
  the end of the line, or before a given character such as `,` or `;`. The
  line pattern is generated from these and shown read-only. New rules
  start in simple mode; rules whose patterns have exactly the generated
  form open in it, all others in advanced mode. **Advanced** shows the
  line and value patterns for editing and keeps the generated ones;
  switching back is offered while the patterns still have the simple form.
  Simple rules are saved as their patterns, so the config format is
  unchanged and older versions read them. Whitespace means ASCII
  whitespace: a no-break space is part of the value.
- **Rule templates** — **From template…** next to the add button offers
  ready-made rules for common values, each with a name and a one-line
  description: Version (`version 1.2.3`, `Version: v2.0.1-rc1`, `ver=1.2`,
  `v1.2.3`, `appVersion: 2.1.0`, `{"version": "1.2.3"}`), Build number
  (`Build:`), Serial number (`Serial number:`), IPv4 address, Timestamp
  (ISO 8601) and `key=value`, which asks for the key. The chosen template
  is added as a new rule after the others and selected; existing rules are
  not changed. Build number and Serial number open in simple mode, the
  others, whose values need a shape or boundaries, in advanced mode. If an
  enabled rule with a line pattern already has the key, the dialog says
  that the new rule becomes an alternative for it.
- **Live preview in the rule editor** — below the selected rule's fields,
  a preview shows what the rule finds in the active log file, without
  applying it: the first matching line with its number, the line pattern's
  match and the captured value highlighted; the raw value and the value
  after mappings; how many lines match within the scan limits, and when a
  limit was reached; and, for rules sharing a key, whether this rule or
  another one supplies the key's value in this file. It updates shortly
  after typing stops, scans in the background with the footer's limits
  and matching, and follows the active file, its growth and its rotation,
  while the editor is open. Without an active file, with an invalid pattern, or for
  an unfinished simple rule, it says why instead. The editor no longer
  runs in a nested event loop, so the plugin can be shut down safely
  while it is open.

### Changed
- **Requires LogSquirl 26.10.0 or later**, like 0.3.0: this release is built with
  the Qt of LogSquirl 26.10.0 (Qt 6.11.3).
- **Rule editor** — the rules are now shown as a list, each with its
  enabled check box, key and line pattern, and the selected rule in a
  panel next to it with its key, line pattern, value pattern and value
  mappings in one form, replacing the five-column table and the separate
  mapping panel. Edits in the panel show up in the list as you type.
  Invalid patterns and a line pattern without a key are marked at their
  field with the reason, and on the rule's row in the list. Return in a
  field confirms it and Escape reverts it; neither closes the dialog.
  Adding, removing, reordering, importing and exporting rules, and the
  saved config, are unchanged.

## [0.3.0] — 2026-09-30

### Changed
- **Requires LogSquirl 26.10.0 or later.** This release is built with the Qt of
  LogSquirl 26.10.0 (Qt 6.11.3); an older LogSquirl cannot load it.
- **Rules sharing a key** — rules with the same key are now alternatives,
  e.g. one pattern for old logs and one for new ones: the key's value comes
  from the first line that any of them matches, and if several match that
  line, the rule higher in the list wins. The key is shown once, at the
  position of its first rule. Previously the key was shown twice and the
  last rule won.
- **Rule validation in the editor** — invalid patterns, and enabled rules
  with a line pattern but no key, are marked in their cell, with the reason
  in the tooltip and below the table; OK and Apply stay disabled until they
  are fixed. The scanner skips such rules and reports them in the host log;
  rules without a line pattern are ignored.
- **Background scanning** — the active file is now scanned on a worker
  thread instead of blocking the window; a scan still running when another
  file becomes active, or the rules are applied, is cancelled and its values
  are never shown. The rules are
  read once and again only when they are applied, instead of twice on every
  file switch. The footer is cleared as soon as another file becomes active.
- **Follow mode** — the active file is watched and scanned again shortly
  after it changes, so values that appear later in a growing log show up
  without switching files. Only the lines appended since the last scan are
  read, and none once every key has a value; a file that was truncated or
  replaced is scanned from its start. A change while the file is scanned
  does not restart the scan: it finishes, and one more scan then covers
  every change made meanwhile, so a log that grows faster than it can be
  scanned still shows its values. When the file is rotated away, its
  directory is watched until it is recreated.

### Fixed
- **Mappings lost when removing a rule** — removing the selected rule could
  write the mappings of one rule over another's, or drop the edits made
  afterwards. Mappings are now stored with their rule's row.
- **Mapping panel dead after import** — after importing rules the mapping
  panel stayed disabled for the selected rule until another one was selected.
- **Invalid patterns** — a rule whose value pattern does not compile is now
  skipped like one with an invalid line pattern, instead of silently falling
  back to the line pattern and showing the wrong value. Skipped rules are
  reported in the host log.
- **Huge lines** — a file without line breaks was read into memory in one
  piece; lines are now matched against their first 64 KiB, and a scan reads
  at most 64 MiB.
- **Missing config directory** — without a config directory from the host,
  rules were read from and written to `/custom_footer.ini`; they are now
  neither loaded nor saved, and a warning is logged. A failed save is logged
  too.
- **Editor parent** — the rule editor opened through `configure()` ignored
  the parent window the host passed, and the one from the Plugins menu had
  none; both now open over LogSquirl's main window.
- **Exceptions at the C boundary** — an exception in an entry point or host
  callback, e.g. out of memory while scanning, is now logged instead of
  crossing into the host.

## [0.2.0] — 2026-04-08

### Added
- **Two-stage matching** — optional `valuePattern` regex applied to the same
  line after `linePattern` matches, for fine-grained value extraction.
- **Value mapping** — per-rule mapping table that translates raw extracted
  values to human-readable display text (e.g. `false` → `Disabled`).
- **JSON import / export** — share rule sets via JSON files from the editor
  toolbar.
- **Inline mapping editor** — the rule editor now shows a "Value Mappings"
  panel below the rules table for editing mappings per rule.
- **Ordered footer display** — key-value pairs in the footer bar now follow
  the rule definition order, not alphabetical order.
- **Plugin icon** — added `icon` field to `plugin.json`.
- **Decentralized registry** — added `releases.json` with per-platform
  download URLs and SHA-256 checksums.

### Changed
- Renamed `regexPattern` to `linePattern` throughout the data model, config
  persistence, and UI (backward-compatible INI read with `regex` fallback).
- Rule editor table expanded from 3 to 5 columns (Enabled, Key, Line Pattern,
  Value Pattern, Mappings).
- Minimum dialog size increased to 700×500 for the additional columns.

### Fixed
- **Mapping remove button** was always grayed out — now correctly enabled
  when a mapping row is selected.
- **Stale mappings on save** — mapping edits in the inline panel were not
  flushed before OK/Apply; now synced before returning entries.

## [0.1.0] — 2026-04-07

### Added
- Initial release.
- **Regex-based extraction** — define rules with key + regex pattern to
  extract values from log lines.
- **Footer bar display** — matched key-value pairs shown in a compact bar
  at the bottom of the LogSquirl main window.
- **Rule editor dialog** — add, remove, reorder, enable/disable rules with
  OK / Cancel / Apply buttons.
- **INI persistence** — rules saved to `custom_footer.ini` in the plugin
  config directory.
- **Active file callback** — automatically re-scans when the active log
  file changes.
- **Menu action** — "Custom Footer…" entry in the Plugins menu.
