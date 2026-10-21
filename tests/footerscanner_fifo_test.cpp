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
 * @file footerscanner_fifo_test.cpp
 * @brief BDD tests for an active file that is not a regular file: a FIFO,
 *        which blocks whoever opens it to read, is never opened. POSIX only;
 *        skipped where there are no FIFOs.
 */

#include <catch2/catch.hpp>

#include "fifo.h"
#include "footerscanner.h"

#include <QFile>
#include <QTemporaryDir>

using namespace custom_footer;

#if defined( Q_OS_UNIX )

namespace {

void writeFile( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

const QList<FooterEntry> kEntries{ { "VIN", "VIN:\\s+(\\S+)", "", true, {} } };

} // namespace

SCENARIO( "The scanner reads only regular files", "[footerscanner][fifo]" )
{
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto fifo = dir.path() + "/fifo.log";
    REQUIRE( makeFifo( fifo ) );
    const FooterScanner scanner( kEntries );

    GIVEN( "a FIFO as the file, which no one writes to" )
    {
        // Opening it to read would block until a writer comes.
        FifoWatchdog watchdog( fifo );

        WHEN( "it is scanned" )
        {
            const auto shown = scanner.scanFile( fifo );
            const auto scan = scanner.scanFrom( fifo, {} );

            THEN( "the scan finishes without opening it, with no values" )
            {
                REQUIRE_FALSE( watchdog.fired() );
                REQUIRE( shown.isEmpty() );
                REQUIRE( scan.values.isEmpty() );
                REQUIRE( scan.progress.offset == 0 );
            }
        }

        WHEN( "it is previewed" )
        {
            const auto preview = FooterScanner::preview( fifo, kEntries, 0 );

            THEN( "the preview finishes without opening it, as a file that cannot be read" )
            {
                REQUIRE_FALSE( watchdog.fired() );
                REQUIRE( preview.status == FooterScanner::Preview::Status::Unreadable );
                REQUIRE( preview.filePath == fifo );
            }
        }

        WHEN( "its encoding is told" )
        {
            const auto encoding = FooterScanner::Encoding::ofFile( fifo );

            THEN( "it is UTF-8, as for any file that cannot be read" )
            {
                REQUIRE_FALSE( watchdog.fired() );
                REQUIRE( encoding == FooterScanner::Encoding() );
            }
        }

        WHEN( "it is reached through a symbolic link" )
        {
            const auto link = dir.path() + "/link.log";
            REQUIRE( QFile::link( fifo, link ) );
            const auto shown = scanner.scanFile( link );
            const auto preview = FooterScanner::preview( link, kEntries, 0 );

            THEN( "it is not opened either" )
            {
                REQUIRE_FALSE( watchdog.fired() );
                REQUIRE( shown.isEmpty() );
                REQUIRE( preview.status == FooterScanner::Preview::Status::Unreadable );
            }
        }
    }

    GIVEN( "a regular file scanned and previewed, then replaced by a FIFO" )
    {
        const auto path = dir.path() + "/replaced.log";
        writeFile( path, "VIN: ABC\nmore\n" );
        const auto scan = scanner.scanFrom( path, {} );
        const auto preview = FooterScanner::preview( path, kEntries, 0 );
        REQUIRE( scan.values.value( "VIN" ).value == "ABC" );
        REQUIRE( preview.status == FooterScanner::Preview::Status::Scanned );
        REQUIRE( QFile::remove( path ) );
        REQUIRE( makeFifo( path ) );
        FifoWatchdog watchdog( path );

        WHEN( "it is asked whether the change can alter them" )
        {
            const bool scanUnchanged = FooterScanner::fileUnchangedFor( path, scan );
            const bool previewUnchanged = FooterScanner::fileUnchangedFor( path, preview );

            THEN( "it can, and the FIFO was not opened to tell" )
            {
                REQUIRE_FALSE( watchdog.fired() );
                REQUIRE_FALSE( scanUnchanged );
                REQUIRE_FALSE( previewUnchanged );
            }
        }

        WHEN( "it is scanned on from the earlier scan" )
        {
            const auto rescan = scanner.scanFrom( path, scan.progress );

            THEN( "nothing is kept from the file it replaced" )
            {
                REQUIRE_FALSE( watchdog.fired() );
                REQUIRE( rescan.values.isEmpty() );
            }
        }
    }

    GIVEN( "a symbolic link to a regular file" )
    {
        const auto path = dir.path() + "/regular.log";
        const auto link = dir.path() + "/link.log";
        writeFile( path, "VIN: LINKED\n" );
        REQUIRE( QFile::link( path, link ) );

        THEN( "the file it points to is scanned and previewed" )
        {
            REQUIRE( scanner.scanFile( link ).value( "VIN" ) == "LINKED" );
            REQUIRE( FooterScanner::preview( link, kEntries, 0 ).status
                     == FooterScanner::Preview::Status::Scanned );
        }
    }
}

#else

TEST_CASE( "The scanner reads only regular files", "[footerscanner][fifo]" )
{
    WARN( "Skipped: no FIFOs on this platform." );
}

#endif
