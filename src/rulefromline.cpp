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

#include "rulefromline.h"

#include <QList>
#include <QRegularExpression>

namespace custom_footer {

namespace {

/// ASCII whitespace, as `\s` in the patterns: the scanner compiles them
/// without Unicode properties.
bool isSpace( QChar c )
{
    return c == QLatin1Char( ' ' ) || c == QLatin1Char( '\t' ) || c == QLatin1Char( '\n' )
           || c == QLatin1Char( '\v' ) || c == QLatin1Char( '\f' ) || c == QLatin1Char( '\r' );
}

/// Whether @p text has a letter or a digit, in any script.
bool hasLetterOrDigit( const QString& text )
{
    const auto codePoints = text.toUcs4();
    for ( const auto codePoint : codePoints ) {
        if ( QChar::isLetterOrNumber( codePoint ) ) {
            return true;
        }
    }
    return false;
}

/// Whether @p rule, compiled as the scanner compiles it, finds exactly
/// @p value at @p start of @p line.
bool findsExactly( const SimpleRule& rule, const QString& line, qsizetype start,
                   const QString& value )
{
    const auto pattern = simpleLinePattern( rule );
    if ( pattern.isEmpty() ) {
        return false;
    }
    const auto match = QRegularExpression( pattern ).match( line );
    return match.hasMatch() && match.capturedStart( 1 ) == start && match.captured( 1 ) == value;
}

/// Where each word before @p end starts, the nearest first. Words are
/// separated by whitespace; @p end is just after a word.
QList<qsizetype> wordStartsBefore( const QString& line, qsizetype end )
{
    QList<qsizetype> starts;
    auto at = end;
    while ( at > 0 ) {
        while ( at > 0 && !isSpace( line.at( at - 1 ) ) ) {
            --at;
        }
        starts.append( at );
        while ( at > 0 && isSpace( line.at( at - 1 ) ) ) {
            --at;
        }
    }
    return starts;
}

/// The ways the value may end, the most likely first: at whitespace, at the
/// character after the value, at the end of the line.
QList<SimpleRule> valueEnds( const QString& line, qsizetype valueEnd )
{
    QList<SimpleRule> ends;
    SimpleRule atWhitespace;
    ends.append( atWhitespace );
    if ( valueEnd < line.size() && !isSpace( line.at( valueEnd ) ) ) {
        SimpleRule atCharacter;
        atCharacter.valueEnd = ValueEnd::Character;
        const bool pair = line.at( valueEnd ).isHighSurrogate() && valueEnd + 1 < line.size()
                          && line.at( valueEnd + 1 ).isLowSurrogate();
        atCharacter.endCharacter = line.mid( valueEnd, pair ? 2 : 1 );
        if ( endCharacterProblem( atCharacter ).isEmpty() ) {
            ends.append( atCharacter );
        }
    }
    SimpleRule atEndOfLine;
    atEndOfLine.valueEnd = ValueEnd::EndOfLine;
    ends.append( atEndOfLine );
    return ends;
}

} // namespace

std::optional<RuleFromLine> ruleFromLine( const QString& line, qsizetype start, qsizetype length )
{
    if ( start < 0 || length < 0 || start > line.size() || length > line.size() - start ) {
        return std::nullopt;
    }

    // The value: the mark without the whitespace around it, or for a click
    // the word starting at or after it.
    auto valueStart = start;
    auto valueEnd = start + length;
    while ( valueStart < valueEnd && isSpace( line.at( valueStart ) ) ) {
        ++valueStart;
    }
    if ( length == 0 ) {
        while ( valueStart < line.size() && isSpace( line.at( valueStart ) ) ) {
            ++valueStart;
        }
        valueEnd = valueStart;
        while ( valueEnd < line.size() && !isSpace( line.at( valueEnd ) ) ) {
            ++valueEnd;
        }
    }
    while ( valueEnd > valueStart && isSpace( line.at( valueEnd - 1 ) ) ) {
        --valueEnd;
    }
    if ( valueEnd == valueStart ) {
        return std::nullopt;
    }
    const auto value = line.mid( valueStart, valueEnd - valueStart );

    // The text before it ends at its last non-whitespace, as the rule
    // ignores whitespace between the text and the value.
    auto textEnd = valueStart;
    while ( textEnd > 0 && isSpace( line.at( textEnd - 1 ) ) ) {
        --textEnd;
    }
    if ( textEnd == 0 ) {
        return std::nullopt;
    }

    auto starts = wordStartsBefore( line, textEnd );
    // A separator on its own, such as `=`, says little: the word before it
    // belongs to the text.
    if ( starts.size() > 1
         && !hasLetterOrDigit( line.mid( starts.first(), textEnd - starts.first() ) ) ) {
        starts.removeFirst();
    }
    const auto ends = valueEnds( line, valueEnd );
    for ( const auto textStart : std::as_const( starts ) ) {
        const auto textBefore = line.mid( textStart, textEnd - textStart );
        for ( auto rule : ends ) {
            rule.textBefore = textBefore;
            if ( findsExactly( rule, line, valueStart, value ) ) {
                return RuleFromLine{ suggestedKey( textBefore ), rule, value };
            }
        }
    }
    return std::nullopt;
}

QString suggestedKey( const QString& textBefore )
{
    const auto isWordPart = []( char32_t c ) {
        return QChar::isLetterOrNumber( c ) || c == U'_' || c == U'.' || c == U'-';
    };
    const auto isInnerOnly = []( char32_t c ) { return c == U'.' || c == U'-'; };

    QString best;
    const auto codePoints = textBefore.toUcs4();
    for ( qsizetype at = 0; at < codePoints.size(); ) {
        if ( !isWordPart( codePoints.at( at ) ) ) {
            ++at;
            continue;
        }
        auto first = at;
        auto end = at;
        while ( end < codePoints.size() && isWordPart( codePoints.at( end ) ) ) {
            ++end;
        }
        at = end;
        // `.` and `-` only inside a word.
        while ( first < end && isInnerOnly( codePoints.at( first ) ) ) {
            ++first;
        }
        while ( end > first && isInnerOnly( codePoints.at( end - 1 ) ) ) {
            --end;
        }
        const auto word = QString::fromUcs4(
            reinterpret_cast<const char32_t*>( codePoints.constData() ) + first, end - first );
        if ( hasLetterOrDigit( word ) && word.size() >= best.size() ) {
            best = word;
        }
    }
    return best;
}

} // namespace custom_footer
