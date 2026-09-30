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

/**
 * @file simplerule_test.cpp
 * @brief Pattern generation and classification of simple-mode rules.
 */

#include <catch2/catch.hpp>

#include "footerscanner.h"
#include "simplerule.h"

#include <QFile>
#include <QTemporaryDir>

#include <optional>

using namespace custom_footer;

namespace {

SimpleRule rule( const QString& textBefore, ValueEnd valueEnd = ValueEnd::Whitespace,
                 QChar endCharacter = QChar() )
{
    SimpleRule simple;
    simple.textBefore = textBefore;
    simple.valueEnd = valueEnd;
    simple.endCharacter = endCharacter;
    return simple;
}

/// The value a simple rule extracts from @p line, through the scanner, as
/// the footer would show it; nothing if the rule does not match.
std::optional<QString> extract( const SimpleRule& simple, const QString& line )
{
    FooterEntry entry;
    entry.key = "K";
    applySimpleRule( simple, entry );

    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto path = dir.path() + "/sample.log";
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    file.write( "first line\n" );
    file.write( line.toUtf8() );
    file.write( "\nlast line\n" );
    file.close();

    const auto values = FooterScanner::scan( path, { entry } );
    if ( !values.contains( "K" ) ) {
        return std::nullopt;
    }
    return values.value( "K" );
}

} // namespace

SCENARIO( "Simple rules generate a line pattern and no value pattern", "[simplerule]" )
{
    GIVEN( "the text before the value" )
    {
        THEN( "each way the value can end has its own capture" )
        {
            REQUIRE( simpleLinePattern( rule( "VIN:" ) ) == "VIN:\\s*(\\S+)" );
            REQUIRE( simpleLinePattern( rule( "VIN:", ValueEnd::EndOfLine ) )
                     == "VIN:\\s*(.*\\S)" );
            REQUIRE( simpleLinePattern( rule( "VIN:", ValueEnd::Character, ',' ) )
                     == "VIN:\\s*([^,]*[^,\\s])" );
        }

        THEN( "the rule gets no value pattern" )
        {
            FooterEntry entry{ "VIN", "old", "old value", true, {} };
            applySimpleRule( rule( "VIN:" ), entry );
            REQUIRE( entry.linePattern == "VIN:\\s*(\\S+)" );
            REQUIRE( entry.valuePattern.isEmpty() );
            REQUIRE( entry.key == "VIN" );
        }

        THEN( "every generated pattern compiles" )
        {
            for ( const auto end : { ValueEnd::Whitespace, ValueEnd::EndOfLine } ) {
                REQUIRE( FooterScanner::patternError(
                             simpleLinePattern( rule( "[.(\\$^*+?{|)]", end ) ) )
                             .isEmpty() );
            }
            for ( const QChar c : QString( "[].()\\$^*+?{}|-, ;x" ) ) {
                REQUIRE( FooterScanner::patternError(
                             simpleLinePattern( rule( "a", ValueEnd::Character, c ) ) )
                             .isEmpty() );
            }
        }
    }

    GIVEN( "no text before the value, or no end character" )
    {
        THEN( "there is no line pattern: the rule is incomplete, like an empty one" )
        {
            REQUIRE( simpleLinePattern( rule( "" ) ).isEmpty() );
            REQUIRE( simpleLinePattern( rule( "", ValueEnd::EndOfLine ) ).isEmpty() );
            REQUIRE( simpleLinePattern( rule( "VIN:", ValueEnd::Character ) ).isEmpty() );
        }
    }
}

