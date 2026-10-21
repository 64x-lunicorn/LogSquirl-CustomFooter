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

#include <QChar>
#include <QString>

namespace custom_footer {

/// How many UTF-16 units of a value the footer and the editor's hints show
/// at most (#68, #71).
inline constexpr int kMaxShownValueLength = 100;

/// @p text as shown: cut at kMaxShownValueLength, before a character rather
/// than within it, with an ellipsis, if it is longer.
inline QString elidedText( const QString& text )
{
    if ( text.size() <= kMaxShownValueLength ) {
        return text;
    }
    qsizetype length = kMaxShownValueLength;
    if ( text.at( length - 1 ).isHighSurrogate() && text.at( length ).isLowSurrogate() ) {
        --length;
    }
    return text.left( length ) + QChar( 0x2026 );
}

} // namespace custom_footer
