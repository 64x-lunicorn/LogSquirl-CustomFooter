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

SCENARIO( "FooterScanner treats rules sharing a key as alternatives", "[footerscanner][edge]" )
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

            THEN( "the first line any of the key's rules matches provides the value" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "VIN" ] == "from-second-rule" );
            }

            THEN( "the ordered values show the key once" )
            {
                const auto ordered
                    = scanner.footerValues( scanner.scanFrom( filePath, {} ).values );
                REQUIRE( ordered.size() == 1 );
                REQUIRE( ordered[ 0 ].key == "VIN" );
            }

            THEN( "nothing is reported" )
            {
                REQUIRE( scanner.problems().isEmpty() );
            }
        }
    }

    GIVEN( "two rules with the same key that both match the same line" )
    {
        const QStringList lines = { "noise", "build=7 rev=abc" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "Build", "rev=(\\S+)", "", true, {} } );
        entries.append( { "Build", "build=(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "the first rule in list order wins" )
            {
                REQUIRE( results[ "Build" ] == "abc" );
            }
        }
    }

    GIVEN( "alternatives for a key around a rule for another key" )
    {
        const QStringList lines = { "id=42", "new-format version 2.0" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "Version", "old-format v(\\S+)", "", true, {} } );
        entries.append( { "ID", "id=(\\S+)", "", true, {} } );
        entries.append( { "Version", "new-format version (\\S+)", "", true, {} } );

        WHEN( "only the later alternative matches" )
        {
            const FooterScanner scanner( entries );
            const auto ordered = scanner.footerValues( scanner.scanFrom( filePath, {} ).values );

            THEN( "the key is shown at the position of its first rule" )
            {
                REQUIRE( ordered.size() == 2 );
                REQUIRE( ordered[ 0 ] == FooterValue{ "Version", "2.0", "2.0", 2 } );
                REQUIRE( ordered[ 1 ] == FooterValue{ "ID", "42", "42", 1 } );
            }
        }
    }

    GIVEN( "a disabled rule followed by an enabled one with the same key" )
    {
        const QStringList lines = { "VIN: ignored", "id=42" };
        const auto filePath = writeTempLines( tmpDir, lines );

        QList<FooterEntry> entries;
        entries.append( { "ID", "VIN:\\s+(\\S+)", "", false, {} } );
        entries.append( { "ID", "id=(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const auto results = FooterScanner::scan( filePath, entries );

            THEN( "only the enabled rule provides the value" )
            {
                REQUIRE( results[ "ID" ] == "42" );
            }
        }
    }
}

SCENARIO( "FooterScanner skips incomplete rules", "[footerscanner][edge]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    const auto filePath = writeTempLines( tmpDir, { "id=42" } );

    GIVEN( "an enabled rule with a line pattern but no key" )
    {
        QList<FooterEntry> entries;
        entries.append( { "  ", "id=(\\S+)", "", true, {} } );
        entries.append( { "ID", "id=(\\S+)", "", true, {} } );

        WHEN( "scanning" )
        {
            const FooterScanner scanner( entries );
            const auto results = scanner.scanFile( filePath );

            THEN( "it is skipped and reported" )
            {
                REQUIRE( results.size() == 1 );
                REQUIRE( results[ "ID" ] == "42" );
                REQUIRE( scanner.problems().size() == 1 );
                REQUIRE( scanner.problems()[ 0 ].startsWith( "Rule 1" ) );
            }
        }
    }

    GIVEN( "an enabled rule with a key but no line pattern" )
    {
        QList<FooterEntry> entries;
        entries.append( { "Empty", "", "", true, {} } );

        WHEN( "scanning" )
        {
            const FooterScanner scanner( entries );

            THEN( "it is skipped silently" )
            {
                REQUIRE( scanner.scanFile( filePath ).isEmpty() );
                REQUIRE( scanner.problems().isEmpty() );
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
            const auto ordered = scanner.footerValues(
                { { "Alpha", { "Alpha", "1", "1", 1 } }, { "Zeta", { "Zeta", "2", "2", 0 } } } );

            THEN( "they follow the rule order, not the key order" )
            {
                REQUIRE( ordered.size() == 2 );
                REQUIRE( ordered[ 0 ].key == "Zeta" );
                REQUIRE( ordered[ 1 ].key == "Alpha" );
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

    GIVEN( "a single line longer than the scan limit, followed by a match" )
    {
        const auto filePath = tmpDir.path() + "/one-line.log";
        QFile file( filePath );
        REQUIRE( file.open( QIODevice::WriteOnly ) );
        const QByteArray chunk( 1024 * 1024, 'x' );
        for ( qint64 written = 0; written <= FooterScanner::kMaxScanBytes;
              written += chunk.size() ) {
            REQUIRE( file.write( chunk ) == chunk.size() );
        }
        file.write( "\nVIN: AFTER\n" );
        file.close();

        WHEN( "scanning without a line limit" )
        {
            const auto scan = FooterScanner( entries ).scanFrom( filePath, {}, 0 );

            THEN( "the scan stops at the scan limit inside the line" )
            {
                REQUIRE( scan.values.isEmpty() );
                REQUIRE( scan.progress.done );
                REQUIRE( scan.progress.lines == 0 );
                REQUIRE( scan.progress.offset >= FooterScanner::kMaxScanBytes );
                REQUIRE( scan.progress.offset
                         <= FooterScanner::kMaxScanBytes + FooterScanner::kMaxLineBytes + 1 );
            }

            AND_WHEN( "the file is scanned again" )
            {
                const auto again = FooterScanner( entries ).scanFrom( filePath, scan.progress, 0 );

                THEN( "the scan continues as done, without reading the line again" )
                {
                    REQUIRE( again.resumed );
                    REQUIRE( again.progress.done );
                    REQUIRE( again.values.isEmpty() );
                }
            }
        }

        WHEN( "scanning with a cancelled flag" )
        {
            const std::atomic_bool cancelled{ true };
            const auto scan = FooterScanner( entries ).scanFrom( filePath, {}, 0, &cancelled );

            THEN( "nothing is returned" )
            {
                REQUIRE( scan.values.isEmpty() );
                REQUIRE_FALSE( scan.progress.done );
                REQUIRE( scan.progress.offset == 0 );
            }
        }
    }

    GIVEN( "a long line crossing the scan limit, with a match at its start" )
    {
        const auto filePath = tmpDir.path() + "/crossing.log";
        QFile file( filePath );
        REQUIRE( file.open( QIODevice::WriteOnly ) );
        // Lines of 1 MiB up to just before the limit, then one that crosses it.
        const QByteArray line = QByteArray( 1024 * 1024 - 1, 'x' ) + '\n';
        for ( qint64 written = 0;
              written + line.size() < FooterScanner::kMaxScanBytes - 4 * 1024 * 1024;
              written += line.size() ) {
            REQUIRE( file.write( line ) == line.size() );
        }
        file.write( "VIN: EARLY " );
        const QByteArray chunk( 1024 * 1024, 'y' );
        for ( int i = 0; i < 8; ++i ) {
            REQUIRE( file.write( chunk ) == chunk.size() );
        }
        file.write( "\n" );
        file.close();

        WHEN( "scanning without a line limit" )
        {
            const auto scan = FooterScanner( entries ).scanFrom( filePath, {}, 0 );

            THEN( "the start of the crossing line is still matched, and the scan is done" )
            {
                REQUIRE( scan.values[ "VIN" ].value == "EARLY" );
                REQUIRE( scan.progress.values[ "VIN" ].value == "EARLY" );
                REQUIRE( scan.progress.done );
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
