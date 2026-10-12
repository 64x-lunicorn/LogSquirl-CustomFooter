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
 * @file rulefromline_test.cpp
 * @brief A simple rule and its key, made from a sample line and the value
 *        marked in it.
 */

#include <catch2/catch.hpp>

#include "footerscanner.h"
#include "rulefromline.h"
#include "simplerule.h"

#include <QFile>
#include <QTemporaryDir>

#include <optional>

using namespace custom_footer;

namespace {

/// The rule made from @p line with @p value marked: its first occurrence
/// at or after @p from.
std::optional<RuleFromLine> marking( const QString& line, const QString& value, int from = 0 )
{
    const auto start = line.indexOf( value, from );
    REQUIRE( start >= 0 );
    return ruleFromLine( line, start, value.size() );
}

/// The value the rule extracts from @p line through the scanner, as the
/// footer would show it, with other lines around it; nothing if none.
std::optional<QString> extract( const RuleFromLine& made, const QString& line )
{
    FooterEntry entry;
    entry.key = made.key.isEmpty() ? QStringLiteral( "K" ) : made.key;
    applySimpleRule( made.rule, entry );

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
    if ( !values.contains( entry.key ) ) {
        return std::nullopt;
    }
    return values.value( entry.key );
}

} // namespace

SCENARIO( "A rule is made from the value marked in a sample line", "[rulefromline]" )
{
    GIVEN( "a Log Line with a VIN after a timestamp and a level" )
    {
        const QString line = "2026-10-12 08:15:02.123 INFO VIN: WDB1234567 speed=5";

        WHEN( "the VIN is marked" )
        {
            const auto made = marking( line, "WDB1234567" );

            THEN( "the word before it is the text before the value, the value ends at "
                  "whitespace, and the key is that word without its colon" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "VIN:" );
                REQUIRE( made->rule.valueEnd == ValueEnd::Whitespace );
                REQUIRE( made->key == "VIN" );
                REQUIRE( made->value == "WDB1234567" );
            }

            THEN( "the rule finds the VIN in the line, as a simple rule" )
            {
                REQUIRE( extract( *made, line ) == QString( "WDB1234567" ) );
                FooterEntry entry;
                applySimpleRule( made->rule, entry );
                REQUIRE( simpleRuleOf( entry ) == made->rule );
            }
        }

        WHEN( "the user clicks where the VIN starts" )
        {
            const auto made = ruleFromLine( line, line.indexOf( "WDB" ), 0 );

            THEN( "the value is the word starting there" )
            {
                REQUIRE( made );
                REQUIRE( made->value == "WDB1234567" );
                REQUIRE( made->rule.textBefore == "VIN:" );
                REQUIRE( made->rule.valueEnd == ValueEnd::Whitespace );
            }
        }

        WHEN( "the user clicks into the space before the VIN" )
        {
            const auto made = ruleFromLine( line, line.indexOf( "VIN:" ) + 4, 0 );

            THEN( "the value is the next word" )
            {
                REQUIRE( made );
                REQUIRE( made->value == "WDB1234567" );
                REQUIRE( made->rule.textBefore == "VIN:" );
            }
        }

        WHEN( "the mark takes the spaces around the VIN too" )
        {
            const auto made = marking( line, " WDB1234567 " );

            THEN( "the spaces are not part of the value" )
            {
                REQUIRE( made );
                REQUIRE( made->value == "WDB1234567" );
                REQUIRE( made->rule.textBefore == "VIN:" );
                REQUIRE( made->rule.valueEnd == ValueEnd::Whitespace );
            }
        }

        WHEN( "the speed after its equals sign is marked" )
        {
            const auto made = marking( line, "5", line.indexOf( "speed" ) );

            THEN( "the text before it is speed=, and the key speed" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "speed=" );
                REQUIRE( made->key == "speed" );
                REQUIRE( extract( *made, line ) == QString( "5" ) );
            }
        }
    }

    GIVEN( "regular expression metacharacters right before the value" )
    {
        const QString line = "x [sensor.temp](°C)*+?^$|{2}\\: 21.5 ok";

        WHEN( "the value is marked" )
        {
            const auto made = marking( line, "21.5" );

            THEN( "they are matched literally, and the key is the longest word in them" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "[sensor.temp](°C)*+?^$|{2}\\:" );
                REQUIRE( made->key == "sensor.temp" );
                REQUIRE( extract( *made, line ) == QString( "21.5" ) );
            }
        }
    }

    GIVEN( "a separator on its own before the value" )
    {
        const QString line = "level id = 42 done";

        WHEN( "the value is marked" )
        {
            const auto made = marking( line, "42" );

            THEN( "the word before the separator belongs to the text, and is the key" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "id =" );
                REQUIRE( made->key == "id" );
                REQUIRE( extract( *made, line ) == QString( "42" ) );
            }
        }
    }

    GIVEN( "Unicode text and a value with a space in it at the end of the line" )
    {
        const QString line = QString::fromUtf8( "Größe → Wert: 12 m²" );

        WHEN( "the value is marked" )
        {
            const auto made = marking( line, QString::fromUtf8( "12 m²" ) );

            THEN( "the value ends at the end of the line, and the key keeps its letters" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "Wert:" );
                REQUIRE( made->rule.valueEnd == ValueEnd::EndOfLine );
                REQUIRE( made->key == "Wert" );
                REQUIRE( extract( *made, line ) == QString::fromUtf8( "12 m²" ) );
            }
        }

        WHEN( "the word with umlauts is the text before a value" )
        {
            const QString other = QString::fromUtf8( "Größe: 3" );
            const auto made = marking( other, "3" );

            THEN( "it is the key" )
            {
                REQUIRE( made );
                REQUIRE( made->key == QString::fromUtf8( "Größe" ) );
            }
        }
    }

    GIVEN( "a line with tabs" )
    {
        const QString line = "VIN:\tWDB1\tok";

        WHEN( "the value between the tabs is marked" )
        {
            const auto made = marking( line, "WDB1" );

            THEN( "the tab after the text is the whitespace that is ignored" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "VIN:" );
                REQUIRE( made->rule.valueEnd == ValueEnd::Whitespace );
                REQUIRE( extract( *made, line ) == QString( "WDB1" ) );
            }
        }
    }

    GIVEN( "leading and trailing whitespace in the line" )
    {
        const QString line = "   Owner:   John Smith   ";

        WHEN( "the name and the spaces after it are marked" )
        {
            const auto made = marking( line, "John Smith   " );

            THEN( "the value is the name, up to the end of the line" )
            {
                REQUIRE( made );
                REQUIRE( made->value == "John Smith" );
                REQUIRE( made->rule.textBefore == "Owner:" );
                REQUIRE( made->rule.valueEnd == ValueEnd::EndOfLine );
                REQUIRE( extract( *made, line ) == QString( "John Smith" ) );
            }
        }
    }

    GIVEN( "values separated by commas and semicolons" )
    {
        WHEN( "a value right before a comma is marked" )
        {
            const QString line = "speed=5,rpm=300";
            const auto made = marking( line, "5" );

            THEN( "the value ends at the comma" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.valueEnd == ValueEnd::Character );
                REQUIRE( made->rule.endCharacter == "," );
                REQUIRE( extract( *made, line ) == QString( "5" ) );
            }
        }

        WHEN( "a value with a space in it before a semicolon is marked" )
        {
            const QString line = "msg=hello world; next=1";
            const auto made = marking( line, "hello world" );

            THEN( "the value ends at the semicolon" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "msg=" );
                REQUIRE( made->rule.valueEnd == ValueEnd::Character );
                REQUIRE( made->rule.endCharacter == ";" );
                REQUIRE( extract( *made, line ) == QString( "hello world" ) );
            }
        }
    }

    GIVEN( "the text before the value also earlier in the line" )
    {
        const QString line = "a: 1\ta: 2";

        WHEN( "the second value is marked" )
        {
            const auto made = marking( line, "2" );

            THEN( "the text reaches back until the rule finds the marked value, not the first" )
            {
                REQUIRE( made );
                REQUIRE( made->rule.textBefore == "1\ta:" );
                REQUIRE( made->key == "a" );
                REQUIRE( extract( *made, line ) == QString( "2" ) );
            }
        }
    }

    GIVEN( "marks that make no rule" )
    {
        const QString line = "WDB123 is the VIN  ";

        THEN( "a value at the start of the line has no text before it" )
        {
            REQUIRE_FALSE( marking( line, "WDB123" ) );
        }

        THEN( "a click into the whitespace at the end marks nothing" )
        {
            REQUIRE_FALSE( ruleFromLine( line, line.size() - 1, 0 ) );
            REQUIRE_FALSE( ruleFromLine( line, line.size(), 0 ) );
        }

        THEN( "a mark of whitespace only marks nothing" )
        {
            REQUIRE_FALSE( ruleFromLine( line, line.size() - 2, 2 ) );
        }

        THEN( "an empty line marks nothing" )
        {
            REQUIRE_FALSE( ruleFromLine( QString(), 0, 0 ) );
        }

        THEN( "a mark outside the line marks nothing" )
        {
            REQUIRE_FALSE( ruleFromLine( line, -1, 3 ) );
            REQUIRE_FALSE( ruleFromLine( line, 4, 100 ) );
        }
    }
}

