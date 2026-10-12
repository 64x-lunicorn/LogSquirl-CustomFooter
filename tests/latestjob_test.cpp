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
 * @file latestjob_test.cpp
 * @brief BDD tests for LatestJob: counting started and finished jobs,
 *        including the results stop() drops.
 */

#include <catch2/catch.hpp>

#include "latestjob.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QSemaphore>
#include <QThread>

#include <atomic>

using namespace custom_footer;

namespace {

/// Process events until @p done, for at most a generous bound: a hung
/// worker fails the test instead of hanging it. Nothing depends on timing.
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
        QThread::msleep( 1 );
    }
    return true;
}

} // namespace

SCENARIO( "LatestJob counts the results stop() drops as finished", "[latestjob]" )
{
    GIVEN( "a job that returned on the worker, its result queued but not handed over" )
    {
        QObject context;
        LatestJob<int> jobs( &context );
        QSemaphore returned;
        int handedOver = 0;
        jobs.run(
            [ &returned ]( const std::atomic_bool* ) {
                returned.release();
                return 1;
            },
            [ &handedOver ]( int ) { ++handedOver; } );
        returned.acquire();

        WHEN( "stop() drops it" )
        {
            jobs.stop();

            THEN( "it counts as finished, and its result is not handed over" )
            {
                CHECK( jobs.started() == 1 );
                CHECK( jobs.finished() == jobs.started() );
                CHECK_FALSE( jobs.isRunning() );
                QCoreApplication::processEvents();
                CHECK( handedOver == 0 );
            }

            AND_WHEN( "another job runs to completion" )
            {
                jobs.run( []( const std::atomic_bool* ) { return 2; },
                          [ &handedOver ]( int ) { ++handedOver; } );

                THEN( "every started job counts as finished once" )
                {
                    REQUIRE( processUntil( [ & ] { return handedOver == 1; } ) );
                    CHECK( jobs.started() == 2 );
                    CHECK( jobs.finished() == 2 );
                }
            }
        }
    }

    GIVEN( "a job whose result was handed over" )
    {
        QObject context;
        LatestJob<int> jobs( &context );
        int handedOver = 0;
        jobs.run( []( const std::atomic_bool* ) { return 1; },
                  [ &handedOver ]( int ) { ++handedOver; } );
        REQUIRE( processUntil( [ & ] { return handedOver == 1; } ) );

        WHEN( "stop() is called" )
        {
            jobs.stop();

            THEN( "the job is not counted twice" )
            {
                CHECK( jobs.started() == 1 );
                CHECK( jobs.finished() == 1 );
            }
        }
    }
}
