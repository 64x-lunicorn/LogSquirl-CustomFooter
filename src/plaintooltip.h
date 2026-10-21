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

/// A tooltip showing @p text as it is, e.g. a rule's pattern or a file's
/// path. Qt shows a tooltip as rich text as soon as its text looks like
/// HTML, and has no plain-text tooltip: so it is always rich text, the text
/// escaped, its spaces and line breaks kept. Empty for empty text, which
/// shows no tooltip.
inline QString plainToolTip( const QString& text )
{
    if ( text.isEmpty() ) {
        return {};
    }
    return QStringLiteral( "<p style=\"white-space: pre-wrap;\">%1</p>" )
        .arg( text.toHtmlEscaped() );
}

} // namespace custom_footer
