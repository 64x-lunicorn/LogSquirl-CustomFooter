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

#include "simplerule.h"
#include "footerentry.h"

#include <QCoreApplication>

namespace custom_footer {

namespace {

/// How escaped() writes NUL.
const QLatin1String kNul( "\\x{0}" );

/// Escape the characters of @p text that are in @p special with a
/// backslash, and NUL as `\x{0}`, which no digit after it can extend;
/// everything else stays readable as it is.
QString escaped( const QString& text, QLatin1String special )
{
    QString result;
    result.reserve( text.size() );
    for ( const QChar c : text ) {
        if ( c.isNull() ) {
            result.append( kNul );
            continue;
        }
        if ( special.contains( c ) ) {
            result.append( QLatin1Char( '\\' ) );
        }
        result.append( c );
    }
    return result;
}

/// The metacharacters of PCRE2 outside a character class. `]` and `}` are
/// literal there, as `{` is escaped. Patterns are never compiled in
/// extended mode, so whitespace and `#` are literal.
QString escapedText( const QString& text )
{
    return escaped( text, QLatin1String( "\\^$.|?*+()[{" ) );
}

/// The characters special inside a character class.
QString escapedInClass( const QString& character )
{
    return escaped( character, QLatin1String( "\\]^-" ) );
}

/// Whether @p text is exactly one code point, as one UTF-16 unit or a
/// surrogate pair.
bool isOneCodePoint( const QString& text )
{
    if ( text.size() == 1 ) {
        return !text.at( 0 ).isSurrogate();
    }
    return text.size() == 2 && text.at( 0 ).isHighSurrogate() && text.at( 1 ).isLowSurrogate();
}

/// Whitespace between the text and the value.
const QLatin1String kGap( "\\s*" );

/// The pattern after the escaped text, for a complete rule.
QString valuePart( ValueEnd valueEnd, const QString& endCharacter )
{
    switch ( valueEnd ) {
    case ValueEnd::Whitespace:
        return kGap + QLatin1String( "(\\S+)" );
    case ValueEnd::EndOfLine:
        // Up to the last non-space, so the value is never only whitespace.
        return kGap + QLatin1String( "(.*\\S)" );
    case ValueEnd::Character: {
        // Up to the last non-space before the character, as above.
        const auto end = escapedInClass( endCharacter );
        return kGap + QLatin1String( "([^" ) + end + QLatin1String( "]*[^" ) + end
               + QLatin1String( "\\s])" );
    }
    }
    return {};
}

/// The text escaped() made @p pattern from. Other escapes come back as the
/// character after the backslash; escaping the result again and comparing
/// it with @p pattern tells those apart.
std::optional<QString> unescape( const QString& pattern )
{
    QString text;
    text.reserve( pattern.size() );
    for ( qsizetype i = 0; i < pattern.size(); ++i ) {
        if ( pattern.at( i ) != QLatin1Char( '\\' ) ) {
            text.append( pattern.at( i ) );
            continue;
        }
        if ( QStringView( pattern ).mid( i ).startsWith( kNul ) ) {
            text.append( QChar() );
            i += kNul.size() - 1;
            continue;
        }
        if ( ++i == pattern.size() ) {
            return std::nullopt;
        }
        text.append( pattern.at( i ) );
    }
    return text;
}

/// The simple rule ending as @p valueEnd with the line pattern
/// @p linePattern, if there is one.
std::optional<SimpleRule> ruleEndingAs( const QString& linePattern, ValueEnd valueEnd )
{
    SimpleRule rule;
    rule.valueEnd = valueEnd;

    QString suffix;
    if ( valueEnd == ValueEnd::Character ) {
        // `\s*([^E]*[^E\s])`, the escaped character E twice.
        const QLatin1String start( "\\s*([^" );
        const auto at = linePattern.lastIndexOf( start );
        if ( at < 0 ) {
            return std::nullopt;
        }
        const auto rest = linePattern.mid( at + start.size() );
        const auto fixedLength = QLatin1String( "]*[^\\s])" ).size();
        if ( ( rest.size() - fixedLength ) % 2 != 0 ) {
            return std::nullopt;
        }
        const auto character = unescape( rest.left( ( rest.size() - fixedLength ) / 2 ) );
        if ( !character || !isOneCodePoint( *character ) ) {
            return std::nullopt;
        }
        rule.endCharacter = *character;
    }
    suffix = valuePart( valueEnd, rule.endCharacter );
    if ( !linePattern.endsWith( suffix ) ) {
        return std::nullopt;
    }

    const auto text = unescape( linePattern.chopped( suffix.size() ) );
    if ( !text || text->isEmpty() ) {
        return std::nullopt;
    }
    rule.textBefore = *text;
    if ( simpleLinePattern( rule ) != linePattern ) {
        return std::nullopt;
    }
    return rule;
}

} // namespace

QString simpleLinePattern( const SimpleRule& rule )
{
    if ( rule.textBefore.isEmpty() || !endCharacterProblem( rule ).isEmpty() ) {
        return {};
    }
    return escapedText( rule.textBefore ) + valuePart( rule.valueEnd, rule.endCharacter );
}

QString endCharacterProblem( const SimpleRule& rule )
{
    if ( rule.valueEnd != ValueEnd::Character ) {
        return {};
    }
    if ( rule.endCharacter.isEmpty() ) {
        return QCoreApplication::translate( "SimpleRule", "Enter the character the value ends at" );
    }
    if ( !isOneCodePoint( rule.endCharacter ) ) {
        return QCoreApplication::translate( "SimpleRule", "Enter a single character" );
    }
    if ( QChar::category( rule.endCharacter.toUcs4().at( 0 ) ) == QChar::Other_Control ) {
        return QCoreApplication::translate( "SimpleRule",
                                            "The value cannot end at a control character" );
    }
    return {};
}

void applySimpleRule( const SimpleRule& rule, FooterEntry& entry )
{
    entry.linePattern = simpleLinePattern( rule );
    entry.valuePattern.clear();
}

std::optional<SimpleRule> simpleRuleOf( const QString& linePattern, const QString& valuePattern )
{
    if ( !valuePattern.isEmpty() ) {
        return std::nullopt;
    }
    if ( linePattern.isEmpty() ) {
        return SimpleRule();
    }
    for ( const auto valueEnd :
          { ValueEnd::Whitespace, ValueEnd::EndOfLine, ValueEnd::Character } ) {
        if ( auto rule = ruleEndingAs( linePattern, valueEnd ) ) {
            return rule;
        }
    }
    return std::nullopt;
}

std::optional<SimpleRule> simpleRuleOf( const FooterEntry& entry )
{
    return simpleRuleOf( entry.linePattern, entry.valuePattern );
}

} // namespace custom_footer
