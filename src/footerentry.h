/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-custom-footer.
 *
 * logsquirl-custom-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-custom-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-custom-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "simplerule.h"

#include <QList>
#include <QString>

#include <optional>

namespace custom_footer {

/// Maps a raw extracted value to a human-readable display string.
struct ValueMapping {
    QString pattern;      ///< Raw value to match (exact string comparison).
    QString displayValue; ///< What to show instead.
};

/// A single key-value extraction rule.
struct FooterEntry {
    QString key;                  ///< Display label, e.g. "Component Protection".
    QString linePattern;          ///< Regex to find the target line.
    QString valuePattern;         ///< Optional regex to extract value from found line.
    bool enabled = true;          ///< Whether this entry is active.
    QList<ValueMapping> mappings; ///< Value substitution rules.

    /// Editor only, never saved or scanned: a simple rule whose patterns
    /// cannot be generated, as its end character is missing or unusable.
    /// It keeps the rule's fields, and its problem, while other rules are
    /// edited; the rule's patterns are empty meanwhile.
    std::optional<SimpleRule> unfinishedSimpleRule = std::nullopt;
};

} // namespace custom_footer
