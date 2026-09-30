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

#include <QString>

namespace custom_footer {

/// A value shown in the footer, and where it came from.
struct FooterValue {
    QString key;      ///< The key it is shown under.
    QString value;    ///< The value as shown, after the mappings.
    QString rawValue; ///< The value as found in the line, before the mappings.
    int rule = -1;    ///< Index of the rule that supplied it, in the list of rules.

    /// Whether a mapping changed the value.
    bool isMapped() const
    {
        return value != rawValue;
    }

    friend bool operator==( const FooterValue& a, const FooterValue& b )
    {
        return a.key == b.key && a.value == b.value && a.rawValue == b.rawValue && a.rule == b.rule;
    }
    friend bool operator!=( const FooterValue& a, const FooterValue& b )
    {
        return !( a == b );
    }
};

} // namespace custom_footer
