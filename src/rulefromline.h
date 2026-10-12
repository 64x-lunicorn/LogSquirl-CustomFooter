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

#include <QString>

#include <optional>

namespace custom_footer {

/// A simple rule made from a sample line, with the key suggested for it.
struct RuleFromLine {
    QString key; ///< Suggested from the text before the value; may be empty.
    SimpleRule rule;
    QString value; ///< What the rule finds in the sample line: the marked value.
};

/**
 * The simple rule that finds the value marked in @p line, a Log Line the
 * user picked as a sample.
 *
 * The value is the @p length characters (UTF-16 units) at @p start, without
 * the whitespace around them; a @p length of 0 is a click, which marks the
 * word starting at the first non-whitespace at or after @p start.
 * Whitespace is ASCII whitespace, as in simple rules.
 *
 * The text before the value is the word before it, with the word before
 * that when it is only punctuation (`id =`), and as many words more as the
 * rule needs to find the marked value rather than an earlier one. The value
 * ends at whitespace if it is one word followed by whitespace or the end of
 * the line, else at the character after it, else at the end of the line;
 * the first of these that finds exactly the marked value is taken. The key
 * is suggestedKey() of the text.
 *
 * Nothing when nothing is marked, the mark is outside the line, the value
 * has no text before it, or no simple rule finds exactly the marked value.
 */
std::optional<RuleFromLine> ruleFromLine( const QString& line, qsizetype start, qsizetype length );

/**
 * A key for the value after @p textBefore: its longest word, the last of
 * equally long ones, where a word is letters, digits and `_`, with `.` and
 * `-` inside it. `VIN:` gives `VIN`, `app.version=` gives `app.version`.
 * Empty if the text has no letter or digit.
 */
QString suggestedKey( const QString& textBefore );

} // namespace custom_footer