SCENARIO( "Simple rules match the text before the value literally", "[simplerule]" )
{
    GIVEN( "text with characters that are special in regular expressions" )
    {
        THEN( "each of them is matched as itself" )
        {
            for ( const QString special :
                  { "[", ".", "(", "\\", "$", "^", "*", "+", "?", "{", "|", ")", "]", "}" } ) {
                const auto text = "id" + special + ":";
                INFO( text.toStdString() );
                REQUIRE( extract( rule( text ), "x " + text + " 42 y" ) == QString( "42" ) );
            }
        }

        THEN( "a dot does not match any character" )
        {
            REQUIRE( extract( rule( "v.1" ), "vx1 7" ) == std::nullopt );
            REQUIRE( extract( rule( "v.1" ), "v.1 7" ) == QString( "7" ) );
        }

        THEN( "brackets, parentheses and backslashes are not groups, classes or escapes" )
        {
            REQUIRE( extract( rule( "[id]" ), "i 1" ) == std::nullopt );
            REQUIRE( extract( rule( "[id]" ), "[id] 1" ) == QString( "1" ) );
            REQUIRE( extract( rule( "(a|b)" ), "a 2" ) == std::nullopt );
            REQUIRE( extract( rule( "(a|b)" ), "(a|b) 2" ) == QString( "2" ) );
            REQUIRE( extract( rule( "C:\\d" ), "C:5 3" ) == std::nullopt );
            REQUIRE( extract( rule( "C:\\d" ), "C:\\d 3" ) == QString( "3" ) );
            REQUIRE( extract( rule( "a+b*" ), "aab 4" ) == std::nullopt );
            REQUIRE( extract( rule( "^$" ), "x ^$ 5" ) == QString( "5" ) );
        }

        THEN( "spaces and non-ASCII text in it are matched as they are" )
        {
            const auto text = QString::fromUtf8( "Gr\xC3\xB6\xC3\x9F"
                                                 "e =" );
            REQUIRE( extract( rule( text ), text + " 12" ) == QString( "12" ) );
            REQUIRE( extract( rule( text ), QString( text ).remove( ' ' ) + " 12" )
                     == std::nullopt );
        }
    }

    GIVEN( "whitespace after the text" )
    {
        THEN( "any amount of it is ignored, or none" )
        {
            REQUIRE( extract( rule( "VIN:" ), "VIN:ABC" ) == QString( "ABC" ) );
            REQUIRE( extract( rule( "VIN:" ), "VIN: ABC" ) == QString( "ABC" ) );
            REQUIRE( extract( rule( "VIN:" ), "VIN:\t   ABC" ) == QString( "ABC" ) );
        }
    }
}

SCENARIO( "Simple rules end the value where the user says", "[simplerule]" )
{
    GIVEN( "a value that ends at whitespace" )
    {
        const auto simple = rule( "VIN:" );

        THEN( "the value is the next word" )
        {
            REQUIRE( extract( simple, "2024-01-01 VIN: WVW123 model Golf" )
                     == QString( "WVW123" ) );
            REQUIRE( extract( simple, "VIN: WVW123" ) == QString( "WVW123" ) );
            REQUIRE( extract( simple, "VIN:, x" ) == QString( "," ) );
        }

        THEN( "a line without a value does not match" )
        {
            REQUIRE( extract( simple, "VIN:" ) == std::nullopt );
            REQUIRE( extract( simple, "VIN:   " ) == std::nullopt );
            REQUIRE( extract( simple, "no vin here" ) == std::nullopt );
        }
    }

    GIVEN( "a value that ends at the end of the line" )
    {
        const auto simple = rule( "Model:", ValueEnd::EndOfLine );

        THEN( "the value is the rest of the line, without trailing whitespace" )
        {
            REQUIRE( extract( simple, "Model: Golf GTI 2.0" ) == QString( "Golf GTI 2.0" ) );
            REQUIRE( extract( simple, "Model:   Golf  GTI \t " ) == QString( "Golf  GTI" ) );
            REQUIRE( extract( simple, "Model:x" ) == QString( "x" ) );
        }

        THEN( "a line without a value does not match" )
        {
            REQUIRE( extract( simple, "Model:" ) == std::nullopt );
            REQUIRE( extract( simple, "Model:    " ) == std::nullopt );
        }
    }

    GIVEN( "a value that ends at a character" )
    {
        THEN( "the value runs up to that character, or to the end of the line" )
        {
            REQUIRE( extract( rule( "user=", ValueEnd::Character, ',' ), "user=Jane Doe, id=7" )
                     == QString( "Jane Doe" ) );
            REQUIRE( extract( rule( "user=", ValueEnd::Character, ';' ), "user=Jane;x" )
                     == QString( "Jane" ) );
            REQUIRE( extract( rule( "user=", ValueEnd::Character, ',' ), "user= Jane Doe" )
                     == QString( "Jane Doe" ) );
        }

        THEN( "whitespace around the value is not part of it" )
        {
            REQUIRE( extract( rule( "user=", ValueEnd::Character, ',' ), "user=  Jane Doe \t, x" )
                     == QString( "Jane Doe" ) );
            REQUIRE( extract( rule( "user=", ValueEnd::Character, ' ' ), "user=Jane Doe" )
                     == QString( "Jane" ) );
        }

        THEN( "characters special in a character class end the value as themselves" )
        {
            for ( const QChar c : QString( "]^\\-[.)|" ) ) {
                INFO( QString( c ).toStdString() );
                const auto line = QString( "k=ab%1cd" ).arg( c );
                REQUIRE( extract( rule( "k=", ValueEnd::Character, c ), line ) == QString( "ab" ) );
            }
        }

        THEN( "a line without a value before the character does not match" )
        {
            REQUIRE( extract( rule( "user=", ValueEnd::Character, ',' ), "user=,x" )
                     == std::nullopt );
            REQUIRE( extract( rule( "user=", ValueEnd::Character, ',' ), "user=  ,x" )
                     == std::nullopt );
        }
    }
}

