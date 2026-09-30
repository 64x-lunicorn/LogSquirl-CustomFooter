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

#include <QRegularExpression>

namespace custom_footer {

namespace {

/// Whitespace between the text and the value.
const QLatin1String kGap( "\\s*" );

/// The pattern after the escaped text, for a complete rule.
QString valuePart( ValueEnd valueEnd, QChar endCharacter )
{
    switch ( valueEnd ) {
    case ValueEnd::Whitespace:
        return kGap + QLatin1String( "(\\S+)" );
    case ValueEnd::EndOfLine:
        // Up to the last non-space, so the value is never only whitespace.
        return kGap + QLatin1String( "(.*\\S)" );
    case ValueEnd::Character: {
        // Up to the last non-space before the character, as above.
        const auto end = QRegularExpression::escape( QString( endCharacter ) );
        return kGap + QLatin1String( "([^" ) + end + QLatin1String( "]*[^" ) + end
               + QLatin1String( "\\s])" );
    }
    }
    return {};
}

/// The text QRegularExpression::escape() made @p escaped from. Other
/// escapes come back as the character after the backslash; comparing the
/// escaped result with @p escaped tells those apart.
std::optional<QString> unescape( const QString& escaped )
{
    QString text;
    text.reserve( escaped.size() );
    for ( qsizetype i = 0; i < escaped.size(); ++i ) {
        if ( escaped.at( i ) != QLatin1Char( '\\' ) ) {
            text.append( escaped.at( i ) );
            continue;
        }
        if ( ++i == escaped.size() ) {
            return std::nullopt;
        }
        text.append( escaped.at( i ) == QLatin1Char( '0' ) ? QChar() : escaped.at( i ) );
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
        if ( !character || character->size() != 1 ) {
            return std::nullopt;
        }
        rule.endCharacter = character->at( 0 );
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
    if ( rule.textBefore.isEmpty()
         || ( rule.valueEnd == ValueEnd::Character && rule.endCharacter.isNull() ) ) {
        return {};
    }
    return QRegularExpression::escape( rule.textBefore )
           + valuePart( rule.valueEnd, rule.endCharacter );
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
