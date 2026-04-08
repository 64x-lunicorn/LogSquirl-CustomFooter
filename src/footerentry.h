/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QString>

namespace costume_footer {

/// A single key-value extraction rule.
struct FooterEntry {
    QString key;          ///< Display label, e.g. "VIN".
    QString regexPattern; ///< Regular expression to search for.
    bool enabled = true;  ///< Whether this entry is active.
};

/// Where matched key-values are displayed.
enum class DisplayMode {
    Footer  = 0, ///< Status bar only.
    Sidebar = 1, ///< Sidebar tab only.
    Both    = 2  ///< Status bar and sidebar.
};

} // namespace costume_footer