SCENARIO( "Rules are simple only if their patterns are regenerated exactly", "[simplerule]" )
{
    GIVEN( "rules with every way to end the value, and special characters" )
    {
        QList<SimpleRule> rules;
        for ( const QString& text :
              { QString( "VIN:" ), QString( "[.(\\$^*+?{|)]" ), QString( "a b" ),
                QString::fromUtf8( "Gr\xC3\xB6\xC3\x9F"
                                   "e" ),
                QString( "x" ), QString( QChar() ) + "nul", QString( QChar() ) + "12",
                QString( "\\s*(\\S+)" ), QString::fromUtf8( "emoji \xF0\x9F\x98\x80:" ) } ) {
            rules.append( rule( text ) );
            rules.append( rule( text, ValueEnd::EndOfLine ) );
            for ( const QChar c : QString( ",;]^\\- x" ) ) {
                rules.append( rule( text, ValueEnd::Character, c ) );
            }
        }

        THEN( "their generated patterns classify as the same rules" )
        {
            for ( const auto& simple : rules ) {
                INFO( simpleLinePattern( simple ).toStdString() );
                const auto classified = simpleRuleOf( simpleLinePattern( simple ), QString() );
                REQUIRE( classified.has_value() );
                REQUIRE( *classified == simple );
            }
        }

        THEN( "with a value pattern they are not simple" )
        {
            for ( const auto& simple : rules ) {
                REQUIRE_FALSE( simpleRuleOf( simpleLinePattern( simple ), "(\\S+)" ) );
            }
        }
    }

    GIVEN( "a rule without patterns" )
    {
        THEN( "it is an empty simple rule, as a new rule is" )
        {
            const auto classified = simpleRuleOf( QString(), QString() );
            REQUIRE( classified.has_value() );
            REQUIRE( *classified == SimpleRule() );
            REQUIRE( simpleRuleOf( FooterEntry() ).has_value() );
        }
    }

    GIVEN( "patterns that are close to the simple form, but not it" )
    {
        THEN( "they are advanced" )
        {
            for ( const QString pattern :
                  { "VIN\\:\\s*(\\S+)",           // needless escape
                    "VIN\\ ID:\\s*(\\S+)",        // needless escape of a space
                    "VIN:\\s+(\\S+)",             // \s+ instead of \s*
                    "VIN:(\\S+)",                 // no whitespace
                    "VIN:\\s*(\\S+)$",            // anchored
                    "^VIN:\\s*(\\S+)",            // anchored
                    "VIN:\\s*(\\S+) ",            // trailing space
                    "VIN:\\s*(\\w+)",             // other capture
                    "VIN:\\s*(.+?)\\s*$",         // another end of line
                    "VIN:\\s*(.*\\S)$",           // anchored
                    "VIN:\\s*([^,]+)",            // another end at a character
                    "VIN:\\s*([^\\,]*[^\\,\\s])", // needless escape in the class
                    "VIN:\\s*([^,]*[^;\\s])",     // two characters
                    "VIN:\\s*([^,;]*[^,;\\s])",   // two characters
                    "VIN:\\s*([^]*[^\\s])",       // no character
                    "VIN:\\s*([^]]*[^]\\s])",     // ] not escaped
                    "\\s*(\\S+)",                 // no text
                    "VIN.\\s*(\\S+)",             // unescaped dot
                    "VIN\\\\:\\s*(\\S+)\\",       // trailing backslash
                    "[Vv]IN:\\s*(\\S+)",          // a class in the text
                    "VIN\\x3a\\s*(\\S+)",         // another escape in the text
                    "VIN:\\s*(\\S+)|x",           // alternative
                    "isComponentProtectionEnabled",
                    "build=(\\d+)" } ) {
                INFO( pattern.toStdString() );
                REQUIRE_FALSE( simpleRuleOf( pattern, QString() ) );
            }
        }

        THEN( "only a value pattern makes an empty line pattern advanced" )
        {
            REQUIRE_FALSE( simpleRuleOf( QString(), "(\\S+)" ) );
        }
    }

    GIVEN( "a rule that is simple" )
    {
        FooterEntry entry{ "VIN", "VIN:\\s*(\\S+)", "", true, {} };

        THEN( "it is classified from the entry too" )
        {
            const auto classified = simpleRuleOf( entry );
            REQUIRE( classified.has_value() );
            REQUIRE( classified->textBefore == "VIN:" );
            REQUIRE( classified->valueEnd == ValueEnd::Whitespace );
        }
    }
}

