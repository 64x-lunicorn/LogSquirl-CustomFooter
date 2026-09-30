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
 * @file activefilewatcher_test.cpp
 * @brief BDD tests for the watcher of the active file, shared by the footer
 *        and the editor's preview.
 */

#include <catch2/catch.hpp>

#include "activefilewatcher.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

using namespace custom_footer;

namespace {

/// Process events until @p done, for at most a generous bound: file system
/// events arrive when the system delivers them.
template <typename Predicate>
bool processUntil( Predicate done, int boundMs = 10000 )
{
    QElapsedTimer timer;
    timer.start();
    while ( !done() ) {
        if ( timer.elapsed() > boundMs ) {
            return false;
        }
        QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
    }
    return true;
}

void writeBytes( const QString& path, const QByteArray& content, bool append = false )
{
    QFile file( path );
    REQUIRE( file.open( append ? QIODevice::Append : QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

} // namespace

SCENARIO( "ActiveFileWatcher follows the active file", "[activefilewatcher]" )
{
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto first = dir.path() + "/first.log";
    const auto second = dir.path() + "/second.log";
    writeBytes( first, "a\n" );
    writeBytes( second, "b\n" );

    ActiveFileWatcher watcher( 0 );
    int changes = 0;
    QObject::connect( &watcher, &ActiveFileWatcher::changed, [ &changes ] { ++changes; } );

    GIVEN( "a watcher that switched from one file to another" )
    {
        watcher.setFile( first );
        watcher.watch();
        watcher.setFile( second );
        watcher.watch();

        WHEN( "a change of the first file, still queued, arrives" )
        {
            REQUIRE( QMetaObject::invokeMethod( &watcher, "fileChanged", Qt::DirectConnection,
                                                Q_ARG( QString, first ) ) );

            THEN( "it is ignored, and the second file stays watched" )
            {
                REQUIRE_FALSE( watcher.isPending() );
                REQUIRE( watcher.watchedPaths() == QStringList{ second } );
            }
        }
    }

    GIVEN( "a watched file" )
    {
        watcher.setFile( first );
        watcher.watch();

        WHEN( "it grows" )
        {
            writeBytes( first, "more\n", true );

            THEN( "a change is reported" )
            {
                REQUIRE( processUntil( [ &changes ] { return changes == 1; } ) );
            }
        }

        WHEN( "it is deleted, and read again by its owner" )
        {
            REQUIRE( QFile::remove( first ) );
            REQUIRE( processUntil( [ &changes ] { return changes == 1; } ) );
            watcher.watch();

            THEN( "its directory is watched instead" )
            {
                REQUIRE( watcher.isWaitingForFile() );
                REQUIRE( watcher.watchedPaths() == QStringList{ dir.path() } );
            }

            AND_WHEN( "it is created again" )
            {
                writeBytes( first, "new\n" );

                THEN( "a change is reported, and the file is watched again once read" )
                {
                    REQUIRE( processUntil( [ &changes ] { return changes == 2; } ) );
                    watcher.watch();
                    REQUIRE_FALSE( watcher.isWaitingForFile() );
                    REQUIRE( watcher.watchedPaths() == QStringList{ first } );
                }
            }
        }
    }
}
