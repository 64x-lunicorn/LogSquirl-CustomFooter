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
 * @brief BDD tests for the background scanning of the active file. The
 *        follower's clock and change source are the test's own: the test
 *        reports the file's changes and moves the time itself. Only the
 *        scans run on the real worker thread, on real files.
 */

#include <catch2/catch.hpp>

#include "followerfakes.h"
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

constexpr int kGap = FollowerParts::kGapMs;

void writeFile( const QString& path, const QByteArray& content, bool append = false )
{
    QFile file( path );
    REQUIRE( file.open( append ? QIODevice::Append : QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

/// Process events until @p done, for at most a generous bound: a hung
/// worker fails the test instead of hanging it. Nothing depends on timing.
bool processUntil( const std::function<bool()>& done, int boundMs = 10000 )
{
    QElapsedTimer timer;
    timer.start();
    while ( !done() ) {
        if ( timer.elapsed() > boundMs ) {
            return false;
        }
        QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
        QThread::msleep( 1 );
    }
    return true;
}

/// Process events until every scan started has finished, shown or dropped.
bool scansSettle( const FooterController& controller )
{
    return processUntil(
        [ &controller ] { return controller.scansFinished() == controller.scansStarted(); } );
}

QString shownText( const FooterDisplayWidget& widget )
{
    QStringList shown;
    for ( const auto& value : widget.values() ) {
        shown.append( value.key + ": " + value.value );
    }
    return shown.join( " | " );
}

/// The follower's parts for a controller: a clock and a change source the
/// test keeps a hold of.
struct Fakes {
    Fakes()
    {
        auto manualClock = std::make_unique<ManualClock>();
        auto fakeChanges = std::make_unique<FakeChangeSource>();
        clock = manualClock.get();
        changes = fakeChanges.get();
        parts = FollowerParts{ std::move( manualClock ), std::move( fakeChanges ) };
    }

    ManualClock* clock = nullptr;
    FakeChangeSource* changes = nullptr;
    FollowerParts parts;
};

/// A controller on @p widget with the test's clock and change source.
std::unique_ptr<FooterController> makeController( FooterDisplayWidget& widget,
                                                  const QString& configDir, Fakes& fakes )
{
    return std::make_unique<FooterController>( &widget, configDir, FooterController::Warn{},
                                               nullptr, std::move( fakes.parts ) );
}

} // namespace

SCENARIO( "FooterController scans the active file in the background", "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );

    const auto first = logDir.path() + "/first.log";
    const auto second = logDir.path() + "/second.log";
    writeFile( first, "VIN: FIRST\n" );
    writeFile( second, "VIN: SECOND\n" );

    FooterDisplayWidget widget;
    Fakes fakes;
    auto controller = makeController( widget, configDir.path(), fakes );

    WHEN( "a file becomes active" )
    {
        controller->setActiveFile( first );

        THEN( "its values are shown, without waiting for a pause" )
        {
            REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: FIRST"; } ) );
            REQUIRE( fakes.clock->now() == 0 );
        }

        THEN( "its changes are followed" )
        {
            REQUIRE( fakes.changes->file == first );
        }
    }

    WHEN( "the active file changes before the first scan is shown" )
    {
        controller->setActiveFile( first );
        controller->setActiveFile( second );

        THEN( "only the values of the newer file end up shown" )
        {
            REQUIRE( scansSettle( *controller ) );
            REQUIRE( controller->scansStarted() == 2 );
            REQUIRE( shownText( widget ) == "VIN: SECOND" );
            REQUIRE( fakes.changes->file == second );
        }
    }

    WHEN( "another file becomes active" )
    {
        controller->setActiveFile( first );
        REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: FIRST"; } ) );
        controller->setActiveFile( second );

        THEN( "the values of the previous file are gone at once" )
        {
            REQUIRE( shownText( widget ).isEmpty() );
            REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: SECOND"; } ) );
        }
    }

    WHEN( "no file is active" )
    {
        controller->setActiveFile( first );
        REQUIRE( processUntil( [ & ] { return !shownText( widget ).isEmpty(); } ) );
        controller->setActiveFile( QString() );

        THEN( "the footer is cleared and nothing is followed" )
        {
            REQUIRE( shownText( widget ).isEmpty() );
            REQUIRE( fakes.changes->file.isEmpty() );

            const int scans = controller->scansStarted();
            fakes.changes->fire();
            fakes.clock->advance( kGap );
            REQUIRE( controller->scansStarted() == scans );
        }
    }

    WHEN( "the rules change on disk" )
    {
        controller->setActiveFile( first );
        REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: FIRST"; } ) );
        REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                            { { "Vehicle", "VIN:\\s+(\\S+)", "", true, {} } } ) );

        THEN( "they are used only after a reload" )
        {
            // A real scan with the rules loaded before: the file was rewritten.
            const int scans = controller->scansStarted();
            writeFile( first, "VIN: REWRITTEN\n" );
            fakes.changes->fire();
            fakes.clock->advance( kGap );
            REQUIRE( controller->scansStarted() == scans + 1 );
            REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: REWRITTEN"; } ) );

            controller->reloadConfig();
            REQUIRE(
                processUntil( [ & ] { return shownText( widget ) == "Vehicle: REWRITTEN"; } ) );
        }
    }
}

