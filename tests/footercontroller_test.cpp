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
 * @file footercontroller_test.cpp
 * @brief BDD tests for the background scanning of the active file.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footercontroller.h"
#include "footerdisplaywidget.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include <functional>
#include <memory>

using namespace custom_footer;

namespace {

void writeFile( const QString& path, const QByteArray& content, bool append = false )
{
    QFile file( path );
    REQUIRE( file.open( append ? QIODevice::Append : QIODevice::WriteOnly ) );
    file.write( content );
}

/// Process events until the condition holds or the timeout expires.
bool waitFor( const std::function<bool()>& condition, int timeoutMs = 5000 )
{
    QElapsedTimer timer;
    timer.start();
    while ( !condition() ) {
        if ( timer.elapsed() > timeoutMs ) {
            return false;
        }
        QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
        QThread::msleep( 5 );
    }
    return true;
}

/// Process events for a while, for checks that something does not happen.
void settle( int ms = 300 )
{
    waitFor( [] { return false; }, ms );
}

QString shownText( const FooterDisplayWidget& widget )
{
    QStringList shown;
    for ( const auto* item : widget.findChildren<FooterValueItem*>() ) {
        shown.append( item->value().key + ": " + item->value().value );
    }
    return shown.join( " | " );
}

} // namespace

SCENARIO( "FooterController scans the active file in the background", "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );

    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), entries ) );

    const auto first = logDir.path() + "/first.log";
    const auto second = logDir.path() + "/second.log";
    writeFile( first, "VIN: FIRST\n" );
    writeFile( second, "VIN: SECOND\n" );

    FooterDisplayWidget widget;

    GIVEN( "a controller for the widget" )
    {
        FooterController controller( &widget, configDir.path() );

        WHEN( "a file becomes active" )
        {
            controller.setActiveFile( first );

            THEN( "its values are shown" )
            {
                REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "FIRST" ); } ) );
            }
        }

        WHEN( "the active file changes before the first scan is shown" )
        {
            controller.setActiveFile( first );
            controller.setActiveFile( second );

            THEN( "only the values of the newer file end up shown" )
            {
                REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "SECOND" ); } ) );
                settle();
                REQUIRE_FALSE( shownText( widget ).contains( "FIRST" ) );
            }
        }

        WHEN( "another file becomes active" )
        {
            controller.setActiveFile( first );
            REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "FIRST" ); } ) );
            controller.setActiveFile( second );

            THEN( "the values of the previous file are gone at once" )
            {
                REQUIRE_FALSE( shownText( widget ).contains( "FIRST" ) );
                REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "SECOND" ); } ) );
            }
        }

        WHEN( "no file is active" )
        {
            controller.setActiveFile( first );
            REQUIRE( waitFor( [ & ] { return !shownText( widget ).isEmpty(); } ) );
            controller.setActiveFile( QString() );

            THEN( "the footer is cleared" )
            {
                REQUIRE( shownText( widget ).isEmpty() );
            }
        }

        WHEN( "the rules change on disk" )
        {
            controller.setActiveFile( first );
            REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "FIRST" ); } ) );

            QList<FooterEntry> renamed;
            renamed.append( { "Vehicle", "VIN:\\s+(\\S+)", "", true, {} } );
            REQUIRE( FooterConfig::saveEntries( configDir.path(), renamed ) );

            THEN( "they are used only after a reload" )
            {
                controller.setActiveFile( first );
                settle();
                REQUIRE_FALSE( shownText( widget ).contains( "Vehicle" ) );

                controller.reloadConfig();
                REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "Vehicle" ); } ) );
            }
        }
    }

    GIVEN( "an active file that grows" )
    {
        const auto growing = logDir.path() + "/growing.log";
        writeFile( growing, "starting up\n" );

        FooterController controller( &widget, configDir.path() );
        controller.setActiveFile( growing );
        settle();
        REQUIRE( shownText( widget ).isEmpty() );

        WHEN( "a matching line is appended" )
        {
            writeFile( growing, "VIN: LATER\n", true );

            THEN( "the new value is shown without switching files" )
            {
                REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "LATER" ); } ) );
            }
        }
    }

    GIVEN( "an active file whose values are shown" )
    {
        const auto rotating = logDir.path() + "/rotating.log";
        writeFile( rotating, "VIN: BEFORE-ROTATION\nmore lines\n" );

        FooterController controller( &widget, configDir.path() );
        controller.setActiveFile( rotating );
        REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "BEFORE-ROTATION" ); } ) );

        WHEN( "it is truncated and written again" )
        {
            writeFile( rotating, "VIN: AFTER\n" );

            THEN( "the new values are shown" )
            {
                REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "AFTER" ); } ) );
            }
        }

        WHEN( "it is rotated away and recreated only later" )
        {
            REQUIRE( QFile::rename( rotating, rotating + ".1" ) );
            REQUIRE( waitFor( [ & ] { return controller.isWaitingForActiveFile(); } ) );
            writeFile( rotating, "VIN: RECREATED\n" );

            THEN( "the recreated file is scanned" )
            {
                REQUIRE( waitFor( [ & ] { return shownText( widget ).contains( "RECREATED" ); } ) );

                AND_THEN( "it is watched again" )
                {
                    writeFile( rotating, "VIN: SECOND-ROTATION-WITH-A-LONGER-VALUE\n" );
                    REQUIRE( waitFor(
                        [ & ] { return shownText( widget ).contains( "SECOND-ROTATION" ); } ) );
                }
            }
        }
    }
}

