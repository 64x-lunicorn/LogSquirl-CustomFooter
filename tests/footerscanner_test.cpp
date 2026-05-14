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
    if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
        return {};
    }
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
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
        entries.append( { "Timestamp", "Timestamp:\\s+(\\d+)", "", true, {} } );

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
        entries.append( { "Status", "^(ERROR|OK)", "", true, {} } );

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
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

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

SCENARIO( "FooterScanner supports two-stage matching", "[footerscanner]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a log line containing a key-value pair" )
    {
        const QStringList lines = { "config: isComponentProtectionEnabled: false",
                                    "config: maxRetries: 5" };
        const auto filePath = writeTempFile( tmpDir, lines );

        WHEN( "using a value pattern to extract from the matched line" )
        {
            QList<FooterEntry> entries;
            entries.append(
                { "Component Protection", "isComponentProtectionEnabled",
                  ":\\s+(\\S+)$", true, {} } );

            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the value pattern extracts from the found line" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "Component Protection" ] == "false" );
            }
        }

        WHEN( "using only a line pattern without a value pattern" )
        {
            QList<FooterEntry> entries;
            entries.append(
                { "Retries", "maxRetries:\\s+(\\d+)", "", true, {} } );

            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the line pattern capturing group is used" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "Retries" ] == "5" );
            }
        }
    }
}

SCENARIO( "FooterScanner applies value mappings", "[footerscanner]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a log line with a boolean value and mappings defined" )
    {
        const QStringList lines = { "isComponentProtectionEnabled: false" };
        const auto filePath = writeTempFile( tmpDir, lines );

        QList<ValueMapping> mappings;
        mappings.append( ValueMapping{ "true", "Enabled" } );
        mappings.append( ValueMapping{ "false", "Disabled" } );

        QList<FooterEntry> entries;
        entries.append( { "Protection", "isComponentProtectionEnabled:\\s+(\\S+)",
                          "", true, mappings } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the extracted value is mapped to the display value" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "Protection" ] == "Disabled" );
            }
        }
    }

    GIVEN( "a value that does not match any mapping" )
    {
        const QStringList lines = { "status: unknown" };
        const auto filePath = writeTempFile( tmpDir, lines );

        QList<ValueMapping> mappings;
        mappings.append( ValueMapping{ "true", "Enabled" } );
        mappings.append( ValueMapping{ "false", "Disabled" } );

        QList<FooterEntry> entries;
        entries.append( { "Status", "status:\\s+(\\S+)", "", true, mappings } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the raw extracted value is returned" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "Status" ] == "unknown" );
            }
        }
    }

    GIVEN( "two-stage matching combined with value mappings" )
    {
        const QStringList lines = { "config isComponentProtectionEnabled: true" };
        const auto filePath = writeTempFile( tmpDir, lines );

        QList<ValueMapping> mappings;
        mappings.append( ValueMapping{ "true", "Active" } );
        mappings.append( ValueMapping{ "false", "Inactive" } );

        QList<FooterEntry> entries;
        entries.append( { "Protection", "isComponentProtectionEnabled",
                          ":\\s+(\\S+)$", true, mappings } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the value is extracted and mapped" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "Protection" ] == "Active" );
            }
        }
    }
}
