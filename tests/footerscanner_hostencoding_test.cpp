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
 * @file footerscanner_hostencoding_test.cpp
 * @brief BDD tests for a Log File read in the encoding the host reports:
 *        a UTF-16 file without byte order mark and a single-byte encoding,
 *        in the scanner, the live preview and the footer session, through
 *        the host-function seam (HostFunctions::logFileEncoding).
 */

#include <catch2/catch.hpp>

#include "eventloopwait.h"
#include "footerconfig.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"
#include "footerentry.h"
#include "footerscanner.h"
#include "footersession.h"
#include "hostfunctions.h"
#include "rulefromline.h"
#include "rulepreviewer.h"

#include <QFile>
#include <QTemporaryDir>

#include <vector>

using namespace custom_footer;

namespace {

using Encoding = FooterScanner::Encoding;
using Status = FooterScanner::Preview::Status;

void writeBytes( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

/// @p text in UTF-16LE, code unit by code unit, without a mark.
QByteArray utf16LE( const QString& text )
{
    QByteArray bytes;
    for ( const QChar unit : text ) {
        bytes += static_cast<char>( unit.unicode() & 0xFF );
        bytes += static_cast<char>( unit.unicode() >> 8 );
    }
    return bytes;
}

/// @p text in UTF-16BE, code unit by code unit, without a mark.
QByteArray utf16BE( const QString& text )
{
    QByteArray bytes;
    for ( const QChar unit : text ) {
        bytes += static_cast<char>( unit.unicode() >> 8 );
        bytes += static_cast<char>( unit.unicode() & 0xFF );
    }
    return bytes;
}

/// @p text in UTF-32LE, code point by code point, without a mark.
QByteArray utf32LE( const QString& text )
{
    QByteArray bytes;
    for ( const char32_t c : text.toUcs4() ) {
        for ( int shift = 0; shift < 32; shift += 8 ) {
            bytes += static_cast<char>( ( c >> shift ) & 0xFF );
        }
    }
    return bytes;
}

QList<FooterEntry> vinAndIdRules()
{
    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    entries.append( { "ID", "id=(\\S+)", "", true, {} } );
    return entries;
}

/// U+010A "Ċ": its UTF-16LE code unit 0A 01 holds the byte 0x0A.
const QString cDot = QStringLiteral( "Ċ" );

/// A log whose first line holds a 0x0A byte in UTF-16LE: LogSquirl shows it
/// as three lines, "Ċ", "VIN: ĊAR" and "id=42".
QString sampleLog()
{
    return cDot + "\n" + "VIN: " + cDot + "AR\r\n" + "id=42\n";
}

/// Latin-1 / windows-1252 "VIN: ÄÖ", "id=ß": bytes that are no UTF-8, in
/// the range both encodings agree on.
const QByteArray singleByteLog = "first\nVIN: \xC4\xD6\nid=\xDF\n";

FooterEntry wholeLine( const QString& key = "Line" )
{
    return { key, "^(.*)$", "", true, {} };
}

} // namespace

SCENARIO( "FooterScanner reads a UTF-16 file without byte order mark as the host names it",
          "[footerscanner][hostencoding]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/nobom.log";
    const FooterScanner scanner( vinAndIdRules() );

    GIVEN( "a UTF-16LE file without a mark, which the host reads as UTF-16LE" )
    {
        writeBytes( path, utf16LE( sampleLog() ) );

        WHEN( "it is scanned in the host's encoding" )
        {
            const auto scan = scanner.scanFrom( path, {}, FooterScanner::kDefaultMaxLines, nullptr,
                                                "UTF-16LE" );

            THEN( "its values are decoded, and their Log Lines are the host's" )
            {
                REQUIRE( scan.values[ "VIN" ].value == cDot + "AR" );
                REQUIRE( scan.values[ "VIN" ].lineNumber == 2 );
                REQUIRE( scan.values[ "ID" ].value == "42" );
                REQUIRE( scan.values[ "ID" ].lineNumber == 3 );
            }

            THEN( "a line ends at a line feed code unit only, from the first byte on" )
            {
                REQUIRE( scan.progress.encoding.form == Encoding::Form::Utf16LE );
                REQUIRE( scan.progress.encoding.textStart == 0 );
                REQUIRE( scan.progress.lines == 3 );
            }

            AND_WHEN( "lines are appended and it is scanned on, still in UTF-16LE" )
            {
                const FooterScanner more( { { "Tail", "^(tail)$", "", true, {} } } );
                const auto first = more.scanFrom( path, {}, FooterScanner::kDefaultMaxLines,
                                                  nullptr, "UTF-16LE" );
                QFile file( path );
                REQUIRE( file.open( QIODevice::Append ) );
                file.write( utf16LE( "tail\n" ) );
                file.close();
                const auto next = more.scanFrom(
                    path, first.progress, FooterScanner::kDefaultMaxLines, nullptr, "UTF-16LE" );

                THEN( "the scan continues, counting the lines on" )
                {
                    REQUIRE( next.resumed );
                    REQUIRE( next.values[ "Tail" ].lineNumber == 4 );
                }
            }
        }

        WHEN( "it is scanned without a name, as on a host without the encoding" )
        {
            const auto scan = scanner.scanFrom( path, {} );

            THEN( "it is read as before: bytes split at every 0x0A" )
            {
                REQUIRE( scan.progress.encoding.form == Encoding::Form::Utf8 );
                REQUIRE( scan.progress.lines == utf16LE( sampleLog() ).count( '\n' ) );
            }

            AND_WHEN( "the host then names UTF-16LE, and it is scanned on from that scan" )
            {
                const auto again = scanner.scanFrom(
                    path, scan.progress, FooterScanner::kDefaultMaxLines, nullptr, "UTF-16LE" );

                THEN( "it is scanned afresh from its start, in UTF-16LE" )
                {
                    REQUIRE_FALSE( again.resumed );
                    REQUIRE( again.progress.lines == 3 );
                    REQUIRE( again.values[ "VIN" ].lineNumber == 2 );
                }
            }
        }

        WHEN( "its first line and the line ending in CR LF are previewed in UTF-16LE" )
        {
            const QList<FooterEntry> entries
                = { wholeLine( "First" ), { "Car", "^VIN: (.*)$", "", true, {} } };
            const auto first = FooterScanner::preview(
                path, entries, 0, FooterScanner::kDefaultMaxLines, nullptr, "UTF-16LE" );
            const auto car = FooterScanner::preview(
                path, entries, 1, FooterScanner::kDefaultMaxLines, nullptr, "UTF-16LE" );

            THEN( "the preview reads the lines as the scan does" )
            {
                REQUIRE( first.status == Status::Scanned );
                REQUIRE( first.line == cDot );
                REQUIRE( first.lines == 3 );
                REQUIRE( car.lineNumber == 2 );
                REQUIRE( car.line == "VIN: " + cDot + "AR" );
                REQUIRE( car.read.encoding.form == Encoding::Form::Utf16LE );
            }
        }
    }

    GIVEN( "a UTF-16BE file without a mark, which the host reads as UTF-16BE" )
    {
        writeBytes( path, utf16BE( sampleLog() ) );

        WHEN( "it is scanned in the host's encoding" )
        {
            const auto scan = scanner.scanFrom( path, {}, FooterScanner::kDefaultMaxLines, nullptr,
                                                "UTF-16BE" );

            THEN( "its values and their Log Lines are the host's" )
            {
                REQUIRE( scan.values[ "VIN" ].value == cDot + "AR" );
                REQUIRE( scan.values[ "VIN" ].lineNumber == 2 );
                REQUIRE( scan.progress.lines == 3 );
            }
        }
    }

    GIVEN( "a UTF-16LE file with its mark, which the host reads as UTF-16LE" )
    {
        writeBytes( path, "\xFF\xFE" + utf16LE( sampleLog() ) );

        WHEN( "its first line is previewed in the host's encoding" )
        {
            const auto first = FooterScanner::preview(
                path, { wholeLine() }, 0, FooterScanner::kDefaultMaxLines, nullptr, "utf-16le" );

            THEN( "the mark is not part of the first line, whatever the name's case" )
            {
                REQUIRE( first.read.encoding.textStart == 2 );
                REQUIRE( first.line == cDot );
            }
        }
    }

    GIVEN( "a UTF-32LE file without a mark, which the host reads as UTF-32LE" )
    {
        // U+0A0A holds the byte 0x0A twice, and U+010A once.
        writeBytes( path, utf32LE( QString( QChar( 0x0A0A ) ) + cDot + "\nVIN: X\n" ) );

        WHEN( "it is scanned in the host's encoding" )
        {
            const auto scan = scanner.scanFrom( path, {}, FooterScanner::kDefaultMaxLines, nullptr,
                                                "UTF-32LE" );

            THEN( "only the whole line feed code unit ends a line" )
            {
                REQUIRE( scan.progress.encoding.unitBytes() == 4 );
                REQUIRE( scan.values[ "VIN" ].value == "X" );
                REQUIRE( scan.values[ "VIN" ].lineNumber == 2 );
                REQUIRE( scan.progress.lines == 2 );
            }
        }
    }
}

SCENARIO( "FooterScanner reads a file in a single-byte encoding the host names",
          "[footerscanner][hostencoding]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/latin.log";
    const FooterScanner scanner( vinAndIdRules() );
    writeBytes( path, singleByteLog );

    GIVEN( "a file the host reads as windows-1252, or as ISO-8859-1" )
    {
        const QByteArray name
            = GENERATE( QByteArray( "windows-1252" ), QByteArray( "ISO-8859-1" ) );

        WHEN( std::string( "it is scanned in " ) + name.constData() )
        {
            const auto scan
                = scanner.scanFrom( path, {}, FooterScanner::kDefaultMaxLines, nullptr, name );

            THEN( "its values are decoded in that encoding, on the host's Log Lines" )
            {
                REQUIRE( scan.values[ "VIN" ].value == QStringLiteral( "ÄÖ" ) );
                REQUIRE( scan.values[ "VIN" ].lineNumber == 2 );
                REQUIRE( scan.values[ "ID" ].value == QStringLiteral( "ß" ) );
                REQUIRE( scan.values[ "ID" ].lineNumber == 3 );
                REQUIRE( scan.progress.encoding.form == Encoding::Form::Named );
                REQUIRE( scan.progress.encoding.name == name );
            }
        }

        WHEN( std::string( "a rule is previewed in " ) + name.constData() )
        {
            const auto preview = FooterScanner::preview(
                path, vinAndIdRules(), 0, FooterScanner::kDefaultMaxLines, nullptr, name );

            THEN( "the preview decodes the line as the scan does" )
            {
                REQUIRE( preview.line == QStringLiteral( "VIN: ÄÖ" ) );
                REQUIRE( preview.lineNumber == 2 );
            }
        }
    }

    GIVEN( "a file with a character only windows-1252 has, at 0x80" )
    {
        writeBytes( path, "VIN: \x80\n" );

        WHEN( "it is scanned in windows-1252" )
        {
            const auto scan = scanner.scanFrom( path, {}, FooterScanner::kDefaultMaxLines, nullptr,
                                                "windows-1252" );

            THEN( "it is the euro sign, also where this Qt has no windows-1252 decoder" )
            {
                REQUIRE( scan.values[ "VIN" ].value == QStringLiteral( "€" ) );
            }
        }
    }

    GIVEN( "a file the host reads in an encoding no Qt knows" )
    {
        WHEN( "it is scanned in that encoding" )
        {
            const auto scan = scanner.scanFrom( path, {}, FooterScanner::kDefaultMaxLines, nullptr,
                                                "x-no-such-encoding" );

            THEN( "its lines are split at 0x0A and read as Latin-1" )
            {
                REQUIRE( scan.values[ "VIN" ].value == QStringLiteral( "ÄÖ" ) );
                REQUIRE( scan.values[ "ID" ].lineNumber == 3 );
            }
        }
    }

    GIVEN( "a file read as UTF-8 before" )
    {
        const auto utf8 = scanner.scanFrom( path, {} );
        REQUIRE( utf8.values[ "VIN" ].value != QStringLiteral( "ÄÖ" ) );

        WHEN( "the host names windows-1252 for it, and it is scanned on from that scan" )
        {
            const auto again = scanner.scanFrom(
                path, utf8.progress, FooterScanner::kDefaultMaxLines, nullptr, "windows-1252" );

            THEN( "it is scanned afresh, in windows-1252" )
            {
                REQUIRE_FALSE( again.resumed );
                REQUIRE( again.values[ "VIN" ].value == QStringLiteral( "ÄÖ" ) );
            }
        }
    }
}

SCENARIO( "The encoding the host names wins over a byte order mark",
          "[footerscanner][hostencoding]" )
{
    const QByteArray utf16Mark( "\xFF\xFE" );

    THEN( "no name tells the encoding from the mark, as before" )
    {
        REQUIRE( Encoding::forHost( {}, utf16Mark ) == Encoding::detect( utf16Mark ) );
        REQUIRE( Encoding::forHost( {}, "VIN" ) == Encoding() );
    }

    THEN( "a file with a UTF-16 mark is read as UTF-16 while the host names the locale's UTF-8 "
          "or windows-1252, as before it has detected the encoding" )
    {
        REQUIRE( Encoding::forHost( "UTF-8", utf16Mark ) == Encoding::detect( utf16Mark ) );
        REQUIRE( Encoding::forHost( "windows-1252", utf16Mark ) == Encoding::detect( utf16Mark ) );
    }

    THEN( "a UTF-8 mark is no reason to doubt the host's name" )
    {
        REQUIRE( Encoding::forHost( "windows-1252", "\xEF\xBB\xBF" ).form
                 == Encoding::Form::Named );
    }

    THEN( "UTF-16BE chosen for a file with a UTF-16LE mark keeps the mark as text" )
    {
        const auto encoding = Encoding::forHost( "UTF-16BE", utf16Mark );
        REQUIRE( encoding.form == Encoding::Form::Utf16BE );
        REQUIRE( encoding.textStart == 0 );
    }

    THEN( "UTF-16 without a byte order takes the mark's" )
    {
        REQUIRE( Encoding::forHost( "UTF-16", "\xFE\xFF" ).form == Encoding::Form::Utf16BE );
        REQUIRE( Encoding::forHost( "UTF-16", "\xFE\xFF" ).textStart == 2 );
        REQUIRE( Encoding::forHost( "UTF-16", utf16Mark ).form == Encoding::Form::Utf16LE );
    }

    THEN( "a UTF-32 mark is skipped for UTF-32 of its byte order" )
    {
        const QByteArray utf32Mark( "\xFF\xFE\x00\x00", 4 );
        REQUIRE( Encoding::forHost( "UTF-32LE", utf32Mark ).textStart == 4 );
        REQUIRE( Encoding::forHost( "UTF-32", utf32Mark ).form == Encoding::Form::Utf32LE );
    }

    THEN( "any other name is read with a one-byte line feed, under that name" )
    {
        const auto encoding = Encoding::forHost( "Shift_JIS", "VIN" );
        REQUIRE( encoding.form == Encoding::Form::Named );
        REQUIRE( encoding.name == "Shift_JIS" );
        REQUIRE( encoding.unitBytes() == 1 );
        REQUIRE( encoding.textStart == 0 );
    }
}

SCENARIO( "A rule from a selected line measures the line limit in the host's encoding",
          "[rulefromline][hostencoding]" )
{
    const auto limit = FooterScanner::kMaxLineBytes;

    GIVEN( "a line of more than 64 KiB of \"ä\"" )
    {
        const QString line( limit + 10, QChar( 0x00E4 ) );

        THEN( "in UTF-8, two bytes each, half of them are read" )
        {
            REQUIRE( footerReadLength( line ) == limit / 2 );
        }

        THEN( "in windows-1252 or ISO-8859-1, one byte each, 64 Ki of them are read" )
        {
            REQUIRE( footerReadLength( line, Encoding::forHost( "windows-1252", {} ) ) == limit );
            REQUIRE( footerReadLength( line, Encoding::forHost( "ISO-8859-1", {} ) ) == limit );
        }

        THEN( "in UTF-32, four bytes each, a quarter of them are read" )
        {
            REQUIRE( footerReadLength( line, Encoding::forHost( "UTF-32LE", {} ) ) == limit / 4 );
        }
    }

    GIVEN( "a line of emoji, surrogate pairs, longer than the limit in UTF-32" )
    {
        QString line;
        for ( int i = 0; i < limit / 4 + 10; ++i ) {
            line += QStringLiteral( "\U0001F600" );
        }

        THEN( "in UTF-32 whole code points are read, four bytes each" )
        {
            const auto read = footerReadLength( line, Encoding::forHost( "UTF-32BE", {} ) );
            REQUIRE( read == 2 * ( limit / 4 ) );
            REQUIRE_FALSE( line.at( read ).isLowSurrogate() );
        }
    }

    GIVEN( "a short line" )
    {
        THEN( "all of it is read, in any encoding" )
        {
            REQUIRE( footerReadLength( "VIN: ä", Encoding::forHost( "windows-1252", {} ) ) == 6 );
            REQUIRE( footerReadLength( "VIN: ä", Encoding::forHost( "UTF-32LE", {} ) ) == 6 );
        }
    }
}

SCENARIO( "The live preview reads the active file in the host's encoding",
          "[preview][hostencoding]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/nobom.log";
    writeBytes( path, utf16LE( sampleLog() ) );

    RulePreviewer previewer;
    previewer.setEditDelay( 0 );
    std::vector<FooterScanner::Preview> previews;
    QObject::connect(
        &previewer, &RulePreviewer::previewed,
        [ &previews ]( const FooterScanner::Preview& preview ) { previews.push_back( preview ); } );

    GIVEN( "a rule previewed against the file, which the host reads as UTF-16LE" )
    {
        previewer.setActiveFile( path, "UTF-16LE" );
        previewer.schedule( { { "Id", "^id=(.*)$", "", true, {} } }, 0,
                            RulePreviewer::Start::Soon );

        THEN( "the value is found on the host's Log Line" )
        {
            REQUIRE( processUntil( [ & ] { return !previews.empty(); } ) );
            REQUIRE( previews.back().value->value == "42" );
            REQUIRE( previews.back().lineNumber == 3 );
            REQUIRE( previewer.activeFileEncoding() == "UTF-16LE" );
        }

        WHEN( "the host reads it in another encoding" )
        {
            REQUIRE( processUntil( [ & ] { return !previews.empty(); } ) );
            previewer.setActiveFile( path, "UTF-8" );

            THEN( "it is previewed again at once, in that encoding" )
            {
                REQUIRE( processUntil( [ & ] { return previews.size() >= 2; } ) );
                REQUIRE( previews.back().read.encoding.form == Encoding::Form::Utf8 );
                REQUIRE( previews.back().lineNumber != 3 );
            }
        }
    }
}

namespace {

/// The footer's values as "key=value@line", in the order shown.
QStringList shownWithLines( const FooterDisplayWidget& footer )
{
    QStringList values;
    for ( const auto& value : footer.values() ) {
        values.append(
            QStringLiteral( "%1=%2@%3" ).arg( value.key, value.value ).arg( value.lineNumber ) );
    }
    return values;
}

/// A host that reads every Log File in one encoding, which a test changes,
/// and that remembers which files it was asked about.
struct EncodingHost {
    QByteArray encoding;
    QStringList askedFor;

