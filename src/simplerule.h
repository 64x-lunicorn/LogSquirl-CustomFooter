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

#include "footerentry.h"

#include <QChar>
#include <QString>

#include <optional>

namespace custom_footer {

/// Where the value of a simple rule ends.
enum class ValueEnd {
    Whitespace, ///< At the next whitespace: the value is one word.
    EndOfLine,  ///< At the end of the line, without trailing whitespace.
    Character,  ///< Before a given character, e.g. `,` or `;`.
};

/**
 * A rule described without a regular expression: the value follows a text,
 * matched literally, with any whitespace after it ignored.
 *
 * A simple rule is only a way to write and read a rule's patterns: it is
 * saved as the patterns simpleLinePattern() generates, and a rule is simple
 * exactly when simpleRuleOf() finds the simple rule whose patterns it has.
 */
struct SimpleRule {
    QString textBefore; ///< E.g. `VIN:`, matched literally.
    ValueEnd valueEnd = ValueEnd::Whitespace;
    QChar endCharacter; ///< Used with ValueEnd::Character.

    bool operator==( const SimpleRule& other ) const
    {
        return textBefore == other.textBefore && valueEnd == other.valueEnd
               && endCharacter == other.endCharacter;
    }
    bool operator!=( const SimpleRule& other ) const
    {
        return !( *this == other );
    }
};

/**
 * The line pattern of a simple rule: the text, optional whitespace, and the
 * value as the first capturing group. Only the metacharacters
 * `\ ^ $ . | ? * + ( ) [ ] { }` of the text are escaped, and only
 * `\ ] ^ -` of an end character, so `VIN:` becomes `VIN:\s*(\S+)`. Simple rules need no value
 * pattern.
 *
 * Without a text, or with ValueEnd::Character but no character, the pattern
 * is empty: the rule is incomplete and matches nothing, as a new rule.
 */
QString simpleLinePattern( const SimpleRule& rule );

/// Give @p entry the patterns of @p rule; its key, mappings and enabled
/// state are kept.
void applySimpleRule( const SimpleRule& rule, FooterEntry& entry );

/**
 * The simple rule whose patterns these are, if there is one: the patterns
 * generated from it are these patterns, byte for byte. Empty patterns are
 * the empty simple rule. Any other patterns are advanced.
 */
std::optional<SimpleRule> simpleRuleOf( const QString& linePattern, const QString& valuePattern );

/// The simple rule of @p entry's patterns, if they have the simple form.
std::optional<SimpleRule> simpleRuleOf( const FooterEntry& entry );

} // namespace custom_footer
