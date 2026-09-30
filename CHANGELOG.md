# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
  unchanged and older versions read them.

### Changed
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
