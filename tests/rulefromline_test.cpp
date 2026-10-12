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

#include <QElapsedTimer>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QtGlobal>

#include <optional>

using namespace custom_footer;

namespace {

/// The rule made from @p line with @p value marked: its first occurrence
/// at or after @p from.
RuleFromLine marking( const QString& line, const QString& value, int from = 0 )
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
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "VIN:" );
                REQUIRE( made.rule.valueEnd == ValueEnd::Whitespace );
                REQUIRE( made.key == "VIN" );
                REQUIRE( made.value == "WDB1234567" );
            }

            THEN( "the rule finds the VIN in the line, as a simple rule" )
            {
                REQUIRE( extract( made, line ) == QString( "WDB1234567" ) );
                FooterEntry entry;
                applySimpleRule( made.rule, entry );
                REQUIRE( simpleRuleOf( entry ) == made.rule );
            }
        }

        WHEN( "the user clicks where the VIN starts" )
        {
            const auto made = ruleFromLine( line, line.indexOf( "WDB" ), 0 );

            THEN( "the value is the word starting there" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.value == "WDB1234567" );
                REQUIRE( made.rule.textBefore == "VIN:" );
                REQUIRE( made.rule.valueEnd == ValueEnd::Whitespace );
            }
        }

        WHEN( "the user clicks into the middle of the VIN, or into its first letter" )
        {
            const auto inMiddle = ruleFromLine( line, line.indexOf( "WDB" ) + 4, 0 );
            const auto inFirst = ruleFromLine( line, line.indexOf( "WDB" ) + 1, 0 );

            THEN( "the value is the whole word the click lands in" )
            {
                REQUIRE( inMiddle.isRule() );
                REQUIRE( inMiddle.value == "WDB1234567" );
                REQUIRE( inMiddle.rule.textBefore == "VIN:" );
                REQUIRE( inFirst.value == "WDB1234567" );
                REQUIRE( inFirst.rule.textBefore == "VIN:" );
            }
        }

        WHEN( "the user clicks into the space before the VIN" )
        {
            const auto made = ruleFromLine( line, line.indexOf( "VIN:" ) + 4, 0 );

            THEN( "the value is the next word" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.value == "WDB1234567" );
                REQUIRE( made.rule.textBefore == "VIN:" );
            }
        }

        WHEN( "the mark takes the spaces around the VIN too" )
        {
            const auto made = marking( line, " WDB1234567 " );

            THEN( "the spaces are not part of the value" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.value == "WDB1234567" );
                REQUIRE( made.rule.textBefore == "VIN:" );
                REQUIRE( made.rule.valueEnd == ValueEnd::Whitespace );
            }
        }

        WHEN( "the speed after its equals sign is marked" )
        {
            const auto made = marking( line, "5", line.indexOf( "speed" ) );

            THEN( "the text before it is speed=, and the key speed" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "speed=" );
                REQUIRE( made.key == "speed" );
                REQUIRE( extract( made, line ) == QString( "5" ) );
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
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "[sensor.temp](°C)*+?^$|{2}\\:" );
                REQUIRE( made.key == "sensor.temp" );
                REQUIRE( extract( made, line ) == QString( "21.5" ) );
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
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "id =" );
                REQUIRE( made.key == "id" );
                REQUIRE( extract( made, line ) == QString( "42" ) );
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
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "Wert:" );
                REQUIRE( made.rule.valueEnd == ValueEnd::EndOfLine );
                REQUIRE( made.key == "Wert" );
                REQUIRE( extract( made, line ) == QString::fromUtf8( "12 m²" ) );
            }
        }

        WHEN( "the word with umlauts is the text before a value" )
        {
            const QString other = QString::fromUtf8( "Größe: 3" );
            const auto made = marking( other, "3" );

            THEN( "it is the key" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.key == QString::fromUtf8( "Größe" ) );
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
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "VIN:" );
                REQUIRE( made.rule.valueEnd == ValueEnd::Whitespace );
                REQUIRE( extract( made, line ) == QString( "WDB1" ) );
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
                REQUIRE( made.isRule() );
                REQUIRE( made.value == "John Smith" );
                REQUIRE( made.rule.textBefore == "Owner:" );
                REQUIRE( made.rule.valueEnd == ValueEnd::EndOfLine );
                REQUIRE( extract( made, line ) == QString( "John Smith" ) );
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
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.valueEnd == ValueEnd::Character );
                REQUIRE( made.rule.endCharacter == "," );
                REQUIRE( extract( made, line ) == QString( "5" ) );
            }
        }

        WHEN( "a value with a space in it before a semicolon is marked" )
        {
            const QString line = "msg=hello world; next=1";
            const auto made = marking( line, "hello world" );

            THEN( "the value ends at the semicolon" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "msg=" );
                REQUIRE( made.rule.valueEnd == ValueEnd::Character );
                REQUIRE( made.rule.endCharacter == ";" );
                REQUIRE( extract( made, line ) == QString( "hello world" ) );
            }
        }
    }

    GIVEN( "the text before the value also earlier in the line" )
    {
        const QString line = "08:15 a: 1 a: 2 a: 3";

        WHEN( "the last value is marked" )
        {
            const auto made = marking( line, "3" );

            THEN( "no rule is made, as it would find the first value; the problem says so" )
            {
                REQUIRE_FALSE( made.isRule() );
                REQUIRE( made.problem == MarkProblem::TextBeforeEarlier );
                REQUIRE( made.rule.textBefore == "a:" );
                REQUIRE( made.value == "3" );
            }
        }

        WHEN( "the first value is marked" )
        {
            const auto made = marking( line, "1", line.indexOf( "a:" ) );

            THEN( "the rule finds it" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "a:" );
                REQUIRE( extract( made, line ) == QString( "1" ) );
            }
        }
    }

    GIVEN( "marks that make no rule" )
    {
        const QString line = "WDB123 is the VIN  ";
        struct Mark {
            const char* what;
            qsizetype start;
            qsizetype length;
            MarkProblem problem;
        };
        const QList<Mark> marks{
            { "a value at the start of the line", 0, 6, MarkProblem::NoTextBefore },
            { "a click into the whitespace at the end", line.size() - 1, 0,
              MarkProblem::NothingMarked },
            { "a click after the end", line.size(), 0, MarkProblem::NothingMarked },
            { "whitespace only", line.size() - 2, 2, MarkProblem::NothingMarked },
            { "a mark starting before the line", -1, 3, MarkProblem::NothingMarked },
            { "a mark reaching past the line", 4, 100, MarkProblem::NothingMarked },
        };

        WHEN( "each is made into a rule" )
        {
            QList<RuleFromLine> made;
            for ( const auto& mark : marks ) {
                made.append( ruleFromLine( line, mark.start, mark.length ) );
            }
            const auto empty = ruleFromLine( QString(), 0, 0 );

            THEN( "none makes a rule, and each says why" )
            {
                for ( qsizetype i = 0; i < marks.size(); ++i ) {
                    INFO( marks.at( i ).what );
                    REQUIRE_FALSE( made.at( i ).isRule() );
                    REQUIRE( made.at( i ).problem == marks.at( i ).problem );
                }
                REQUIRE( empty.problem == MarkProblem::NothingMarked );
            }
        }
    }

    GIVEN( "a mark starting on a combining mark or inside a surrogate pair" )
    {
        // "e" and a combining acute accent; an emoji of two UTF-16 units.
        const QString line = QString::fromUtf8( "k: e\xCC\x81t\xF0\x9F\x98\x80 x" );

        WHEN( "the mark starts on the accent, and on the emoji's second unit" )
        {
            // Marks, not clicks: a click takes the whole word it lands in.
            const auto onAccent = ruleFromLine( line, line.indexOf( "e" ) + 1, 2 );
            const auto inEmoji = ruleFromLine( line, line.indexOf( "t" ) + 2, 1 );

            THEN( "it starts at the character the accent belongs to, and at the emoji" )
            {
                REQUIRE( onAccent.isRule() );
                REQUIRE( onAccent.value == QString::fromUtf8( "e\xCC\x81t" ) );
                REQUIRE( onAccent.rule.textBefore == "k:" );
                REQUIRE( inEmoji.isRule() );
                REQUIRE( inEmoji.value == QString::fromUtf8( "\xF0\x9F\x98\x80" ) );
                REQUIRE( inEmoji.rule.textBefore == QString::fromUtf8( "e\xCC\x81t" ) );
            }
        }
    }
}