    HostFunctions host()
    {
        HostFunctions functions;
        functions.logFileEncoding = [ this ]( const QString& filePath ) {
            askedFor.append( filePath );
            return encoding.isEmpty() ? std::nullopt : std::optional<QByteArray>( encoding );
        };
        return functions;
    }
};

} // namespace

SCENARIO( "A footer session reads the active file in the encoding the host reports",
          "[footersession][hostencoding]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), vinAndIdRules() ) );
    const auto noBom = logDir.path() + "/nobom.log";
    const auto latin = logDir.path() + "/latin.log";
    writeBytes( noBom, utf16LE( sampleLog() ) );
    writeBytes( latin, singleByteLog );

    FooterDisplayWidget footer;
    EncodingHost encodingHost;

    GIVEN( "a session on a host that reads the UTF-16LE file without mark as UTF-16LE" )
    {
        encodingHost.encoding = "UTF-16LE";
        FooterSession session( configDir.path(), &footer, encodingHost.host() );
        REQUIRE( session.has( HostFeature::LogFileEncoding ) );
        session.setActiveFile( noBom );

        THEN( "the footer shows its values on the host's Log Lines, asked for that file" )
        {
            REQUIRE( processUntil( [ & ] {
                return shownWithLines( footer ) == QStringList{ "VIN=" + cDot + "AR@2", "ID=42@3" };
            } ) );
            // For the tab in front, which shows the active file.
            REQUIRE( encodingHost.askedFor == QStringList{ QString() } );
        }

        WHEN( "the editor is opened" )
        {
            auto* editor = session.openEditor( nullptr );
            auto* previewer = editor->findChild<RulePreviewer*>( "rulePreviewer" );
            REQUIRE( previewer );

            THEN( "its live preview reads the file in UTF-16LE too" )
            {
                REQUIRE( previewer->activeFileEncoding() == "UTF-16LE" );
            }

            AND_WHEN( "the host reads the file in another encoding" )
            {
                encodingHost.encoding = "UTF-8";
                session.encodingChanged( noBom, "UTF-8" );

                THEN( "the preview follows" )
                {
                    REQUIRE( previewer->activeFileEncoding() == "UTF-8" );
                }
            }
        }
    }

    GIVEN( "a session showing the single-byte file, read as UTF-8 by the host" )
    {
        encodingHost.encoding = "UTF-8";
        FooterSession session( configDir.path(), &footer, encodingHost.host() );
        session.setActiveFile( latin );
        REQUIRE( processUntil( [ & ] { return footer.values().size() == 2; } ) );
        REQUIRE( footer.values().at( 0 ).value != QStringLiteral( "ÄÖ" ) );

        WHEN( "the user chooses windows-1252 for its tab in LogSquirl" )
        {
            encodingHost.encoding = "windows-1252";
            session.encodingChanged( latin, "windows-1252" );

            THEN( "the file is scanned again, and its values decoded in windows-1252" )
            {
                REQUIRE( processUntil( [ & ] {
                    return shownWithLines( footer )
                           == QStringList{ QStringLiteral( "VIN=ÄÖ@2" ),
                                           QStringLiteral( "ID=ß@3" ) };
                } ) );
            }
        }

        WHEN( "the same file's tab in another window is read in another encoding" )
        {
            const auto before = shownWithLines( footer );
            session.encodingChanged( latin, "windows-1252" );

            THEN( "the footer keeps the tab in front's encoding, asked for again" )
            {
                REQUIRE( encodingHost.askedFor.size() == 2 );
                REQUIRE( shownWithLines( footer ) == before );
            }
        }

        WHEN( "another file's encoding changes" )
        {
            const auto before = shownWithLines( footer );
            session.encodingChanged( noBom, "windows-1252" );

            THEN( "the footer stays as it is" )
            {
                REQUIRE( shownWithLines( footer ) == before );
            }
        }
    }

    GIVEN( "a session on a host that names the locale's UTF-8 for a UTF-16 file with mark it "
           "has not loaded yet" )
    {
        const auto withBom = logDir.path() + "/bom.log";
        writeBytes( withBom, "\xFF\xFE" + utf16LE( sampleLog() ) );
        encodingHost.encoding = "UTF-8";
        FooterSession session( configDir.path(), &footer, encodingHost.host() );
        session.setActiveFile( withBom );

        THEN( "the footer reads it as UTF-16LE, told from its mark, as with #42" )
        {
            REQUIRE( processUntil( [ & ] {
                return shownWithLines( footer ) == QStringList{ "VIN=" + cDot + "AR@2", "ID=42@3" };
            } ) );
        }
    }

    GIVEN( "a session on a host without the encoding function" )
    {
        FooterSession session( configDir.path(), &footer, HostFunctions() );
        REQUIRE_FALSE( session.has( HostFeature::LogFileEncoding ) );

        WHEN( "a UTF-16LE file with its byte order mark is shown" )
        {
            const auto withBom = logDir.path() + "/bom.log";
            writeBytes( withBom, "\xFF\xFE" + utf16LE( sampleLog() ) );
            session.setActiveFile( withBom );

            THEN( "it is read as UTF-16LE all the same, told from its mark" )
            {
                REQUIRE( processUntil( [ & ] {
                    return shownWithLines( footer )
                           == QStringList{ "VIN=" + cDot + "AR@2", "ID=42@3" };
                } ) );
            }
        }
    }
}