SCENARIO( "FooterController scans afresh with rules reloaded while it scans", "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} },
                                          { "Missing", "never=(\\S+)", "", true, {} } } ) );

    const auto path = logDir.path() + "/reloaded.log";
    writeFile( path, "VIN: FIRST\nid=1\n" );

    FooterDisplayWidget widget;
    Fakes fakes;
    auto controller = makeController( widget, configDir.path(), fakes );

    GIVEN( "a scanned file, with a key still missing, being scanned on" )
    {
        controller->setActiveFile( path );
        REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: FIRST"; } ) );
        writeFile( path, "more\n", true );
        fakes.changes->fire();
        fakes.clock->advance( kGap );
        // Running until its result is handled here, as events are not processed yet.
        REQUIRE( controller->scansStarted() == 2 );

        WHEN( "other rules are saved and reloaded" )
        {
            REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                                { { "Vehicle", "VIN:\\s+(\\S+)", "", true, {} },
                                                  { "ID", "id=(\\S+)", "", true, {} } } ) );
            controller->reloadConfig();

            THEN( "the running scan is dropped, and the file scanned at once from its start" )
            {
                REQUIRE( controller->scansStarted() == 3 );
                REQUIRE( scansSettle( *controller ) );
                REQUIRE( controller->scansFinished() == 3 );
                // Both values are in lines the earlier scans had read already.
                REQUIRE( shownText( widget ) == "Vehicle: FIRST | ID: 1" );
                REQUIRE( fakes.clock->now() == kGap );
            }
        }
    }
}

SCENARIO( "FooterController scans on where it stopped when the active file grows",
          "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE(
        FooterConfig::saveEntries( configDir.path(), { { "VIN", "VIN:\\s+(\\S+)", "", true, {} },
                                                       { "ID", "id=(\\S+)", "", true, {} } } ) );

    const auto growing = logDir.path() + "/growing.log";
    writeFile( growing, "starting up\n" );

    FooterDisplayWidget widget;
    Fakes fakes;
    auto controller = makeController( widget, configDir.path(), fakes );

    GIVEN( "an active file without values yet" )
    {
        controller->setActiveFile( growing );
        REQUIRE( scansSettle( *controller ) );
        REQUIRE( shownText( widget ).isEmpty() );

        WHEN( "a matching line is appended and reported" )
        {
            writeFile( growing, "VIN: LATER\n", true );
            fakes.changes->fire();

            THEN( "it is scanned a pause later" )
            {
                fakes.clock->advance( kGap - 1 );
                REQUIRE( controller->scansStarted() == 1 );

                fakes.clock->advance( 1 );
                REQUIRE( controller->scansStarted() == 2 );
                REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: LATER"; } ) );

                AND_THEN( "the values found before are kept as it grows on" )
                {
                    writeFile( growing, "id=7\n", true );
                    fakes.changes->fire();
                    fakes.clock->advance( kGap );
                    REQUIRE( processUntil(
                        [ & ] { return shownText( widget ) == "VIN: LATER | ID: 7"; } ) );
                }
            }
        }
    }
}