namespace {

/// Counts the warnings Qt logs while it lives, e.g. of an invalid
/// QRegularExpression used for matching.
class WarningCounter {
public:
    WarningCounter()
    {
        counter() = 0;
        previous_ = qInstallMessageHandler( &count );
    }
    ~WarningCounter()
    {
        qInstallMessageHandler( previous_ );
    }
    WarningCounter( const WarningCounter& ) = delete;
    WarningCounter& operator=( const WarningCounter& ) = delete;

    int warnings() const
    {
        return counter();
    }

private:
    static int& counter()
    {
        static int warnings = 0;
        return warnings;
    }
    static void count( QtMsgType type, const QMessageLogContext&, const QString& )
    {
        if ( type != QtDebugMsg && type != QtInfoMsg ) {
            ++counter();
        }
    }

    QtMessageHandler previous_ = nullptr;
};

/// A line of about 1 MiB, as long as the host hands over.
constexpr qsizetype kMiB = 1024 * 1024;

} // namespace

SCENARIO( "A rule is made from a mark in a very long line in bounded time", "[rulefromline][long]" )
{
    GIVEN( "a 1 MiB line of the same text before many values" )
    {
        const QString line = QString( "a: 1 " ).repeated( kMiB / 5 ) + "a: 2";

        WHEN( "the last value is marked, and a value within the footer's line limit clicked" )
        {
            WarningCounter counter;
            QElapsedTimer timer;
            timer.start();
            const auto marked = marking( line, "2", line.size() - 1 );
            const auto clicked = ruleFromLine( line, 5 * 1000 + 3, 0 );
            const auto elapsed = timer.elapsed();

            THEN( "no rule is made, as the footer reads only the start of a long line, and as "
                  "the text before is not unique, quickly and without warnings" )
            {
                REQUIRE( marked.problem == MarkProblem::BeyondLineLimit );
                REQUIRE( clicked.problem == MarkProblem::TextBeforeEarlier );
                REQUIRE( clicked.value == "1" );
                REQUIRE( elapsed < 2000 );
                REQUIRE( counter.warnings() == 0 );
            }
        }
    }

    GIVEN( "a 1 MiB line of one word, before a value" )
    {
        const QString line = QString( kMiB, QLatin1Char( 'x' ) ) + ": 5";

        WHEN( "the value is marked" )
        {
            WarningCounter counter;
            QElapsedTimer timer;
            timer.start();
            const auto made = marking( line, "5", line.size() - 1 );
            const auto elapsed = timer.elapsed();

            THEN( "the footer would not see it, past its line limit, and that is said, quickly "
                  "and without warnings" )
            {
                REQUIRE( made.problem == MarkProblem::BeyondLineLimit );
                REQUIRE( elapsed < 2000 );
                REQUIRE( counter.warnings() == 0 );
            }
        }
    }

    GIVEN( "a value early in a 1 MiB line" )
    {
        const QString line = "VIN: WDB1 " + QString( "filler " ).repeated( kMiB / 7 );

        WHEN( "it is marked" )
        {
            WarningCounter counter;
            QElapsedTimer timer;
            timer.start();
            const auto made = marking( line, "WDB1" );
            const auto elapsed = timer.elapsed();

            THEN( "the rule is made, quickly and without warnings" )
            {
                REQUIRE( made.isRule() );
                REQUIRE( made.rule.textBefore == "VIN:" );
                REQUIRE( elapsed < 2000 );
                REQUIRE( counter.warnings() == 0 );
            }
        }
    }
}

