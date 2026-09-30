# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **Duplicate keys** — when several enabled rules share a key, the first rule
  in the list owns it; later ones are ignored and reported in the host log.
  Previously the key was shown twice and the last rule won.
- **Rule validation in the editor** — invalid patterns and keys already used
  by an enabled rule above are marked in their cell, with the reason in the
  tooltip and below the table; OK and Apply stay disabled until they are
  fixed.
- **Background scanning** — the active file is now scanned on a worker
  thread instead of blocking the window; a scan still running when the file
  changes again is cancelled and its values are never shown. The rules are
  read once and again only when they are applied, instead of twice on every
  file switch. The footer is cleared as soon as another file becomes active.
- **Follow mode** — the active file is watched and scanned again shortly
  after it changes, so values that appear later in a growing log show up
  without switching files.

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