SCENARIO( "FooterController scans again after a change during a scan, a gap after it",
          "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE(
        FooterConfig::saveEntries( configDir.path(), { { "VIN", "VIN:\\s+(\\S+)", "", true, {} },
                                                       { "ID", "id=(\\S+)", "", true, {} } } ) );

    const auto path = logDir.path() + "/changing.log";
    writeFile( path, "VIN: FIRST\n" );

    FooterDisplayWidget widget;
    Fakes fakes;
    auto controller = makeController( widget, configDir.path(), fakes );

    GIVEN( "a scan of the active file running" )
    {
        // Running until its result is handled here, as events are not processed yet.
        controller->setActiveFile( path );

        WHEN( "the file changes, more than once" )
        {
            fakes.changes->fire();
            fakes.changes->fire();
            controller->setActiveFile( path );

            THEN( "the running scan is not cancelled but shown" )
            {
                REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: FIRST"; } ) );
                REQUIRE( controller->scansStarted() == 1 );
            }

            THEN( "one more scan follows, the follower's gap after it, never back to back" )
            {
                REQUIRE( scansSettle( *controller ) );
                writeFile( path, "id=7\n", true );
                REQUIRE( fakes.clock->wakeTime() == fakes.clock->now() + kGap );

                fakes.clock->advance( kGap - 1 );
                REQUIRE( controller->scansStarted() == 1 );

                fakes.clock->advance( 1 );
                REQUIRE( controller->scansStarted() == 2 );
                REQUIRE(
                    processUntil( [ & ] { return shownText( widget ) == "VIN: FIRST | ID: 7"; } ) );
                REQUIRE( controller->scansStarted() == 2 );
                REQUIRE_FALSE( fakes.clock->wakeTime() );
            }
        }
    }
}

SCENARIO( "FooterController does not rescan a file it only read", "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );

    const auto path = logDir.path() + "/read.log";
    FooterDisplayWidget widget;
    Fakes fakes;

    GIVEN( "a scanned file, with a key still missing" )
    {
        REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                            { { "VIN", "VIN:\\s+(\\S+)", "", true, {} },
                                              { "Missing", "never=(\\S+)", "", true, {} } } ) );
        writeFile( path, "VIN: READ\n" );
        auto controller = makeController( widget, configDir.path(), fakes );
        controller->setActiveFile( path );
        REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: READ"; } ) );
        const int scans = controller->scansStarted();
        const int rearms = fakes.changes->rearms;

        WHEN( "a change is reported that left the file as it was, and the pause passes" )
        {
            // As Linux and Windows may report a read, e.g. for its access
            // time; which notifications count is tested with FileChangeSource.
            fakes.changes->fire();
            fakes.clock->advance( kGap );

            THEN( "it is not scanned again, but watched on" )
            {
                REQUIRE( controller->scansStarted() == scans );
                REQUIRE( fakes.changes->rearms == rearms + 1 );

                AND_THEN( "a later change of it is scanned" )
                {
                    writeFile( path, "never=more\n", true );
                    fakes.changes->fire();
                    fakes.clock->advance( kGap );
                    REQUIRE( controller->scansStarted() == scans + 1 );
                    REQUIRE( processUntil(
                        [ & ] { return shownText( widget ) == "VIN: READ | Missing: more"; } ) );
                }
            }
        }
    }

    GIVEN( "a scanned file in which every key has a value" )
    {
        REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                            { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );
        writeFile( path, "VIN: COMPLETE\n" );
        auto controller = makeController( widget, configDir.path(), fakes );
        controller->setActiveFile( path );
        REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: COMPLETE"; } ) );
        const int scans = controller->scansStarted();

        WHEN( "it only grows" )
        {
            writeFile( path, "VIN: LATER\nmore lines\n", true );
            fakes.changes->fire();
            fakes.clock->advance( kGap );

            THEN( "it is not scanned again, as 0.4.0 did not" )
            {
                REQUIRE( controller->scansStarted() == scans );
                REQUIRE( shownText( widget ) == "VIN: COMPLETE" );
            }
        }

        WHEN( "it is replaced" )
        {
            REQUIRE( QFile::remove( path ) );
            writeFile( path, "VIN: REPLACEMENT\nand more lines\n" );
            fakes.changes->fire();
            fakes.clock->advance( kGap );

            THEN( "it is scanned from its start" )
            {
                REQUIRE( controller->scansStarted() == scans + 1 );
                REQUIRE(
                    processUntil( [ & ] { return shownText( widget ) == "VIN: REPLACEMENT"; } ) );
            }
        }
    }
}