SCENARIO( "The part of a long line the footer reads ends at a character", "[rulefromline]" )
{
    GIVEN( "lines around the footer's 64 KiB line limit" )
    {
        const QString shortLine = "VIN: WDB1";
        const QString exact( 64 * 1024, QLatin1Char( 'x' ) );
        // An emoji of four UTF-8 bytes across the limit.
        const QString emojiAcross = QString( 64 * 1024 - 2, QLatin1Char( 'x' ) )
                                    + QString::fromUtf8( "\xF0\x9F\x98\x80" );

        WHEN( "the part the footer reads is measured" )
        {
            const auto ofShort = footerReadLength( shortLine );
            const auto ofExact = footerReadLength( exact + "y" );
            const auto ofEmoji = footerReadLength( emojiAcross );

            THEN( "a short line is read whole, a long one up to the limit, never half a "
                  "character" )
            {
                REQUIRE( ofShort == shortLine.size() );
                REQUIRE( ofExact == exact.size() );
                REQUIRE( ofEmoji == 64 * 1024 - 2 );
            }
        }
    }
}

SCENARIO( "A key is suggested from the text before the value", "[rulefromline]" )
{
    GIVEN( "texts before values" )
    {
        const QList<QPair<QString, QString>> cases{
            // Punctuation around a word is dropped.
            { "VIN:", "VIN" },
            { "[VIN]", "VIN" },
            { "speed=", "speed" },
            { "id =", "id" },
            // Dots, dashes and underscores inside a word are kept.
            { "app.version:", "app.version" },
            { "Firmware-Version:", "Firmware-Version" },
            { "serial_no=", "serial_no" },
            { "-v.", "v" },
            // Of several words the longest, the last one of equal length.
            { QString::fromUtf8( "[sensor.temp](°C):" ), "sensor.temp" },
            { "1\ta:", "a" },
            { "ab cd:", "cd" },
            // Letters of any script, also outside the BMP.
            { QString::fromUtf8( "Größe:" ), QString::fromUtf8( "Größe" ) },
            { QString::fromUtf8( "\xF0\x9D\x90\x80x:" ), QString::fromUtf8( "\xF0\x9D\x90\x80x" ) },
            // Without a word there is no suggestion.
            { "->", "" },
            { "", "" },
        };

        WHEN( "a key is suggested for each" )
        {
            QStringList keys;
            for ( const auto& [ text, expected ] : cases ) {
                keys.append( suggestedKey( text ) );
            }

            THEN( "it is the expected one" )
            {
                for ( qsizetype i = 0; i < cases.size(); ++i ) {
                    INFO( cases.at( i ).first.toStdString() );
                    REQUIRE( keys.at( i ) == cases.at( i ).second );
                }
            }
        }
    }
}
