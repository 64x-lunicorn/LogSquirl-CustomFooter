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

#include <QList>
#include <QString>

namespace custom_footer {

/// Maps a raw extracted value to a human-readable display string.
struct ValueMapping {
    QString pattern;      ///< Raw value to match (exact string comparison).
    QString displayValue; ///< What to show instead.
};

/// A single key-value extraction rule.
struct FooterEntry {
    QString key;            ///< Display label, e.g. "Component Protection".
    QString linePattern;    ///< Regex to find the target line.
    QString valuePattern;   ///< Optional regex to extract value from found line.
    bool enabled = true;    ///< Whether this entry is active.
    QList<ValueMapping> mappings; ///< Value substitution rules.
};

} // namespace custom_footer