SCENARIO( "FooterController scans a truncated or rotated file from its start",
          "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );

    const auto rotating = logDir.path() + "/rotating.log";
    writeFile( rotating, "VIN: BEFORE-ROTATION\nmore lines\n" );

    FooterDisplayWidget widget;
    Fakes fakes;
    auto controller = makeController( widget, configDir.path(), fakes );

    GIVEN( "an active file whose values are shown" )
    {
        controller->setActiveFile( rotating );
        REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: BEFORE-ROTATION"; } ) );

        WHEN( "it is truncated and written again" )
        {
            writeFile( rotating, "VIN: AFTER\n" );
            fakes.changes->fire();
            fakes.clock->advance( kGap );

            THEN( "the new values are shown" )
            {
                REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: AFTER"; } ) );
            }
        }

        WHEN( "it is rotated away" )
        {
            REQUIRE( QFile::rename( rotating, rotating + ".1" ) );
            fakes.changes->waiting = true;
            fakes.changes->fire();
            fakes.clock->advance( kGap );

            THEN( "its values are gone, and its return is waited for" )
            {
                REQUIRE( processUntil( [ & ] { return shownText( widget ).isEmpty(); } ) );
                REQUIRE( controller->isWaitingForActiveFile() );

                AND_WHEN( "it is recreated" )
                {
                    REQUIRE( scansSettle( *controller ) );
                    writeFile( rotating, "VIN: RECREATED\n" );
                    fakes.changes->waiting = false;
                    fakes.changes->fire();
                    fakes.clock->advance( kGap );

                    THEN( "the recreated file is scanned, and followed on" )
                    {
                        REQUIRE( processUntil(
                            [ & ] { return shownText( widget ) == "VIN: RECREATED"; } ) );
                        REQUIRE_FALSE( controller->isWaitingForActiveFile() );

                        writeFile( rotating, "VIN: SECOND-ROTATION-WITH-A-LONGER-VALUE\n" );
                        fakes.changes->fire();
                        fakes.clock->advance( kGap );
                        REQUIRE( processUntil( [ & ] {
                            return shownText( widget )
                                   == "VIN: SECOND-ROTATION-WITH-A-LONGER-VALUE";
                        } ) );
                    }
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
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "Never", "^NEVER-MATCHES-(\\d+)$", "", true, {} } } ) );
    REQUIRE( FooterConfig::saveMaxLines( configDir.path(), 0 ) );

    const auto big = logDir.path() + "/big.log";
    const QByteArray line( "2026-01-01 INFO some ordinary log line with nothing to find\n" );
    writeFile( big, line.repeated( 400000 ) + "NEVER-MATCHES-1\n" );

    GIVEN( "a scan of a large file in progress, with a change of it reported" )
    {
        FooterDisplayWidget widget;
        Fakes fakes;
        auto controller = makeController( widget, configDir.path(), fakes );
        controller->setActiveFile( big );
        fakes.changes->fire();

        WHEN( "the controller is destroyed right away" )
        {
            QElapsedTimer timer;
            timer.start();
            controller.reset();

            THEN( "it stops the scan promptly, and nothing is shown or scanned afterwards" )
            {
                REQUIRE( timer.elapsed() < 2000 );
                QCoreApplication::processEvents();
                REQUIRE( shownText( widget ).isEmpty() );
            }
        }
    }
}

