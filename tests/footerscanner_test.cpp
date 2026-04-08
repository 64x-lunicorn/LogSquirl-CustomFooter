/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file footerscanner_test.cpp
 * @brief BDD tests for FooterScanner::scan().
 */

#include <catch2/catch.hpp>

#include "footerentry.h"
#include "footerscanner.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace costume_footer;

/// Helper: write lines to a temp file and return its path.
static QString writeTempFile( const QTemporaryDir& dir, const QStringList& lines )
{
    const QString path = dir.path() + "/test.log";
    QFile file( path );
    file.open( QIODevice::WriteOnly | QIODevice::Text );
    QTextStream out( &file );
    for ( const auto& line : lines ) {
        out << line << "\n";
    }
    return path;
}

SCENARIO( "FooterScanner extracts values from log lines", "[footerscanner]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a log file with VIN and timestamp lines" )
    {
        const QStringList lines = { "2024-01-01 INFO Starting up",
                                    "VIN: WVWZZZ1JZWW123456",
                                    "2024-01-01 WARN Low memory",
                                    "Timestamp: 1704067200" };

        const auto filePath = writeTempFile( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", true } );
        entries.append( { "Timestamp", "Timestamp:\\s+(\\d+)", true } );

        WHEN( "scanning with both entries enabled" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "both values are extracted" )
            {
                REQUIRE( results.size() == 2 );
                REQUIRE( results[ "VIN" ] == "WVWZZZ1JZWW123456" );
                REQUIRE( results[ "Timestamp" ] == "1704067200" );
            }
        }

        WHEN( "one entry is disabled" )
        {
            entries[ 1 ].enabled = false;
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "only the enabled entry is returned" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results.contains( "VIN" ) );
                REQUIRE_FALSE( results.contains( "Timestamp" ) );
            }
        }
    }

    GIVEN( "a log file where a pattern has no capturing group" )
    {
        const QStringList lines = { "ERROR something went wrong",
                                    "OK all clear" };
        const auto filePath = writeTempFile( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "Status", "^(ERROR|OK)", true } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the first capturing group is used" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "Status" ] == "ERROR" );
            }
        }
    }

    GIVEN( "an empty entry list" )
    {
        const QStringList lines = { "some line" };
        const auto filePath = writeTempFile( tmpDir, lines );

        WHEN( "scanning with no entries" )
        {
            const auto results = FooterScanner::scan( filePath, {} );

            THEN( "result is empty" )
            {
                REQUIRE( results.isEmpty() );
            }
        }
    }

    GIVEN( "a maxLines limit smaller than the file" )
    {
        const QStringList lines = { "Line 1",
                                    "Line 2",
                                    "VIN: ABC123" };
        const auto filePath = writeTempFile( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", true } );

        WHEN( "maxLines is 2 (match is on line 3)" )
        {
            const auto results = FooterScanner::scan( filePath, entries, 2 );

            THEN( "the match on line 3 is not found" )
            {
                REQUIRE( results.isEmpty() );
            }
        }

        WHEN( "maxLines is 3 (match is on line 3)" )
        {
            const auto results = FooterScanner::scan( filePath, entries, 3 );

            THEN( "the match is found" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "VIN" ] == "ABC123" );
            }
        }
    }
}
