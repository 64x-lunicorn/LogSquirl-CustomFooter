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
 * @file footerscanner_linenumber_test.cpp
 * @brief BDD tests for the Log Line each found value came from, also across
 *        continued scans of a growing file.
 */

#include <catch2/catch.hpp>

#include "footerentry.h"
#include "footerscanner.h"

#include <QFile>
#include <QTemporaryDir>

using namespace custom_footer;

namespace {

void writeBytes( const QString& path, const QByteArray& content, bool append = false )
{
    QFile file( path );
    REQUIRE( file.open( append ? QIODevice::Append : QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

QList<FooterEntry> vinAndIdRules()
{
    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    entries.append( { "ID", "id=(\\S+)", "", true, {} } );
    return entries;
}

} // namespace

SCENARIO( "FooterScanner tells the Log Line of each value", "[footerscanner][linenumber]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/lines.log";

    GIVEN( "a key matched on several lines" )
    {
        writeBytes( path, "noise\nVIN: FIRST\nVIN: SECOND\n" );
        const auto values = FooterScanner( vinAndIdRules() ).scanFrom( path, {} ).values;

        THEN( "the value and its line are those of the first match, counting from 1" )
        {
            REQUIRE( values[ "VIN" ].value == "FIRST" );
            REQUIRE( values[ "VIN" ].lineNumber == 2 );
        }
    }

    GIVEN( "rules sharing a key, the later rule matching earlier in the file" )
    {
        QList<FooterEntry> entries;
        entries.append( { "Mode", "mode=(\\S+)", "", true, {} } );
        entries.append( { "Mode", "legacy-mode (\\S+)", "", true, {} } );
        const FooterScanner scanner( entries );
        writeBytes( path, "noise\nlegacy-mode 4\nmode=0x04\n" );
        const auto shown = scanner.footerValues( scanner.scanFrom( path, {} ).values );

        THEN( "the shown value has the line of the rule that supplied it" )
        {
            REQUIRE( shown.size() == 1 );
            REQUIRE( shown[ 0 ] == FooterValue{ "Mode", "4", "4", 1, 2 } );
        }
    }

    GIVEN( "rules sharing a key that both match one line, after a line only the second matches" )
    {
        QList<FooterEntry> entries;
        entries.append( { "Build", "rev=(\\S+)", "", true, {} } );
        entries.append( { "Build", "build=(\\S+)", "", true, {} } );
        const FooterScanner scanner( entries );
        writeBytes( path, "noise\nnoise\nbuild=7 rev=abc\nbuild=8\n" );
        const auto shown = scanner.footerValues( scanner.scanFrom( path, {} ).values );

        THEN( "the first rule wins on that line, and the line is that one" )
        {
            REQUIRE( shown.size() == 1 );
            REQUIRE( shown[ 0 ] == FooterValue{ "Build", "abc", "abc", 0, 3 } );
        }
    }
}

SCENARIO( "FooterScanner counts the Log Lines of its values across a continued scan",
          "[footerscanner][resume][linenumber]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/growing.log";
    const FooterScanner scanner( vinAndIdRules() );

    GIVEN( "a scanned file with one of the keys on its second line" )
    {
        writeBytes( path, "starting\nVIN: ABC\n" );
        const auto first = scanner.scanFrom( path, {} );
        REQUIRE( first.values[ "VIN" ].lineNumber == 2 );

        WHEN( "the missing key, and a later match of the found one, are appended" )
        {
            writeBytes( path, "noise\nVIN: LATER\nid=7\n", true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the new value has its line in the whole file, counted on from the first scan" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "ID" ].lineNumber == 5 );
            }

            THEN( "the value kept from the first scan keeps its line" )
            {
                REQUIRE( next.values[ "VIN" ].value == "ABC" );
                REQUIRE( next.values[ "VIN" ].lineNumber == 2 );
            }

            THEN( "a fresh scan finds the same values on the same lines" )
            {
                REQUIRE( scanner.scanFrom( path, {} ).values == next.values );
            }
        }

        WHEN( "the file grows twice" )
        {
            writeBytes( path, "a\nb\n", true );
            const auto second = scanner.scanFrom( path, first.progress );
            REQUIRE( second.resumed );
            writeBytes( path, "c\nid=9\n", true );
            const auto third = scanner.scanFrom( path, second.progress );

            THEN( "the line count is carried across every continued scan" )
            {
                REQUIRE( third.resumed );
                REQUIRE( third.values[ "ID" ].lineNumber == 6 );
                REQUIRE( third.values[ "VIN" ].lineNumber == 2 );
            }
        }
    }

    GIVEN( "a scanned file whose last line has no line break yet" )
    {
        writeBytes( path, "id=1\nnoise\nVIN: PART" );
        const auto first = scanner.scanFrom( path, {} );

        THEN( "the value in that line has the line's number" )
        {
            REQUIRE( first.values[ "VIN" ].lineNumber == 3 );
            REQUIRE( first.values[ "ID" ].lineNumber == 1 );
        }

        WHEN( "the line is completed and more lines follow" )
        {
            writeBytes( path, "IAL\nmore\n", true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the line is read again, with the same number" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "VIN" ].value == "PARTIAL" );
                REQUIRE( next.values[ "VIN" ].lineNumber == 3 );
            }
        }
    }

    GIVEN( "a scanned file with Windows line breaks and an empty line" )
    {
        writeBytes( path, "first\r\n\r\nVIN: CRLF\r\n" );
        const auto first = scanner.scanFrom( path, {} );

        THEN( "a CR LF ends one line, and the empty line counts" )
        {
            REQUIRE( first.values[ "VIN" ].value == "CRLF" );
            REQUIRE( first.values[ "VIN" ].lineNumber == 3 );
        }

        WHEN( "more such lines are appended" )
        {
            writeBytes( path, "\r\nid=5\r\n", true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "they are counted on in the same way" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "ID" ].lineNumber == 5 );
            }
        }
    }

    GIVEN( "a scan with a line limit, below the limit so far" )
    {
        writeBytes( path, "one\ntwo\n" );
        const auto first = scanner.scanFrom( path, {}, 3 );
        REQUIRE_FALSE( first.progress.done );

        WHEN( "two matching lines are appended, the second beyond the limit" )
        {
            writeBytes( path, "id=3\nVIN: FOUR\n", true );
            const auto next = scanner.scanFrom( path, first.progress, 3 );

            THEN( "the line within the limit supplies a value with its number, the other none" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "ID" ].lineNumber == 3 );
                REQUIRE_FALSE( next.values.contains( "VIN" ) );
                REQUIRE( next.progress.done );
            }
        }
    }

    GIVEN( "a scanned file that is truncated and written again" )
    {
        writeBytes( path, "a\nb\nVIN: OLD\nid=1\n" );
        const auto first = scanner.scanFrom( path, {} );
        REQUIRE( first.values[ "VIN" ].lineNumber == 3 );
        writeBytes( path, "VIN: NEW\n" );

        WHEN( "it is scanned again" )
        {
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the line comes from the new scan from the start" )
            {
                REQUIRE_FALSE( next.resumed );
                REQUIRE( next.values[ "VIN" ].lineNumber == 1 );
            }
        }
    }

    GIVEN( "a scanned file that is replaced by a larger one" )
    {
        writeBytes( path, "VIN: OLD\n" );
        const auto first = scanner.scanFrom( path, {} );
        REQUIRE( QFile::remove( path ) );
        writeBytes( path, "rotated\nid=2\nnoise\nnoise\nVIN: NEW\n" );

        WHEN( "it is scanned again" )
        {
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the lines come from the new scan from the start, not counted on" )
            {
                REQUIRE_FALSE( next.resumed );
                REQUIRE( next.values[ "ID" ].lineNumber == 2 );
                REQUIRE( next.values[ "VIN" ].lineNumber == 5 );
            }
        }
    }
}