SCENARIO( "FooterController shows the values of a log that changes while it is scanned",
          "[footercontroller]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} },
                                          { "Late", "^LATE-(\\d+)$", "", true, {} } } ) );

    const auto busy = logDir.path() + "/busy.log";
    writeFile( busy, "VIN: BUSY\nsome ordinary log line\n" );

    FooterDisplayWidget widget;
    Fakes fakes;
    auto controller = makeController( widget, configDir.path(), fakes );

    GIVEN( "a busy log becomes active" )
    {
        // The scan runs until its result is handled here, which no change cancels.
        controller->setActiveFile( busy );

        WHEN( "changes keep being reported while it is scanned" )
        {
            for ( int i = 0; i < 10; ++i ) {
                writeFile( busy, "some ordinary log line\n", true );
                fakes.changes->fire();
            }

            THEN( "its values are shown nonetheless" )
            {
                REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: BUSY"; } ) );
                REQUIRE( controller->scansStarted() == 1 );

                AND_THEN( "values appended while it was scanned follow" )
                {
                    writeFile( busy, "LATE-4711\n", true );
                    fakes.clock->advance( kGap );
                    REQUIRE( processUntil(
                        [ & ] { return shownText( widget ) == "VIN: BUSY | Late: 4711"; } ) );
                }
            }
        }

        WHEN( "it is made active again and again while it is scanned" )
        {
            for ( int i = 0; i < 10; ++i ) {
                controller->setActiveFile( busy );
            }

            THEN( "its values are shown nonetheless" )
            {
                REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: BUSY"; } ) );
                REQUIRE( controller->scansStarted() == 1 );
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
    REQUIRE( FooterConfig::saveEntries(
        configDir.path(), { { "VIN", "VIN:\\s+(\\S+)", "", true, {} },
                            { "Mode", "mode=(\\S+)", "", true, { { "0x04", "Production" } } } } ) );

    const auto log = logDir.path() + "/mapped.log";
    writeFile( log, "mode=0x04\nVIN: ABC\n" );

    FooterDisplayWidget widget;
    Fakes fakes;
    auto controller = makeController( widget, configDir.path(), fakes );

    GIVEN( "a file with a mapped and an unmapped value" )
    {
        controller->setActiveFile( log );

        THEN( "the widget gets the raw value and the rule of each shown value" )
        {
            REQUIRE( processUntil( [ & ] { return widget.values().size() == 2; } ) );
            REQUIRE( widget.values()[ 0 ] == FooterValue{ "VIN", "ABC", "ABC", 0, 2 } );
            REQUIRE( widget.values()[ 1 ] == FooterValue{ "Mode", "Production", "0x04", 1, 1 } );
        }
    }
}

SCENARIO( "FooterController follows a growing log on disk", "[footercontroller][smoke]" )
{
    // The production clock and change source, on a real file: only that the
    // parts are wired up, with a bound far above the follower's gap.
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );

    const auto growing = logDir.path() + "/growing.log";
    writeFile( growing, "starting up\n" );

    FooterDisplayWidget widget;
    FooterController controller( &widget, configDir.path() );

    GIVEN( "an active file without values yet" )
    {
        controller.setActiveFile( growing );
        REQUIRE( scansSettle( controller ) );

        WHEN( "a matching line is appended" )
        {
            writeFile( growing, "VIN: LATER\n", true );

            THEN( "the new value is shown without switching files" )
            {
                REQUIRE( processUntil( [ & ] { return shownText( widget ) == "VIN: LATER"; } ) );
            }
        }
    }
}
