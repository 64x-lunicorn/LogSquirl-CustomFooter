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
 * @file footerscanner_utf16_test.cpp
 * @brief BDD tests for UTF-16 log files with a byte order mark: lines,
 *        values and their Log Lines as LogSquirl reads them, across
 *        continued scans, in the preview and in the editor's live preview.
 */

#include <catch2/catch.hpp>

#include "eventloopwait.h"
#include "footerentry.h"
#include "footerscanner.h"
#include "rulepreviewer.h"

#include <QFile>
#include <QTemporaryDir>

#include <vector>

using namespace custom_footer;

namespace {

using Encoding = FooterScanner::Encoding;
using Status = FooterScanner::Preview::Status;

void writeBytes( const QString& path, const QByteArray& content, bool append = false )
{
    QFile file( path );
    REQUIRE( file.open( append ? QIODevice::Append : QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

enum class ByteOrder { LittleEndian, BigEndian };

/// @p text in UTF-16 of @p order, code unit by code unit, without a mark.
QByteArray utf16( const QString& text, ByteOrder order )
{
    QByteArray bytes;
    for ( const QChar unit : text ) {
        const auto low = static_cast<char>( unit.unicode() & 0xFF );
        const auto high = static_cast<char>( unit.unicode() >> 8 );
        if ( order == ByteOrder::LittleEndian ) {
            bytes += low;
            bytes += high;
        }
        else {
            bytes += high;
            bytes += low;
        }
    }
    return bytes;
}

/// The byte order mark of @p order: FF FE or FE FF.
QByteArray bom( ByteOrder order )
{
    return order == ByteOrder::LittleEndian ? QByteArray( "\xFF\xFE" ) : QByteArray( "\xFE\xFF" );
}

/// @p text as a UTF-16 file of @p order starting with its byte order mark.
QByteArray utf16File( const QString& text, ByteOrder order )
{
    return bom( order ) + utf16( text, order );
}

const char* orderName( ByteOrder order )
{
    return order == ByteOrder::LittleEndian ? "UTF-16LE" : "UTF-16BE";
}

/// The encoding the scanner tells from the mark of @p order.
Encoding::Form form( ByteOrder order )
{
    return order == ByteOrder::LittleEndian ? Encoding::Form::Utf16LE : Encoding::Form::Utf16BE;
}

QList<FooterEntry> vinAndIdRules()
{
    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    entries.append( { "ID", "id=(\\S+)", "", true, {} } );
    return entries;
}

/// U+010A "Ċ": its code unit holds the byte 0x0A, first in UTF-16LE
/// (0A 01), second in UTF-16BE (01 0A).
const QString cDot = QStringLiteral( "Ċ" );
/// U+0A00: its code unit holds the byte 0x0A the other way round (00 0A in
/// UTF-16LE, 0A 00 in UTF-16BE).
const QString unitA00 = QString( QChar( 0x0A00 ) );
/// U+1F600, an emoji outside the BMP: a surrogate pair of two code units.
const QString emoji = QStringLiteral( "\U0001F600" );

/// A log with a 0x0A byte inside characters on its first lines, a Windows
/// line end, and an astral character in a value. LogSquirl shows it as
/// four lines: "Ċ", "x" + U+0A00, "VIN: ĊAR" and "id=" emoji "42".
QString sampleLog()
{
    return cDot + "\n" + "x" + unitA00 + "\n" + "VIN: " + cDot + "AR\r\n" + "id=" + emoji
           + "42\r\n";
}

} // namespace

SCENARIO( "FooterScanner reads a UTF-16 file with a byte order mark as LogSquirl does",
          "[footerscanner][utf16]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/utf16.log";
    const auto order = GENERATE( ByteOrder::LittleEndian, ByteOrder::BigEndian );
    const FooterScanner scanner( vinAndIdRules() );

    GIVEN( std::string( "a " ) + orderName( order ) + " file whose characters hold 0x0A bytes" )
    {
        writeBytes( path, utf16File( sampleLog(), order ) );

        WHEN( "it is scanned" )
        {
            const auto scan = scanner.scanFrom( path, {} );

            THEN( "the values are decoded, and their Log Lines are LogSquirl's" )
            {
                REQUIRE( scan.values[ "VIN" ].value == cDot + "AR" );
                REQUIRE( scan.values[ "VIN" ].lineNumber == 3 );
                REQUIRE( scan.values[ "ID" ].value == emoji + "42" );
                REQUIRE( scan.values[ "ID" ].lineNumber == 4 );
            }

            THEN( "a line ends at a line feed code unit only, and a CR LF ends one line" )
            {
                REQUIRE( scan.progress.lines == 4 );
                REQUIRE( scan.progress.offset == QFile( path ).size() );
                REQUIRE( scan.progress.encoding.form == form( order ) );
            }
        }

        WHEN( "the first line, and a line ending in CR LF, are previewed" )
        {
            const QList<FooterEntry> entries
                = { { "First", "^(.*)$", "", true, {} }, { "Car", "^VIN: (.*)$", "", true, {} } };
            const auto first = FooterScanner::preview( path, entries, 0 );
            const auto car = FooterScanner::preview( path, entries, 1 );

            THEN( "the byte order mark is not part of the first line's text" )
            {
                REQUIRE( first.status == Status::Scanned );
                REQUIRE( first.lineNumber == 1 );
                REQUIRE( first.line == cDot );
            }

            THEN( "the line break is not part of the line's text" )
            {
                REQUIRE( car.lineNumber == 3 );
                REQUIRE( car.line == "VIN: " + cDot + "AR" );
                REQUIRE( car.value->value == cDot + "AR" );
            }

            THEN( "every line is counted, as the scan does" )
            {
                REQUIRE( first.matches == 4 );
                REQUIRE( first.lines == 4 );
            }
        }
    }

    GIVEN( std::string( "a " ) + orderName( order ) + " file with U+0A00 before U+0100" )
    {
        // UTF-16LE: 00 0A 00 01, UTF-16BE: 0A 00 01 00. Bytes 0A 00 as in a
        // line feed, but not on a code unit boundary in UTF-16LE.
        writeBytes( path, utf16File( unitA00 + QChar( 0x0100 ) + "\nVIN: X\n", order ) );

        WHEN( "it is scanned" )
        {
            const auto scan = scanner.scanFrom( path, {} );

            THEN( "only the whole line feed code unit ends a line" )
            {
                REQUIRE( scan.values[ "VIN" ].lineNumber == 2 );
                REQUIRE( scan.progress.lines == 2 );
            }
        }
    }

    GIVEN( std::string( "a " ) + orderName( order ) + " file of only its byte order mark" )
    {
        writeBytes( path, bom( order ) );

        WHEN( "it is scanned" )
        {
            const auto scan = scanner.scanFrom( path, {} );

            THEN( "it has no lines and no values" )
            {
                REQUIRE( scan.values.isEmpty() );
                REQUIRE( scan.progress.lines == 0 );
            }
        }
    }
}

SCENARIO( "FooterScanner bounds the lines of a UTF-16 file in bytes", "[footerscanner][utf16]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/long.log";
    const auto order = GENERATE( ByteOrder::LittleEndian, ByteOrder::BigEndian );

    GIVEN( std::string( "a " ) + orderName( order )
           + " file with a line longer than the line length limit" )
    {
        // 40000 code units: 80000 bytes, read in many chunks.
        const QString longLine = cDot + QString( 39999, 'x' );
        writeBytes( path, utf16File( longLine + "\nVIN: AFTER\n", order ) );
        const QList<FooterEntry> entries = { { "Long", "^(" + cDot + "x+)", "", true, {} },
                                             { "VIN", "VIN: (\\S+)", "", true, {} } };

        WHEN( "it is scanned" )
        {
            const auto values = FooterScanner( entries ).scanFrom( path, {} ).values;

            THEN( "the line is matched against its first kMaxLineBytes, whole code units" )
            {
                REQUIRE( values[ "Long" ].value.size() == FooterScanner::kMaxLineBytes / 2 );
                REQUIRE( values[ "Long" ].lineNumber == 1 );
            }

            THEN( "the line after it has the next number" )
            {
                REQUIRE( values[ "VIN" ].value == "AFTER" );
                REQUIRE( values[ "VIN" ].lineNumber == 2 );
            }
        }
    }
}

SCENARIO( "FooterScanner reads other files as before", "[footerscanner][utf16]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/other.log";
    const FooterScanner scanner( vinAndIdRules() );
    const QList<FooterEntry> wholeLine = { { "Line", "^(.*)$", "", true, {} } };

    GIVEN( "a UTF-8 file starting with a UTF-8 byte order mark" )
    {
        writeBytes( path, "\xEF\xBB\xBF"
                          "VIN: BOM\nid=2\n" );

        WHEN( "its first line is previewed" )
        {
            const auto preview = FooterScanner::preview( path, wholeLine, 0 );

            THEN( "the mark is in the first line, but not in its text, as before" )
            {
                REQUIRE( preview.lineNumber == 1 );
                REQUIRE( preview.line == "VIN: BOM" );
                REQUIRE( preview.read.offset == 3 + 9 + 5 );
            }
        }

        WHEN( "it is scanned" )
        {
            const auto scan = scanner.scanFrom( path, {} );

            THEN( "it is read as UTF-8" )
            {
                REQUIRE( scan.values[ "VIN" ].lineNumber == 1 );
                REQUIRE( scan.values[ "ID" ].lineNumber == 2 );
                REQUIRE( scan.progress.encoding.form == Encoding::Form::Utf8 );
            }
        }
    }

    GIVEN( "a single-byte file with bytes that are no UTF-8" )
    {
        // Latin-1 "VIN: ÄÖ": read as UTF-8, as before.
        writeBytes( path, "VIN: \xC4\xD6\nid=1\n" );

        WHEN( "it is scanned" )
        {
            const auto scan = scanner.scanFrom( path, {} );

            THEN( "its lines are split at the byte 0x0A and decoded as UTF-8" )
            {
                REQUIRE( scan.values[ "VIN" ].value == QString::fromUtf8( "\xC4\xD6" ) );
                REQUIRE( scan.values[ "ID" ].lineNumber == 2 );
            }
        }
    }

    GIVEN( "a UTF-16LE file without a byte order mark" )
    {
        const auto bytes = utf16( cDot + "\nVIN: X\n", ByteOrder::LittleEndian );
        writeBytes( path, bytes );

        WHEN( "it is scanned" )
        {
            const auto scan = scanner.scanFrom( path, {} );

            THEN( "it is read as bytes split at every 0x0A, as before" )
            {
                REQUIRE( scan.progress.lines == bytes.count( '\n' ) );
                REQUIRE( scan.progress.encoding.form == Encoding::Form::Utf8 );
            }
        }
    }
}

SCENARIO( "The encoding of a file is told from its first bytes", "[footerscanner][utf16]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/any.log";

    GIVEN( "files with and without a UTF-16 byte order mark, and none" )
    {
        THEN( "a mark gives UTF-16 of its byte order; anything else, UTF-8" )
        {
            for ( const auto order : { ByteOrder::LittleEndian, ByteOrder::BigEndian } ) {
                writeBytes( path, utf16File( "VIN: X\n", order ) );
                REQUIRE( Encoding::ofFile( path ) == Encoding::detect( bom( order ) ) );
                REQUIRE( Encoding::ofFile( path ).unitBytes() == 2 );
            }
            writeBytes( path, "\xEF\xBB\xBF"
                              "VIN: X\n" );
            REQUIRE( Encoding::ofFile( path ) == Encoding() );
            writeBytes( path, "" );
            REQUIRE( Encoding::ofFile( path ) == Encoding() );
            REQUIRE( Encoding::ofFile( tmpDir.path() + "/missing.log" ) == Encoding() );
            REQUIRE( Encoding::ofFile( QString() ) == Encoding() );
        }
    }
}

SCENARIO( "FooterScanner continues a scan of a growing UTF-16 file",
          "[footerscanner][utf16][resume]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/growing.log";
    const auto order = GENERATE( ByteOrder::LittleEndian, ByteOrder::BigEndian );
    const FooterScanner scanner( vinAndIdRules() );

    GIVEN( std::string( "a scanned " ) + orderName( order ) + " file with one of the keys" )
    {
        writeBytes( path, utf16File( cDot + "\r\nVIN: " + cDot + "1\r\n", order ) );
        const auto first = scanner.scanFrom( path, {} );
        REQUIRE( first.values[ "VIN" ].lineNumber == 2 );
        REQUIRE_FALSE( first.progress.done );

        WHEN( "lines are appended, one with the missing key" )
        {
            writeBytes( path, utf16( "noise\r\n" + unitA00 + "\nid=" + emoji + "\r\n", order ),
                        true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the new value has its line in the whole file" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "ID" ].value == emoji );
                REQUIRE( next.values[ "ID" ].lineNumber == 5 );
                REQUIRE( next.values[ "VIN" ].lineNumber == 2 );
            }

            THEN( "a fresh scan finds the same values on the same lines" )
            {
                REQUIRE( scanner.scanFrom( path, {} ).values == next.values );
            }
        }

        WHEN( "nothing changed" )
        {
            THEN( "a change of the file cannot alter the scan" )
            {
                REQUIRE( FooterScanner::fileUnchangedFor( path, first ) );
            }
        }

        WHEN( "it is truncated and written again" )
        {
            writeBytes( path, utf16File( "id=new\n", order ) );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "it is scanned from the start" )
            {
                REQUIRE_FALSE( FooterScanner::fileUnchangedFor( path, first ) );
                REQUIRE_FALSE( next.resumed );
                REQUIRE( next.values[ "ID" ].lineNumber == 1 );
                REQUIRE_FALSE( next.values.contains( "VIN" ) );
            }
        }

        WHEN( "it is rewritten as a larger file of the other byte order" )
        {
            const auto other
                = order == ByteOrder::LittleEndian ? ByteOrder::BigEndian : ByteOrder::LittleEndian;
            writeBytes( path,
                        utf16File( cDot + "\r\nVIN: " + cDot + "1\r\nVIN: 2\nid=3\n", other ) );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "it is scanned from the start, in its byte order" )
            {
                REQUIRE_FALSE( next.resumed );
                REQUIRE( next.values[ "VIN" ].value == cDot + "1" );
                REQUIRE( next.values[ "ID" ].lineNumber == 4 );
            }
        }

        WHEN( "it is replaced by a larger UTF-8 file" )
        {
            REQUIRE( QFile::remove( path ) );
            writeBytes( path, "rotated\nid=8\nVIN: UTF8\nand some more\n" );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "it is scanned from the start as UTF-8" )
            {
                REQUIRE_FALSE( next.resumed );
                REQUIRE( next.values[ "ID" ].lineNumber == 2 );
                REQUIRE( next.values[ "VIN" ].value == "UTF8" );
                REQUIRE( next.progress.encoding.form == Encoding::Form::Utf8 );
            }
        }
    }