SCENARIO( "Simple rules escape only what is special", "[simplerule]" )
{
    GIVEN( "the most common hand-written rule" )
    {
        THEN( "it is simple" )
        {
            const auto classified = simpleRuleOf( "VIN:\\s*(\\S+)", QString() );
            REQUIRE( classified.has_value() );
            REQUIRE( *classified == rule( "VIN:" ) );
        }
    }

    GIVEN( "text with a space and a hash" )
    {
        const auto simple = rule( "Serial #: " );

        THEN( "the pattern keeps them as they are, and matches" )
        {
            REQUIRE( simpleLinePattern( simple ) == "Serial #: \\s*(\\S+)" );
            REQUIRE( extract( simple, "boot Serial #: SN-42 ok" ) == QString( "SN-42" ) );
            REQUIRE( extract( simple, "boot Serial #:SN-42" ) == std::nullopt );
            REQUIRE( simpleRuleOf( simpleLinePattern( simple ), QString() ) == simple );
        }
    }

    GIVEN( "non-ASCII text" )
    {
        const auto text = QString::fromUtf8( "Gr\xC3\xB6\xC3\x9F"
                                             "e:" );

        THEN( "it is not escaped, and matches" )
        {
            REQUIRE( simpleLinePattern( rule( text ) ) == text + "\\s*(\\S+)" );
            REQUIRE( extract( rule( text ), "x " + text + " 12 cm" ) == QString( "12" ) );
            REQUIRE( simpleRuleOf( text + "\\s*(\\S+)", QString() ) == rule( text ) );
        }
    }

    GIVEN( "metacharacters in the text" )
    {
        THEN( "each is escaped once" )
        {
            REQUIRE( simpleLinePattern( rule( "\\^$.|?*+()[]{}" ) )
                     == "\\\\\\^\\$\\.\\|\\?\\*\\+\\(\\)\\[\\]\\{\\}\\s*(\\S+)" );
        }
    }

    GIVEN( "end characters special in a character class" )
    {
        THEN( "only those are escaped, and each ends the value" )
        {
            const QList<QPair<QChar, QString>> classes{ { ']', "\\]" }, { '-', "\\-" },
                                                        { '^', "\\^" }, { '\\', "\\\\" },
                                                        { '.', "." },   { '[', "[" },
                                                        { ',', "," } };
            for ( const auto& [ c, escaped ] : classes ) {
                INFO( QString( c ).toStdString() );
                const auto simple = rule( "k=", ValueEnd::Character, c );
                REQUIRE( simpleLinePattern( simple )
                         == "k=\\s*([^" + escaped + "]*[^" + escaped + "\\s])" );
                REQUIRE( extract( simple, QString( "k= ab %1cd" ).arg( c ) ) == QString( "ab" ) );
                REQUIRE( simpleRuleOf( simpleLinePattern( simple ), QString() ) == simple );
            }
        }
    }
}
