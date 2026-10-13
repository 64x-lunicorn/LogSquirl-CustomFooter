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

#include "footerscanner.h"
#include "simplerule.h"

#include <QString>

namespace custom_footer {

/// Why a mark in a sample line makes no rule.
enum class MarkProblem {
    None,              ///< It makes a rule.
    NothingMarked,     ///< Only whitespace, nothing, or not inside the line.
    NoTextBefore,      ///< The value starts the line: nothing comes before it.
    BeyondLineLimit,   ///< The value ends past the part of a long line the footer reads.
    TextBeforeEarlier, ///< The text before the value occurs earlier in the line.
    NoSimpleRule,      ///< No simple rule finds exactly the marked value.
};

/// The longest text before the value ruleFromLine() makes, in UTF-16 units.
constexpr qsizetype kMaxTextBefore = 64;

/// A simple rule made from a sample line, with the key suggested for it.
struct RuleFromLine {
    MarkProblem problem = MarkProblem::None;
    QString key; ///< Suggested from the text before the value; may be empty.
    /// The rule; with TextBeforeEarlier or NoSimpleRule only its text before
    /// the value is set, to tell the user.
    SimpleRule rule;
    QString value; ///< The marked value, without the whitespace around it.

    /// Whether the mark makes a rule.
    bool isRule() const
    {
        return problem == MarkProblem::None;
    }
};

/**
 * The simple rule that finds the value marked in @p line, a Log Line the
 * user picked as a sample.
 *
 * The value is the @p length characters (UTF-16 units) at @p start, without
 * the whitespace around them; a @p length of 0 is a click, which marks the
 * whole word it lands in, or the next word after whitespace. A start
 * on a combining mark or on the second half of a surrogate pair moves back
 * to the character it belongs to, and an end inside one moves forward.
 * Whitespace is ASCII whitespace, as in simple rules.
 *
 * The text before the value is the word directly before it, with the word
 * before that when it has no letter or digit (`id =`), and at most its last
 * kMaxTextBefore characters. The value ends at
 * whitespace if it is one word followed by whitespace or the end of the
 * line, else at the character after it, else at the end of the line: the
 * first of these whose rule finds exactly the marked value is taken. The
 * key is suggestedKey() of the text.
 *
 * No rule (see MarkProblem) when nothing is marked, the value starts the
 * line, it ends past the part of the line the footer reads in a file of
 * @p encoding (footerReadLength()), the text before it occurs earlier in the
 * line so that the rule would find an earlier value, or no simple rule
 * finds exactly the marked value. The work is bounded and linear in the
 * length of the line: at most three matches, within the part of the line
 * the footer reads, of patterns made of a text of at most kMaxTextBefore
 * characters.
 */
RuleFromLine ruleFromLine( const QString& line, qsizetype start, qsizetype length,
                           const FooterScanner::Encoding& encoding = {} );

/// How much of @p line the footer reads in a file of @p encoding, in UTF-16
/// units: its first FooterScanner::kMaxLineBytes bytes in that encoding (in
/// UTF-16 that is kMaxLineBytes / 2 code units), cut at a character
/// boundary; all of a shorter line.
qsizetype footerReadLength( const QString& line, const FooterScanner::Encoding& encoding = {} );

/**
 * A key for the value after @p textBefore: its longest word, the last of
 * equally long ones, where a word is letters, digits and `_`, with `.` and
 * `-` inside it. `VIN:` gives `VIN`, `app.version=` gives `app.version`.
 * Empty if the text has no letter or digit.
 */
QString suggestedKey( const QString& textBefore );

} // namespace custom_footer