    GIVEN( std::string( "a " ) + orderName( order )
           + " file being written, ending in half a code unit" )
    {
        const auto line = utf16( "VIN: ABC", order );
        const auto start = utf16File( cDot + "\n", order );
        // Up to the first byte of "C".
        writeBytes( path, start + line.left( line.size() - 1 ) );
        const auto first = scanner.scanFrom( path, {} );

        THEN( "the last line's whole code units are matched, with its number" )
        {
            REQUIRE( first.values[ "VIN" ].value == "AB" );
            REQUIRE( first.values[ "VIN" ].lineNumber == 2 );
        }

        THEN( "the progress stays at the end of the last complete line" )
        {
            REQUIRE( first.progress.offset == start.size() );
            REQUIRE( first.progress.lines == 1 );
        }

        WHEN( "the code unit and more lines are written" )
        {
            writeBytes( path, line.right( 1 ) + utf16( "\r\nid=" + cDot + "\n", order ), true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the line is read again whole, with the same number" )
            {
                REQUIRE( next.resumed );
                REQUIRE( next.values[ "VIN" ].value == "ABC" );
                REQUIRE( next.values[ "VIN" ].lineNumber == 2 );
                REQUIRE( next.values[ "ID" ].value == cDot );
                REQUIRE( next.values[ "ID" ].lineNumber == 3 );
            }
        }
    }

    GIVEN( std::string( "a " ) + orderName( order ) + " file ending in half a line feed" )
    {
        const auto lineFeed = utf16( "\n", order );
        writeBytes( path, utf16File( "VIN: X", order ) + lineFeed.left( 1 ) );
        const auto first = scanner.scanFrom( path, {} );

        WHEN( "the line feed is completed and a line appended" )
        {
            writeBytes( path, lineFeed.right( 1 ) + utf16( "id=1\n", order ), true );
            const auto next = scanner.scanFrom( path, first.progress );

            THEN( "the lines count as one line each" )
            {
                REQUIRE( first.values[ "VIN" ].lineNumber == 1 );
                REQUIRE( next.values[ "VIN" ].lineNumber == 1 );
                REQUIRE( next.values[ "ID" ].lineNumber == 2 );
                REQUIRE( next.progress.lines == 2 );
            }
        }
    }
}

SCENARIO( "The editor's live preview finds values in a UTF-16 file", "[preview][utf16]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/utf16.log";
    const auto order = GENERATE( ByteOrder::LittleEndian, ByteOrder::BigEndian );

    GIVEN( std::string( "a previewer with a " ) + orderName( order ) + " file as the active file" )
    {
        writeBytes( path, utf16File( sampleLog(), order ) );
        RulePreviewer previewer;
        std::vector<FooterScanner::Preview> previews;
        QObject::connect( &previewer, &RulePreviewer::previewed,
                          [ &previews ]( const FooterScanner::Preview& preview ) {
                              previews.push_back( preview );
                          } );
        previewer.setActiveFile( path );

        WHEN( "a rule is previewed" )
        {
            previewer.schedule( vinAndIdRules(), 1 );
            previewer.flush();
            REQUIRE( processUntil( [ &previewer ] { return !previewer.isBusy(); } ) );

            THEN( "it shows the value and its Log Line as the footer does" )
            {
                REQUIRE( previews.size() == 1 );
                const auto& preview = previews.front();
                REQUIRE( preview.status == Status::Scanned );
                REQUIRE( preview.value->value == emoji + "42" );
                REQUIRE( preview.lineNumber == 4 );
                REQUIRE( preview.line == "id=" + emoji + "42" );
            }
        }
    }
}
