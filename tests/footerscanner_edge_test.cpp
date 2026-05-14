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
 * @file footerscanner_edge_test.cpp
 * @brief Additional edge-case tests for FooterScanner::scan().
 */

#include <catch2/catch.hpp>

#include "footerentry.h"
#include "footerscanner.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace costume_footer;

/// Helper: write lines to a temp file and return its path.
static QString writeTempLines( const QTemporaryDir& dir, const QStringList& lines,
                               const QString& name = "test.log" )
{
    const QString path = dir.path() + "/" + name;
    QFile file( path );
    if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
        return {};
    }
    QTextStream out( &file );
    for ( const auto& line : lines ) {
        out << line << "\n";
    }
    return path;
}

SCENARIO( "FooterScanner handles invalid regex gracefully", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "an entry with an invalid linePattern regex" )
    {
        const QStringList lines = { "some log line" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "Bad", "[unclosed bracket", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the invalid entry is silently skipped" )
            {
                REQUIRE( results.isEmpty() );
            }
        }
    }

    GIVEN( "an entry with a valid linePattern but invalid valuePattern" )
    {
        const QStringList lines = { "VIN: ABC123" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:", "[bad(regex", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "it falls back to using linePattern capture" )
            {
                REQUIRE( results.size() == 1 );
                // With no capture group in linePattern "VIN:", full match is used
                REQUIRE( results[ "VIN" ] == "VIN:" );
            }
        }
    }
}

SCENARIO( "FooterScanner handles empty and missing files", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a non-existent file path" )
    {
        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results
                = FooterScanner::scan( tmpDir.path() + "/nofile.log", entries );

            THEN( "result is empty" )
            {
                REQUIRE( results.isEmpty() );
            }
        }
    }

    GIVEN( "an empty file" )
    {
        const auto filePath = writeTempLines( tmpDir, {}, "empty.log" );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "result is empty" )
            {
                REQUIRE( results.isEmpty() );
            }
        }
    }

    GIVEN( "an empty file path string" )
    {
        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

        WHEN( "scanning with empty path" )
        {
            const auto results = FooterScanner::scan( QString(), entries );

            THEN( "result is empty" )
            {
                REQUIRE( results.isEmpty() );
            }
        }
    }
}

SCENARIO( "FooterScanner with maxLines=0 means unlimited", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a file with a match on line 5" )
    {
        QStringList lines;
        for ( int i = 0; i < 10; ++i ) {
            lines.append( QString( "line %1" ).arg( i ) );
        }
        lines[ 4 ] = "VIN: MATCH123";
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

        WHEN( "scanning with maxLines=0" )
        {
            const auto results = FooterScanner::scan( filePath, entries, 0 );

            THEN( "all lines are scanned (unlimited)" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "VIN" ] == "MATCH123" );
            }
        }
    }
}

SCENARIO( "FooterScanner with duplicate keys uses first match", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a file with multiple lines matching the same pattern" )
    {
        const QStringList lines = { "VIN: FIRST", "VIN: SECOND", "VIN: THIRD" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the first match is used" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "VIN" ] == "FIRST" );
            }
        }
    }
}

SCENARIO( "FooterScanner all entries disabled returns empty", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a file and all entries disabled" )
    {
        const QStringList lines = { "VIN: ABC123" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", false, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "result is empty" )
            {
                REQUIRE( results.isEmpty() );
            }
        }
    }
}

SCENARIO( "FooterScanner entry with empty linePattern is skipped", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "an entry with an empty linePattern" )
    {
        const QStringList lines = { "VIN: ABC123" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "Empty", "", "", true, {} } );
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the empty-pattern entry is skipped, the valid one matches" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results.contains( "VIN" ) );
                REQUIRE_FALSE( results.contains( "Empty" ) );
            }
        }
    }
}
