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
 * @file footerscanner_edge_test.cpp
 * @brief Additional edge-case tests for FooterScanner::scan().
 */

#include <catch2/catch.hpp>

#include "footerentry.h"
#include "footerscanner.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace custom_footer;

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

            THEN( "the entry is skipped instead of falling back to the line pattern" )
            {
                REQUIRE( results.isEmpty() );
            }
        }
    }

    GIVEN( "rules with invalid patterns next to a valid one" )
    {
        const QStringList lines = { "VIN: ABC123" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "Bad", "[unclosed bracket", "", true, {} } );
        entries.append( { "Worse", "VIN:", "(", true, {} } );
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

        WHEN( "compiling the rules" )
        {
            const FooterScanner scanner( entries );

            THEN( "each invalid rule is reported by key" )
            {
                REQUIRE( scanner.problems().size() == 2 );
                REQUIRE( scanner.problems()[ 0 ].contains( "Bad" ) );
                REQUIRE( scanner.problems()[ 1 ].contains( "Worse" ) );
            }

            THEN( "the valid rule still matches" )
            {
                const auto results = scanner.scanFile( filePath );
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "VIN" ] == "ABC123" );
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
            const auto results = FooterScanner::scan( tmpDir.path() + "/nofile.log", entries );

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

SCENARIO( "FooterScanner uses the first matching line per rule", "[footerscanner][edge]" )
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

SCENARIO( "FooterScanner with duplicate keys uses the first rule", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "two enabled rules with the same key, the later one matching earlier in the file" )
    {
        const QStringList lines = { "id=from-second-rule", "VIN: from-first-rule" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
        entries.append( { "VIN", "id=(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const FooterScanner scanner( entries );
            const auto results = scanner.scanFile( filePath );

            THEN( "the first rule in list order owns the key" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "VIN" ] == "from-first-rule" );
            }

            THEN( "the ordered values show the key once" )
            {
                const auto ordered = scanner.inRuleOrder( results );
                REQUIRE( ordered.size() == 1 );
                REQUIRE( ordered[ 0 ].first == "VIN" );
            }

            THEN( "the ignored duplicate is reported" )
            {
                REQUIRE( scanner.problems().size() == 1 );
                REQUIRE( scanner.problems()[ 0 ].contains( "VIN" ) );
            }
        }
    }

    GIVEN( "a disabled rule followed by an enabled one with the same key" )
    {
        const QStringList lines = { "id=42" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "ID", "VIN:\\s+(\\S+)", "", false, {} } );
        entries.append( { "ID", "id=(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the enabled rule provides the value" )
            {
                REQUIRE( results[ "ID" ] == "42" );
            }
        }
    }
}

SCENARIO( "FooterScanner orders values by rule definition", "[footerscanner][edge]" )
{
    GIVEN( "rules Zeta before Alpha and values for both" )
    {
        QList<FooterEntry> entries;
        entries.append( { "Zeta", "z=(\\S+)", "", true, {} } );
        entries.append( { "Alpha", "a=(\\S+)", "", true, {} } );
        const FooterScanner scanner( entries );

        WHEN( "ordering the values" )
        {
            const auto ordered = scanner.inRuleOrder( { { "Alpha", "1" }, { "Zeta", "2" } } );

            THEN( "they follow the rule order, not the key order" )
            {
                REQUIRE( ordered.size() == 2 );
                REQUIRE( ordered[ 0 ] == qMakePair( QString( "Zeta" ), QString( "2" ) ) );
                REQUIRE( ordered[ 1 ] == qMakePair( QString( "Alpha" ), QString( "1" ) ) );
            }
        }
    }
}

SCENARIO( "FooterScanner bounds what it reads", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );

    GIVEN( "a line far longer than the line length limit before the match" )
    {
        const QStringList lines
            = { QString( FooterScanner::kMaxLineBytes * 4, QChar( 'x' ) ), "VIN: AFTER" };
        const auto filePath = writeTempLines( tmpDir, lines );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the next line is still found" )
            {
                REQUIRE( results[ "VIN" ] == "AFTER" );
            }
        }
    }

    GIVEN( "a match at the start of an over-long line" )
    {
        const QStringList lines
            = { "VIN: EARLY " + QString( FooterScanner::kMaxLineBytes * 2, QChar( 'x' ) ) };
        const auto filePath = writeTempLines( tmpDir, lines );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the truncated line still matches" )
            {
                REQUIRE( results[ "VIN" ] == "EARLY" );
            }
        }
    }

    GIVEN( "a file with Windows line endings" )
    {
        const auto filePath = tmpDir.path() + "/crlf.log";
        QFile file( filePath );
        REQUIRE( file.open( QIODevice::WriteOnly ) );
        file.write( "VIN: CRLF\r\nother\r\n" );
        file.close();

        QList<FooterEntry> anchored;
        anchored.append( { "VIN", "VIN:\\s+(\\S+)$", "", true, {} } );

        WHEN( "scanning with a pattern anchored at the line end" )
        {
            const auto results = FooterScanner::scan( filePath, anchored );

            THEN( "the line ending is not part of the line" )
            {
                REQUIRE( results[ "VIN" ] == "CRLF" );
            }
        }
    }

    GIVEN( "a scan that is cancelled" )
    {
        const auto filePath = writeTempLines( tmpDir, { "VIN: ABC" } );
        const std::atomic_bool cancelled{ true };

        WHEN( "scanning" )
        {
            const auto results = FooterScanner( entries ).scanFile(
                filePath, FooterScanner::kDefaultMaxLines, &cancelled );

            THEN( "nothing is returned" )
            {
                REQUIRE( results.isEmpty() );
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