SCENARIO( "FooterScanner counts the line the scan limit stopped in, across a continued scan",
          "[footerscanner][resume][linenumber]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/huge.log";
    const FooterScanner scanner( vinAndIdRules() );

    GIVEN( "a scanned file of long lines up to shortly before the scan limit" )
    {
        // Lines of 1 MiB; once scanned, one that crosses the limit follows.
        const QByteArray line = QByteArray( 1024 * 1024 - 1, 'x' ) + '\n';
        int lines = 0;
        {
            QFile file( path );
            REQUIRE( file.open( QIODevice::WriteOnly ) );
            for ( qint64 written = 0;
                  written + line.size() < FooterScanner::kMaxScanBytes - 4 * 1024 * 1024;
                  written += line.size() ) {
                REQUIRE( file.write( line ) == line.size() );
                ++lines;
            }
        }
        const auto first = scanner.scanFrom( path, {}, 0 );
        REQUIRE_FALSE( first.progress.done );
        REQUIRE( first.progress.lines == lines );

        WHEN( "a line with a value at its start and crossing the limit is appended" )
        {
            {
                QFile file( path );
                REQUIRE( file.open( QIODevice::Append ) );
                file.write( "VIN: EARLY " );
                const QByteArray chunk( 1024 * 1024, 'y' );
                for ( int i = 0; i < 8; ++i ) {
                    REQUIRE( file.write( chunk ) == chunk.size() );
                }
                file.write( "\nid=late\n" );
            }
            const auto next = scanner.scanFrom( path, first.progress, 0 );

            THEN( "the value has the number of that line, and nothing after it is read" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.progress.done );
                REQUIRE( next.values[ "VIN" ].value == "EARLY" );
                REQUIRE( next.values[ "VIN" ].lineNumber == lines + 1 );
                REQUIRE_FALSE( next.values.contains( "ID" ) );
            }
        }
    }
}