SCENARIO( "A key is suggested from the text before the value", "[rulefromline]" )
{
    THEN( "punctuation around a word is dropped" )
    {
        REQUIRE( suggestedKey( "VIN:" ) == "VIN" );
        REQUIRE( suggestedKey( "[VIN]" ) == "VIN" );
        REQUIRE( suggestedKey( "speed=" ) == "speed" );
        REQUIRE( suggestedKey( "id =" ) == "id" );
    }

    THEN( "dots, dashes and underscores inside a word are kept" )
    {
        REQUIRE( suggestedKey( "app.version:" ) == "app.version" );
        REQUIRE( suggestedKey( "Firmware-Version:" ) == "Firmware-Version" );
        REQUIRE( suggestedKey( "serial_no=" ) == "serial_no" );
        REQUIRE( suggestedKey( "-v." ) == "v" );
    }

    THEN( "of several words the longest is taken, the last one of equal length" )
    {
        REQUIRE( suggestedKey( "[sensor.temp](°C):" ) == "sensor.temp" );
        REQUIRE( suggestedKey( "1\ta:" ) == "a" );
        REQUIRE( suggestedKey( "ab cd:" ) == "cd" );
    }

    THEN( "letters of any script are kept" )
    {
        REQUIRE( suggestedKey( QString::fromUtf8( "Größe:" ) ) == QString::fromUtf8( "Größe" ) );
    }

    THEN( "without a word there is no suggestion" )
    {
        REQUIRE( suggestedKey( "->" ).isEmpty() );
        REQUIRE( suggestedKey( QString() ).isEmpty() );
    }
}
