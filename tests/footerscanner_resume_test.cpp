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
 * @file footerscanner_resume_test.cpp
 * @brief BDD tests for continuing a scan where the previous one stopped.
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

/// Overwrite bytes in place, keeping the file's size.
void overwriteAt( const QString& path, qint64 offset, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::ReadWrite ) );
    REQUIRE( file.seek( offset ) );
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

SCENARIO( "FooterScanner continues a scan when the file grows", "[footerscanner][resume]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/growing.log";
    const FooterScanner scanner( vinAndIdRules() );

    GIVEN( "a scanned file that has only one of the keys" )
    {
        writeBytes( path, "starting\nVIN: ABC\n" );
        const auto first = scanner.scanFrom( path, {} );
        REQUIRE_FALSE( first.resumed );
        REQUIRE( first.values.size() == 1 );
        REQUIRE( first.progress.lines == 2 );
        REQUIRE( first.progress.offset == 18 );
        REQUIRE_FALSE( first.progress.done );

        WHEN( "a line with the missing key is appended" )
        {
            writeBytes( path, "id=7\n", true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "only the new line is scanned and both values are known" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.progress.lines == 3 );
                REQUIRE( next.values[ "VIN" ] == "ABC" );
                REQUIRE( next.values[ "ID" ] == "7" );
                REQUIRE( next.progress.done );
            }
        }

        WHEN( "nothing changed" )
        {
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the known values are kept" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values == first.values );
                REQUIRE( next.progress.lines == 2 );
            }
        }
    }

    GIVEN( "a scanned file with filler lines" )
    {
        const QByteArray filler( "filler filler filler filler filler\n" );
        writeBytes( path, "VIN: ABC\n" + filler.repeated( 30 ) );
        const auto first = scanner.scanFrom( path, {} );
        REQUIRE( first.values.size() == 1 );

        WHEN( "a line already scanned changes in place and a line is appended" )
        {
            // Well inside the file: neither its start nor its end.
            overwriteAt( path, 9 + filler.size() * 15, "id=999" );
            writeBytes( path, "id=1\n", true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "only the appended bytes are scanned" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "ID" ] == "1" );
            }

            THEN( "a fresh scan would find the changed line" )
            {
                REQUIRE( scanner.scanFile( path )[ "ID" ] == "999" );
            }
        }
    }

    GIVEN( "a file whose last line is still being written" )
    {
        writeBytes( path, "id=1\nVIN: PART" );
        const auto first = scanner.scanFrom( path, {} );

        THEN( "the partial line is matched but not remembered" )
        {
            REQUIRE( first.values[ "VIN" ] == "PART" );
            REQUIRE_FALSE( first.progress.values.contains( "VIN" ) );
            REQUIRE( first.progress.lines == 1 );
            REQUIRE( first.progress.offset == 5 );
        }

        WHEN( "the line is completed" )
        {
            writeBytes( path, "IAL\n", true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the complete value is found" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "VIN" ] == "PARTIAL" );
                REQUIRE( next.progress.done );
            }
        }
    }

    GIVEN( "a scan that reached the line limit" )
    {
        writeBytes( path, "one\ntwo\n" );
        const auto first = scanner.scanFrom( path, {}, 2 );
        REQUIRE( first.progress.done );

        WHEN( "matching lines are appended" )
        {
            writeBytes( path, "VIN: LATE\nid=2\n", true );
            const auto next = scanner.scanFrom( path, first.progress, 2 );

            THEN( "they are beyond the limit and not scanned" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values.isEmpty() );
            }
        }
    }
}

SCENARIO( "FooterScanner starts over when the file was replaced", "[footerscanner][resume]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/rotated.log";
    const FooterScanner scanner( vinAndIdRules() );

    GIVEN( "a scanned file" )
    {
        writeBytes( path, "VIN: OLD-VALUE\nid=old\n" );
        const auto first = scanner.scanFrom( path, {} );
        REQUIRE( first.progress.done );

        WHEN( "it is truncated and written again" )
        {
            writeBytes( path, "VIN: NEW\n" );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "it is scanned from the start" )
            {
                REQUIRE_FALSE( next.resumed );
                REQUIRE( next.values[ "VIN" ] == "NEW" );
                REQUIRE_FALSE( next.values.contains( "ID" ) );
            }
        }

        WHEN( "it is replaced by a larger file with other content" )
        {
            REQUIRE( QFile::remove( path ) );
            writeBytes( path, "VIN: REPLACED\nid=new\nand some more lines\n" );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "it is scanned from the start" )
            {
                REQUIRE_FALSE( next.resumed );
                REQUIRE( next.values[ "VIN" ] == "REPLACED" );
                REQUIRE( next.values[ "ID" ] == "new" );
            }
        }

        WHEN( "it disappears" )
        {
            REQUIRE( QFile::remove( path ) );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "nothing is found and the progress is reset" )
            {
                REQUIRE( next.values.isEmpty() );
                REQUIRE( next.progress.offset == 0 );
            }
        }
    }
}