SCENARIO( "FooterController can be destroyed during a scan", "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );

    // Scan everything: the rule never matches, so the scan reads the whole file.
    QList<FooterEntry> entries;
    entries.append( { "Never", "^NEVER-MATCHES-(\\d+)$", "", true, {} } );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), entries ) );
    REQUIRE( FooterConfig::saveMaxLines( configDir.path(), 0 ) );

    const auto big = logDir.path() + "/big.log";
    const QByteArray line( "2026-01-01 INFO some ordinary log line with nothing to find\n" );
    writeFile( big, line.repeated( 400000 ) );

    GIVEN( "a scan of a large file in progress" )
    {
        FooterDisplayWidget widget;
        auto controller = std::make_unique<FooterController>( &widget, configDir.path() );
        controller->setActiveFile( big );

        WHEN( "the controller is destroyed right away" )
        {
            QElapsedTimer timer;
            timer.start();
            controller.reset();

            THEN( "it stops the scan promptly and shows nothing afterwards" )
            {
                REQUIRE( timer.elapsed() < 2000 );
                settle();
                REQUIRE( shownText( widget ).isEmpty() );
            }
        }
    }
}

SCENARIO( "FooterController shows the values of a log that changes faster than it is scanned",
          "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );

    // The VIN is on the first line, but the other rules never match: every
    // scan reads the whole file, which takes longer than the rescan delay.
    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    for ( int i = 0; i < 3; ++i ) {
        entries.append( { QString( "Never%1" ).arg( i ),
                          QString( "^NEVER-%1-MATCHES-(\\d+)$" ).arg( i ),
                          "",
                          true,
                          {} } );
    }
    REQUIRE( FooterConfig::saveEntries( configDir.path(), entries ) );
    REQUIRE( FooterConfig::saveMaxLines( configDir.path(), 0 ) );

    const auto busy = logDir.path() + "/busy.log";
    const QByteArray line( "2026-01-01 INFO some ordinary log line with nothing to find\n" );
    writeFile( busy, "VIN: BUSY\n" + line.repeated( 400000 ) );

    // Append to the log every few milliseconds while waiting.
    QElapsedTimer sinceAppend;
    sinceAppend.start();
    const auto appendWhile = [ & ]( const std::function<bool()>& condition ) {
        return waitFor(
            [ & ] {
                if ( sinceAppend.elapsed() >= 50 ) {
                    writeFile( busy, line, true );
                    sinceAppend.restart();
                }
                return condition();
            },
            30000 );
    };

    FooterDisplayWidget widget;
    FooterController controller( &widget, configDir.path() );

    GIVEN( "a busy log becomes active" )
    {
        controller.setActiveFile( busy );

        WHEN( "lines keep being appended faster than the file is scanned" )
        {
            THEN( "its values are shown nonetheless" )
            {
                REQUIRE( appendWhile( [ & ] { return shownText( widget ).contains( "BUSY" ); } ) );

                AND_THEN( "values appended while it was scanned follow" )
                {
                    writeFile( busy, "NEVER-0-MATCHES-4711\n", true );
                    REQUIRE(
                        appendWhile( [ & ] { return shownText( widget ).contains( "4711" ); } ) );
                }
            }
        }

        WHEN( "it is made active again and again while it is scanned" )
        {
            THEN( "its values are shown nonetheless" )
            {
                QElapsedTimer sinceActivated;
                sinceActivated.start();
                REQUIRE( waitFor(
                    [ & ] {
                        if ( sinceActivated.elapsed() >= 100 ) {
                            controller.setActiveFile( busy );
                            sinceActivated.restart();
                        }
                        return shownText( widget ).contains( "BUSY" );
                    },
                    30000 ) );
            }
        }
    }
}

SCENARIO( "FooterController shows where each value came from", "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );

    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    entries.append( { "Mode", "mode=(\\S+)", "", true, { { "0x04", "Production" } } } );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), entries ) );

    const auto log = logDir.path() + "/mapped.log";
    writeFile( log, "mode=0x04\nVIN: ABC\n" );

    FooterDisplayWidget widget;
    FooterController controller( &widget, configDir.path() );

    GIVEN( "a file with a mapped and an unmapped value" )
    {
        controller.setActiveFile( log );

        THEN( "the widget gets the raw value and the rule of each shown value" )
        {
            REQUIRE( waitFor( [ & ] { return widget.values().size() == 2; } ) );
            REQUIRE( widget.values()[ 0 ] == FooterValue{ "VIN", "ABC", "ABC", 0 } );
            REQUIRE( widget.values()[ 1 ] == FooterValue{ "Mode", "Production", "0x04", 1 } );
        }
    }
}
