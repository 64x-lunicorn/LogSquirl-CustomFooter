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
 * @file footerscanner_preview_test.cpp
 * @brief BDD tests for the live preview of one rule against a file.
 */

#include <catch2/catch.hpp>

#include "footerentry.h"
#include "footerscanner.h"
#include "footervalue.h"

#include <QFile>
#include <QTemporaryDir>

#include <atomic>

using namespace custom_footer;

namespace {

using Status = FooterScanner::Preview::Status;

void writeBytes( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

/// The part of the previewed line a span covers.
QString span( const QString& line, qsizetype start, qsizetype length )
{
    return start < 0 ? QString() : line.mid( start, length );
}

} // namespace

SCENARIO( "FooterScanner previews what a rule finds in a file", "[footerscanner][preview]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/sample.log";
    writeBytes( path, "boot\n"
                      "mode=0x01 early\n"
                      "noise\n"
                      "[init] mode=0x04 set\n"
                      "mode=0x07\n"
                      "tail" );

    GIVEN( "a rule with a mapping" )
    {
        const QList<FooterEntry> entries
            = { { "Mode", "\\[init\\] mode=(\\S+)", "", true, { { "0x04", "Production" } } },
                { "Other", "mode=(\\S+)", "", true, {} } };

        WHEN( "it is previewed" )
        {
            const auto preview = FooterScanner::preview( path, entries, 0 );

            THEN( "it shows the first matching line, its number and both highlights" )
            {
                REQUIRE( preview.status == Status::Scanned );
                REQUIRE( preview.lineNumber == 4 );
                REQUIRE( preview.line == "[init] mode=0x04 set" );
                REQUIRE( span( preview.line, preview.lineMatchStart, preview.lineMatchLength )
                         == "[init] mode=0x04" );
                REQUIRE( span( preview.line, preview.valueStart, preview.valueLength ) == "0x04" );
            }
            THEN( "it shows the raw and the mapped value" )
            {
                REQUIRE( preview.value );
                REQUIRE( *preview.value == FooterValue{ "Mode", "Production", "0x04", 0 } );
            }
            THEN( "it counts the matching lines of the whole file, without a limit reached" )
            {
                REQUIRE( preview.matches == 1 );
                REQUIRE( preview.lines == 6 );
                REQUIRE_FALSE( preview.limitReached() );
            }
            THEN( "the rule supplies its key alone" )
            {
                REQUIRE_FALSE( preview.sharedKey );
                REQUIRE( preview.keyValue == preview.value );
                REQUIRE( preview.keyLineNumber == 4 );
            }
        }

        WHEN( "the other rule, matching several lines, is previewed" )
        {
            const auto preview = FooterScanner::preview( path, entries, 1 );

            THEN( "every matching line is counted, the first one shown" )
            {
                REQUIRE( preview.matches == 3 );
                REQUIRE( preview.lineNumber == 2 );
                REQUIRE( preview.value->rawValue == "0x01" );
                REQUIRE_FALSE( preview.value->isMapped() );
            }
        }

        WHEN( "it is previewed with a line limit the file exceeds" )
        {
            const auto preview = FooterScanner::preview( path, entries, 1, 3 );

            THEN( "only the lines within the limit count, and the limit is reported" )
            {
                REQUIRE( preview.matches == 1 );
                REQUIRE( preview.lines == 3 );
                REQUIRE( preview.lineLimitReached );
                REQUIRE_FALSE( preview.byteLimitReached );
            }
        }

        WHEN( "it is previewed with a line limit equal to the file's lines" )
        {
            const auto preview = FooterScanner::preview( path, entries, 1, 6 );

            THEN( "the whole file was scanned, so no limit is reported" )
            {
                REQUIRE( preview.lines == 6 );
                REQUIRE_FALSE( preview.limitReached() );
            }
        }
    }

    GIVEN( "a rule with a value pattern" )
    {
        const QList<FooterEntry> entries = { { "Mode", "init", "mode=(\\S+)", true, {} } };

        THEN( "the value is highlighted where the value pattern captured it" )
        {
            const auto preview = FooterScanner::preview( path, entries, 0 );
            REQUIRE( span( preview.line, preview.lineMatchStart, preview.lineMatchLength )
                     == "init" );
            REQUIRE( span( preview.line, preview.valueStart, preview.valueLength ) == "0x04" );
        }
    }

    GIVEN( "a rule whose first group does not take part in the match, but a later one does" )
    {
        const QList<FooterEntry> entries = { { "Boot", "^boo(x)?(t)$", "", true, {} } };

        THEN( "the value is empty and has no place in the line" )
        {
            const auto preview = FooterScanner::preview( path, entries, 0 );
            REQUIRE( preview.value->rawValue.isEmpty() );
            REQUIRE( preview.valueStart == -1 );
        }
    }

    GIVEN( "a rule that matches nothing" )
    {
        const QList<FooterEntry> entries = { { "Missing", "absent=(\\S+)", "", true, {} } };

        THEN( "the file is scanned and no value found" )
        {
            const auto preview = FooterScanner::preview( path, entries, 0 );
            REQUIRE( preview.status == Status::Scanned );
            REQUIRE_FALSE( preview.value );
            REQUIRE( preview.matches == 0 );
            REQUIRE( preview.lines == 6 );
        }
    }

    GIVEN( "rules with an invalid line or value pattern" )
    {
        const QList<FooterEntry> entries
            = { { "Bad", "mode=(", "", true, {} }, { "Bad", "mode", "(unclosed", true, {} } };

        THEN( "the preview says which pattern is invalid, and why" )
        {
            const auto line = FooterScanner::preview( path, entries, 0 );
            REQUIRE( line.status == Status::InvalidPattern );
            REQUIRE( line.error.startsWith( "line pattern: " ) );
            REQUIRE( line.error.size() > QString( "line pattern: " ).size() );

            const auto value = FooterScanner::preview( path, entries, 1 );
            REQUIRE( value.status == Status::InvalidPattern );
            REQUIRE( value.error.startsWith( "value pattern: " ) );
        }
        THEN( "an invalid pattern is reported even without a file" )
        {
            REQUIRE( FooterScanner::preview( QString(), entries, 0 ).status
                     == Status::InvalidPattern );
        }
    }

    GIVEN( "a rule without a line pattern" )
    {
        const QList<FooterEntry> entries = { { "Empty", "", "", true, {} } };

        THEN( "the preview says so" )
        {
            REQUIRE( FooterScanner::preview( path, entries, 0 ).status == Status::NoLinePattern );
        }
    }

    GIVEN( "a valid rule but no file" )
    {
        const QList<FooterEntry> entries = { { "Mode", "mode=(\\S+)", "", true, {} } };

        THEN( "the preview says there is no file" )
        {
            REQUIRE( FooterScanner::preview( QString(), entries, 0 ).status == Status::NoFile );
        }
        THEN( "a file that cannot be read is reported" )
        {
            REQUIRE( FooterScanner::preview( tmpDir.path() + "/missing.log", entries, 0 ).status
                     == Status::Unreadable );
        }
    }
}

SCENARIO( "FooterScanner previews which rule supplies a shared key", "[footerscanner][preview]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/shared.log";
    writeBytes( path, "legacy-version 1.0\n"
                      "version=2.0 build=7\n" );

    const QList<FooterEntry> entries = { { "Version", "version=(\\S+)", "", true, {} },
                                         { "Build", "build=(\\S+)", "", true, {} },
                                         { "Version", "legacy-version (\\S+)", "", true, {} },
                                         { "Version", "version=([\\d.]+)", "", true, {} } };

    GIVEN( "a rule whose alternative matches an earlier line" )
    {
        const auto preview = FooterScanner::preview( path, entries, 0 );

        THEN( "the rule finds its own value, but the other rule supplies the key's" )
        {
            REQUIRE( preview.sharedKey );
            REQUIRE( preview.value->value == "2.0" );
            REQUIRE( preview.lineNumber == 2 );
            REQUIRE( preview.keyValue );
            REQUIRE( *preview.keyValue == FooterValue{ "Version", "1.0", "1.0", 2 } );
            REQUIRE( preview.keyLineNumber == 1 );
        }
        THEN( "it matches what the footer shows for the key" )
        {
            const FooterScanner scanner( entries );
            REQUIRE( scanner.scanFrom( path, {} ).values.value( "Version" ) == *preview.keyValue );
        }
    }

    GIVEN( "the alternative that supplies the key" )
    {
        const auto preview = FooterScanner::preview( path, entries, 2 );

        THEN( "the rule itself supplies the key" )
        {
            REQUIRE( preview.keyValue->rule == 2 );
            REQUIRE( preview.keyValue == preview.value );
        }
    }

    GIVEN( "two alternatives matching the same line" )
    {
        const auto preview = FooterScanner::preview( path, entries, 3 );

        THEN( "the one higher in the list would win that line" )
        {
            // Here rule 3 matches line 1, earlier than both.
            REQUIRE( preview.keyValue->rule == 2 );

            auto withoutLegacy = entries;
            withoutLegacy[ 2 ].enabled = false;
            const auto tie = FooterScanner::preview( path, withoutLegacy, 3 );
            REQUIRE( tie.value->value == "2.0" );
            REQUIRE( tie.keyValue->rule == 0 );
        }
    }

    GIVEN( "a disabled rule of a shared key" )
    {
        auto disabled = entries;
        disabled[ 2 ].enabled = false;
        const auto preview = FooterScanner::preview( path, disabled, 2 );

        THEN( "it is previewed, but an enabled alternative supplies the key" )
        {
            REQUIRE_FALSE( preview.enabled );
            REQUIRE( preview.value->value == "1.0" );
            REQUIRE( preview.sharedKey );
            REQUIRE( preview.keyValue->rule == 0 );
        }
    }
}

SCENARIO( "FooterScanner previews within the scan limits", "[footerscanner][preview]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const QList<FooterEntry> entries = { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } };

    GIVEN( "a single line longer than the scan limit" )
    {
        const auto path = tmpDir.path() + "/huge.log";
        {
            QFile file( path );
            REQUIRE( file.open( QIODevice::WriteOnly ) );
            file.write( "VIN: EARLY " );
            const QByteArray chunk( 1024 * 1024, 'x' );
            for ( qint64 written = 0; written <= FooterScanner::kMaxScanBytes;
                  written += chunk.size() ) {
                REQUIRE( file.write( chunk ) == chunk.size() );
            }
            file.write( "\nVIN: LATE\n" );
        }

        const auto preview = FooterScanner::preview( path, entries, 0, 0 );

        THEN( "its start is matched, and the byte limit is reported" )
        {
            REQUIRE( preview.value->value == "EARLY" );
            REQUIRE( preview.line.size() == FooterScanner::kMaxLineBytes );
            REQUIRE( preview.matches == 1 );
            REQUIRE( preview.lines == 1 );
            REQUIRE( preview.byteLimitReached );
            REQUIRE_FALSE( preview.lineLimitReached );
        }
    }

    GIVEN( "a cancelled preview" )
    {
        const auto path = tmpDir.path() + "/small.log";
        writeBytes( path, "VIN: ABC\n" );
        const std::atomic_bool cancelled( true );

        THEN( "it returns nothing but its status" )
        {
            const auto preview = FooterScanner::preview( path, entries, 0, 0, &cancelled );
            REQUIRE( preview.status == Status::Cancelled );
            REQUIRE_FALSE( preview.value );
        }
    }
}

SCENARIO( "FooterScanner's preview reads lines as the footer's scan does",
          "[footerscanner][preview]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/agree.log";
    const QList<FooterEntry> entries = { { "Build", "build=(\\S+)", "", true, {} },
                                         { "Build", "legacy build (\\S+)", "", true, {} } };
    const FooterScanner scanner( entries );

    GIVEN( "a last line without a line break that supplies the key" )
    {
        writeBytes( path, "noise\nlegacy build 7" );
        const auto preview = FooterScanner::preview( path, entries, 1 );
        const auto scan = scanner.scanFrom( path, {} );

        THEN( "both take the value from it, and the preview counts it as a line" )
        {
            REQUIRE( scan.values.value( "Build" ) == *preview.keyValue );
            REQUIRE( preview.keyLineNumber == 2 );
            REQUIRE( preview.lines == 2 );
            REQUIRE( scan.progress.lines == 1 ); // not remembered: still being written
        }
    }

    GIVEN( "a line limit that stops both at the same line" )
    {
        writeBytes( path, "a\nb\nbuild=late\n" );
        const auto preview = FooterScanner::preview( path, entries, 0, 2 );
        const auto scan = scanner.scanFrom( path, {}, 2 );

        THEN( "neither sees the line after the limit" )
        {
            REQUIRE_FALSE( preview.value );
            REQUIRE( scan.values.isEmpty() );
            REQUIRE( preview.lines == scan.progress.lines );
            REQUIRE( preview.lineLimitReached );
        }
    }
}
