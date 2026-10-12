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
#include "footerscanner.h"

#include <QList>
#include <QRegularExpression>
#include <QStringView>

namespace custom_footer {

namespace {

/// ASCII whitespace, as `\s` in the patterns: the scanner compiles them
/// without Unicode properties.
bool isSpace( QChar c )
{
    return c == QLatin1Char( ' ' ) || c == QLatin1Char( '\t' ) || c == QLatin1Char( '\n' )
           || c == QLatin1Char( '\v' ) || c == QLatin1Char( '\f' ) || c == QLatin1Char( '\r' );
}

/// The code point at @p at in @p text, and in @p units how many UTF-16
/// units it takes: two for a surrogate pair, else one.
char32_t codePointAt( QStringView text, qsizetype at, qsizetype* units )
{
    const auto c = text.at( at );
    if ( c.isHighSurrogate() && at + 1 < text.size() && text.at( at + 1 ).isLowSurrogate() ) {
        *units = 2;
        return QChar::surrogateToUcs4( c, text.at( at + 1 ) );
    }
    *units = 1;
    return c.unicode();
}

/// Whether @p text has a letter or a digit, in any script.
bool hasLetterOrDigit( QStringView text )
{
    qsizetype units = 1;
    for ( qsizetype at = 0; at < text.size(); at += units ) {
        if ( QChar::isLetterOrNumber( codePointAt( text, at, &units ) ) ) {
            return true;
        }
    }
    return false;
}

/// Whether the unit at @p at belongs to the character before it: a
/// combining mark, or the second half of a surrogate pair.
bool continuesCharacter( const QString& line, qsizetype at )
{
    if ( at <= 0 || at >= line.size() ) {
        return false;
    }
    const auto c = line.at( at );
    return c.isMark() || ( c.isLowSurrogate() && line.at( at - 1 ).isHighSurrogate() );
}

/// Where the word before @p end starts; @p end is just after the word.
qsizetype wordStartBefore( const QString& line, qsizetype end )
{
    auto at = end;
    while ( at > 0 && !isSpace( line.at( at - 1 ) ) ) {
        --at;
    }
    return at;
}

/// The part of @p line the footer reads: its first kMaxLineBytes in UTF-8,
/// in UTF-16 units.
qsizetype readLength( const QString& line )
{
    qint64 bytes = 0;
    for ( qsizetype at = 0; at < line.size(); ++at ) {
        const auto c = line.at( at ).unicode();
        // A surrogate pair is four bytes, two for each half.
        bytes += c < 0x80 ? 1 : c < 0x800 ? 2 : ( c >= 0xD800 && c < 0xE000 ) ? 2 : 3;
        if ( bytes > FooterScanner::kMaxLineBytes ) {
            return at;
        }
    }
    return line.size();
}

/// The ways the value may end, the most likely first: at whitespace, at the
/// character after the value, at the end of the line.
QList<SimpleRule> valueEnds( const QString& line, qsizetype valueEnd )
{
    QList<SimpleRule> ends;
    ends.append( SimpleRule() );
    if ( valueEnd < line.size() && !isSpace( line.at( valueEnd ) ) ) {
        SimpleRule atCharacter;
        atCharacter.valueEnd = ValueEnd::Character;
        qsizetype units = 1;
        codePointAt( line, valueEnd, &units );
        atCharacter.endCharacter = line.mid( valueEnd, units );
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

RuleFromLine ruleFromLine( const QString& line, qsizetype start, qsizetype length )
{
    RuleFromLine made;
    made.problem = MarkProblem::NothingMarked;
    if ( start < 0 || length < 0 || start > line.size() || length > line.size() - start ) {
        return made;
    }

    // The value: the mark without the whitespace around it, or for a click
    // the word starting at or after it; whole characters only.
    auto valueStart = start;
    auto valueEnd = start + length;
    while ( continuesCharacter( line, valueStart ) ) {
        --valueStart;
    }
    while ( continuesCharacter( line, valueEnd ) ) {
        ++valueEnd;
    }
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
        return made;
    }
    made.value = line.mid( valueStart, valueEnd - valueStart );

    // The text before it ends at its last non-whitespace, as the rule
    // ignores whitespace between the text and the value.
    auto textEnd = valueStart;
    while ( textEnd > 0 && isSpace( line.at( textEnd - 1 ) ) ) {
        --textEnd;
    }
    if ( textEnd == 0 ) {
        made.problem = MarkProblem::NoTextBefore;
        return made;
    }

    // The footer matches a long line's start only.
    const auto read = readLength( line );
    if ( valueEnd > read ) {
        made.problem = MarkProblem::BeyondLineLimit;
        return made;
    }

    auto textStart = wordStartBefore( line, textEnd );
    if ( !hasLetterOrDigit( QStringView( line ).mid( textStart, textEnd - textStart ) ) ) {
        // A separator on its own, such as `=`, says little: the word before
        // it belongs to the text.
        auto before = textStart;
        while ( before > 0 && isSpace( line.at( before - 1 ) ) ) {
            --before;
        }
        if ( before > 0 ) {
            textStart = wordStartBefore( line, before );
        }
    }
    // Short enough to keep matching cheap; whole characters only.
    textStart = qMax( textStart, textEnd - kMaxTextBefore );
    while ( continuesCharacter( line, textStart ) ) {
        ++textStart;
    }
    made.rule.textBefore = line.mid( textStart, textEnd - textStart );

    const QString readPart = line.left( read );
    bool earlier = false;
    for ( auto rule : valueEnds( line, valueEnd ) ) {
        rule.textBefore = made.rule.textBefore;
        const QRegularExpression regex( simpleLinePattern( rule ) );
        if ( regex.pattern().isEmpty() || !regex.isValid() ) {
            continue;
        }
        // As the scanner matches: the first match in what it reads.
        const auto match = regex.match( readPart );
        if ( !match.hasMatch() ) {
            continue;
        }
        if ( match.capturedStart( 0 ) < textStart ) {
            // The same text before an earlier value.
            earlier = true;
            continue;
        }
        if ( match.capturedStart( 1 ) == valueStart && match.captured( 1 ) == made.value ) {
            made.problem = MarkProblem::None;
            made.rule = rule;
            made.key = suggestedKey( rule.textBefore );
            return made;
        }
    }
    made.problem = earlier ? MarkProblem::TextBeforeEarlier : MarkProblem::NoSimpleRule;
    return made;
}

QString suggestedKey( const QString& textBefore )
{
    const auto isWordPart = []( char32_t c ) {
        return QChar::isLetterOrNumber( c ) || c == U'_' || c == U'.' || c == U'-';
    };
    const auto isInnerOnly
        = []( QChar c ) { return c == QLatin1Char( '.' ) || c == QLatin1Char( '-' ); };

    QStringView best;
    qsizetype units = 1;
    for ( qsizetype at = 0; at < textBefore.size(); ) {
        if ( !isWordPart( codePointAt( textBefore, at, &units ) ) ) {
            at += units;
            continue;
        }
        auto first = at;
        auto end = at;
        while ( end < textBefore.size() && isWordPart( codePointAt( textBefore, end, &units ) ) ) {
            end += units;
        }
        at = end;
        // `.` and `-` only inside a word.
        while ( first < end && isInnerOnly( textBefore.at( first ) ) ) {
            ++first;
        }
        while ( end > first && isInnerOnly( textBefore.at( end - 1 ) ) ) {
            --end;
        }
        const auto word = QStringView( textBefore ).mid( first, end - first );
        if ( hasLetterOrDigit( word ) && word.size() >= best.size() ) {
            best = word;
        }
    }
    return best.toString();
}

} // namespace custom_footer
