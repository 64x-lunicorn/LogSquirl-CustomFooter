# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
- **INI persistence** — rules saved to `costume_footer.ini` in the plugin
  config directory.
- **Active file callback** — automatically re-scans when the active log
  file changes.
- **Menu action** — "Custom Footer…" entry in the Plugins menu.
