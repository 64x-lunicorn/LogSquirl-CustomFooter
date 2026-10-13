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
 * @file filefollower_test.cpp
 * @brief BDD tests for the file follower: reruns after changes, the gap
 *        between runs, cancelling on new inputs, skipping changes that cannot
 *        matter, and shutting down. The clock and the change source are the
 *        test's own; only the jobs run on the real worker thread.
 */

#include <catch2/catch.hpp>

#include "filefollower.h"
#include "followerfakes.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMutex>
#include <QSemaphore>
#include <QThread>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace custom_footer;

namespace {

using Follower = FileFollower<int>;

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

/// Jobs that number their results from @p first and record what they were
/// given; they may hold the worker until the test releases them, or until
/// they are cancelled.
struct Jobs {
    enum class Hold { No, UntilReleased, UntilCancelled };

    explicit Jobs( int first = 1 )
        : next( first )
    {
    }

    Follower::Job job()
    {
        return [ this ]( const QString& file, const int* previous,
                         const std::atomic_bool& cancelled ) -> int {
            {
                QMutexLocker lock( &mutex );
                files.append( file );
                previousResults.push_back( previous ? std::optional<int>( *previous )
                                                    : std::nullopt );
            }
            started.release();
            if ( hold == Hold::UntilReleased ) {
                release.acquire();
            }
            else if ( hold == Hold::UntilCancelled ) {
                // Bounded, so that a missing cancel fails instead of hanging.
                for ( int i = 0; i < 10000 && !cancelled.load(); ++i ) {
                    QThread::msleep( 1 );
                }
            }
            sawCancel = cancelled.load();
            if ( throwOnce.exchange( false ) ) {
                throw std::runtime_error( "unreadable" );
            }
            const int result = next++;
            ++returned;
            return result;
        };
    }

    /// Wait, without handling events, until @p runs jobs have returned, and
    /// a little longer, so that their results are likely, though not
    /// guaranteed, on their way to the context's thread. The tests that use
    /// it hold either way: a result that is not yet on its way when the
    /// test goes on is dropped all the same, only later.
    bool waitReturned( int runs )
    {
        QElapsedTimer timer;
        timer.start();
        while ( returned < runs ) {
            if ( timer.elapsed() > 10000 ) {
                return false;
            }
            QThread::msleep( 1 );
        }
        // The worker hands the result over just after the job returned.
        QThread::msleep( 20 );
        return true;
    }

    /// What the runs got as the previous result, in order.
    std::vector<std::optional<int>> previous()
    {
        QMutexLocker lock( &mutex );
        return previousResults;
    }

    std::atomic<Hold> hold{ Hold::No };
    std::atomic_bool throwOnce{ false };
    std::atomic_bool sawCancel{ false };
    std::atomic_int next;
    std::atomic_int returned{ 0 };
    QSemaphore started;
    QSemaphore release;
    QMutex mutex;
    QStringList files;
    std::vector<std::optional<int>> previousResults;
};

/// A follower with the test's clock and change source, and what it delivered.
struct Rig {
    Rig()
    {
        auto manualClock = std::make_unique<ManualClock>();
        auto fakeChanges = std::make_unique<FakeChangeSource>();
        clock = manualClock.get();
        changes = fakeChanges.get();
        follower = std::make_unique<Follower>(
            &context,
            [ this ]( const int& result ) {
                delivered.push_back( result );
                if ( onDelivered ) {
                    onDelivered( result );
                }
            },
            FollowerParts{ std::move( manualClock ), std::move( fakeChanges ) } );
    }

    /// Wait until the follower has nothing running, scheduled or flagged.
    void settle()
    {
        REQUIRE( processUntil( [ this ] { return !follower->status().busy; } ) );
    }

    /// Wait until the follower has finished @p runs runs.
    void waitFinished( int runs )
    {
        REQUIRE( processUntil( [ this, runs ] { return follower->status().finished == runs; } ) );
    }

    int started() const
    {
        return follower->status().started;
    }

    QObject context;
    std::vector<int> delivered;
    /// Called by the sink after recording a result, e.g. to follow anew.
    std::function<void( int )> onDelivered;
    ManualClock* clock = nullptr;
    FakeChangeSource* changes = nullptr;
    std::unique_ptr<Follower> follower;
};

constexpr int kGap = FollowerParts::kGapMs;

} // namespace

SCENARIO( "FileFollower reads the file at once when its inputs change", "[filefollower]" )
{
    GIVEN( "a follower" )
    {
        Jobs jobs;
        Rig rig;

        WHEN( "it is told to follow a file with a job" )
        {
            rig.follower->follow( "/logs/a.log", jobs.job() );

            THEN( "the file is handed to the change source and watched as the job starts" )
            {
                REQUIRE( rig.changes->file == "/logs/a.log" );
                REQUIRE( rig.changes->rearms == 1 );
                REQUIRE( rig.follower->status().busy );
            }
            THEN( "the result is delivered later, never from within follow()" )
            {
                REQUIRE( rig.delivered.empty() );
                rig.settle();
                REQUIRE( rig.delivered == std::vector<int>{ 1 } );
                REQUIRE( jobs.files == QStringList{ "/logs/a.log" } );
                REQUIRE( jobs.previous() == std::vector<std::optional<int>>{ std::nullopt } );
                REQUIRE( rig.started() == 1 );
                REQUIRE_FALSE( rig.clock->wakeTime() );
            }
        }
    }

    THEN( "the gap between runs is half a second" )
    {
        REQUIRE( kGap == 500 );
    }
}

SCENARIO( "FileFollower runs once more after a change during a run, a gap after it",
          "[filefollower]" )
{
    GIVEN( "a run that holds the worker" )
    {
        Jobs jobs;
        Rig rig;
        jobs.hold = Jobs::Hold::UntilReleased;
        rig.follower->follow( "/logs/a.log", jobs.job() );
        REQUIRE( jobs.started.tryAcquire( 1, 10000 ) );

        WHEN( "the file changes, twice, while it runs, and it finishes at 100 ms" )
        {
            rig.changes->fire();
            rig.changes->fire();
            rig.clock->setNow( 100 );
            jobs.release.release( 2 );
            REQUIRE( processUntil( [ &rig ] { return rig.delivered.size() == 1; } ) );

            THEN( "the run was not cancelled, and was delivered" )
            {
                REQUIRE_FALSE( jobs.sawCancel );
                REQUIRE( rig.delivered == std::vector<int>{ 1 } );
            }
            THEN( "one more run waits for a full gap after it finished" )
            {
                REQUIRE( rig.clock->wakeTime() == 100 + kGap );
                REQUIRE( rig.follower->status().busy );
                rig.clock->advance( kGap - 1 );
                REQUIRE( rig.started() == 1 );

                rig.clock->advance( 1 );
                REQUIRE( rig.started() == 2 );
                rig.settle();
                REQUIRE( rig.delivered == std::vector<int>{ 1, 2 } );
                REQUIRE( rig.started() == 2 );
            }
            THEN( "the rerun gets the delivered result as the previous one, and watches again" )
            {
                rig.clock->advance( kGap );
                rig.settle();
                REQUIRE( jobs.previous() == std::vector<std::optional<int>>{ std::nullopt, 1 } );
                REQUIRE( rig.changes->rearms == 2 );
            }
        }
    }
}

SCENARIO( "FileFollower waits a gap after a change, which a busy log does not postpone",
          "[filefollower]" )
{
    GIVEN( "a file read at 0 ms" )
    {
        Jobs jobs;
        Rig rig;
        rig.follower->follow( "/logs/a.log", jobs.job() );
        rig.settle();
        rig.clock->setNow( 1000 );

        WHEN( "it changes at 1000 ms, and again and again during the gap" )
        {
            rig.changes->fire();
            rig.clock->setNow( 1200 );
            rig.changes->fire();
            rig.clock->setNow( 1000 + kGap - 1 );
            rig.changes->fire();

            THEN( "it is read again a gap after the first change, once" )
            {
                REQUIRE( rig.started() == 1 );
                REQUIRE( rig.clock->wakeTime() == 1000 + kGap );
                rig.clock->advance( 1 );
                REQUIRE( rig.started() == 2 );
                rig.settle();
                REQUIRE( rig.delivered == std::vector<int>{ 1, 2 } );
                REQUIRE_FALSE( rig.clock->wakeTime() );
            }
        }
    }
}

SCENARIO( "FileFollower cancels and restarts when its inputs change", "[filefollower]" )
{
    GIVEN( "a follower" )
    {
        Jobs jobs;
        Jobs newJobs( 100 );
        // After the jobs, so that the follower stops before they go.
        Rig rig;

        WHEN( "the inputs change while a run is going" )
        {
            jobs.hold = Jobs::Hold::UntilCancelled;
            rig.follower->follow( "/logs/a.log", jobs.job() );
            REQUIRE( jobs.started.tryAcquire( 1, 10000 ) );
            rig.changes->fire();
            rig.follower->follow( "/logs/b.log", newJobs.job() );

            THEN( "the run is cancelled and dropped, and the new one runs at once" )
            {
                REQUIRE( rig.changes->file == "/logs/b.log" );
                rig.settle();
                REQUIRE( jobs.sawCancel );
                REQUIRE( rig.delivered == std::vector<int>{ 100 } );
                REQUIRE( newJobs.files == QStringList{ "/logs/b.log" } );
                REQUIRE( rig.started() == 2 );
                REQUIRE_FALSE( rig.clock->wakeTime() );
            }
        }

        WHEN( "the inputs change while a change waits for its gap" )
        {
            rig.follower->follow( "/logs/a.log", jobs.job() );
            rig.settle();
            rig.changes->fire();
            REQUIRE( rig.clock->wakeTime() );
            rig.follower->follow( "/logs/a.log", newJobs.job() );

            THEN( "the wait is dropped, and the new job starts without a gap" )
            {
                REQUIRE_FALSE( rig.clock->wakeTime() );
                REQUIRE( rig.started() == 2 );
                rig.settle();
                REQUIRE( rig.delivered == std::vector<int>{ 1, 100 } );
            }
            THEN( "the new job does not get the earlier result as the previous one" )
            {
                rig.settle();
                REQUIRE( newJobs.previous() == std::vector<std::optional<int>>{ std::nullopt } );
            }
        }

        WHEN( "it is told to follow nothing" )
        {
            rig.follower->follow( "/logs/a.log", jobs.job() );
            rig.settle();
            rig.follower->follow( QString(), nullptr );
            rig.changes->fire();

            THEN( "nothing is watched or run any more" )
            {
                REQUIRE( rig.changes->file.isEmpty() );
                REQUIRE_FALSE( rig.clock->wakeTime() );
                REQUIRE_FALSE( rig.follower->status().busy );
                REQUIRE( rig.started() == 1 );
            }
        }
    }
}

SCENARIO( "FileFollower skips a change that cannot matter, and watches the file again",
          "[filefollower]" )
{
    GIVEN( "a file read with a predicate saying whether a change can matter" )
    {
        Jobs jobs;
        Rig rig;
        bool matters = false;
        QString askedFile;
        std::optional<int> askedLast;
        int rearmsWhenAsked = -1;
        rig.follower->follow( "/logs/a.log", jobs.job(),
                              [ & ]( const QString& file, const int& last ) {
                                  askedFile = file;
                                  askedLast = last;
                                  rearmsWhenAsked = rig.changes->rearms;
                                  return matters;
                              } );
        rig.settle();
        REQUIRE( rig.changes->rearms == 1 );

        WHEN( "a change that cannot matter is due" )
        {
            rig.changes->fire();
            rig.clock->advance( kGap );

            THEN( "it is asked about the file and its last result, and nothing runs" )
            {
                REQUIRE( askedFile == "/logs/a.log" );
                REQUIRE( askedLast == 1 );
                REQUIRE( rig.started() == 1 );
                REQUIRE_FALSE( rig.follower->status().busy );
            }
            THEN( "the file was watched again before the predicate read it" )
            {
                // A write during the predicate's read is then reported.
                REQUIRE( rearmsWhenAsked == 2 );
                REQUIRE( rig.changes->rearms == 2 );
            }

            AND_WHEN( "a change that matters follows" )
            {
                matters = true;
                rig.changes->fire();
                rig.clock->advance( kGap );

                THEN( "it is read, watched from before the predicate, not again after it" )
                {
                    REQUIRE( rearmsWhenAsked == 3 );
                    REQUIRE( rig.changes->rearms == 3 );
                    REQUIRE( rig.started() == 2 );
                    rig.settle();
                    REQUIRE( rig.delivered == std::vector<int>{ 1, 2 } );
                }
            }
        }
    }
}

SCENARIO( "FileFollower survives a job that throws", "[filefollower]" )
{
    GIVEN( "a run that throws, while the file changes" )
    {
        Jobs jobs;
        Rig rig;
        jobs.hold = Jobs::Hold::UntilReleased;
        jobs.throwOnce = true;
        rig.follower->follow( "/logs/a.log", jobs.job() );
        REQUIRE( jobs.started.tryAcquire( 1, 10000 ) );
        rig.changes->fire();
        jobs.release.release( 2 );
        rig.waitFinished( 1 );

        THEN( "nothing is delivered, and the change is still read after the gap" )
        {
            REQUIRE( rig.delivered.empty() );
            REQUIRE( rig.clock->wakeTime() == kGap );
            rig.clock->advance( kGap );
            rig.settle();
            REQUIRE( rig.delivered == std::vector<int>{ 1 } );
        }
        THEN( "the failed run never counts as the previous result" )
        {
            rig.clock->advance( kGap );
            rig.settle();
            REQUIRE( jobs.previous()
                     == std::vector<std::optional<int>>{ std::nullopt, std::nullopt } );
        }
    }
}

SCENARIO( "FileFollower delivers nothing once stopped", "[filefollower]" )
{
    GIVEN( "a run that goes on until it is cancelled, and a change during it" )
    {
        Jobs jobs;
        Rig rig;
        jobs.hold = Jobs::Hold::UntilCancelled;
        rig.follower->follow( "/logs/a.log", jobs.job() );
        REQUIRE( jobs.started.tryAcquire( 1, 10000 ) );
        rig.changes->fire();

        WHEN( "the follower is stopped" )
        {
            rig.follower->stop();

            THEN( "the run was cancelled and waited for, and nothing is delivered or pending" )
            {
                REQUIRE( jobs.sawCancel );
                // The dropped run counts as finished (#39).
                REQUIRE( rig.follower->status().finished == 1 );
                QCoreApplication::processEvents();
                REQUIRE( rig.delivered.empty() );
                REQUIRE_FALSE( rig.follower->status().busy );
                REQUIRE_FALSE( rig.clock->wakeTime() );
                REQUIRE( rig.changes->file.isEmpty() );
            }
            THEN( "later changes start nothing" )
            {
                rig.changes->fire();
                rig.clock->advance( 10 * kGap );
                QCoreApplication::processEvents();
                REQUIRE( rig.started() == 1 );
                REQUIRE( rig.delivered.empty() );
            }
        }

        WHEN( "the follower is destroyed" )
        {
            rig.follower.reset();

            THEN( "the run was cancelled and waited for, and nothing is delivered" )
            {
                REQUIRE( jobs.sawCancel );
                QCoreApplication::processEvents();
                REQUIRE( rig.delivered.empty() );
            }
        }
    }
}

SCENARIO( "FileFollower drops a result already on its way when it is told otherwise",
          "[filefollower]" )
{
    GIVEN( "a run whose result is on its way to the context's thread" )
    {
        Jobs jobs;
        Jobs newJobs( 100 );
        Rig rig;
        rig.follower->follow( "/logs/a.log", jobs.job() );
        REQUIRE( jobs.waitReturned( 1 ) );

        WHEN( "the inputs change before it arrives" )
        {
            rig.follower->follow( "/logs/a.log", newJobs.job() );
            rig.settle();

            THEN( "it is not delivered, and never becomes the previous result" )
            {
                REQUIRE( rig.delivered == std::vector<int>{ 100 } );
                rig.changes->fire();
                rig.clock->advance( kGap );
                rig.settle();
                REQUIRE( newJobs.previous()
                         == std::vector<std::optional<int>>{ std::nullopt, 100 } );
                REQUIRE( rig.delivered == std::vector<int>{ 100, 101 } );
            }
        }

        WHEN( "the follower is stopped before it arrives" )
        {
            rig.follower->stop();
            QCoreApplication::processEvents();

            THEN( "it is not delivered" )
            {
                REQUIRE( rig.delivered.empty() );
            }
        }
    }
}

SCENARIO( "FileFollower lets its sink follow anew while a rerun is flagged", "[filefollower]" )
{
    GIVEN( "a run during which the file changed, whose result makes the sink follow anew" )
    {
        Jobs jobs;
        Jobs newJobs( 100 );
        Rig rig;
        rig.onDelivered = [ & ]( int result ) {
            if ( result == 1 ) {
                rig.follower->follow( "/logs/b.log", newJobs.job() );
            }
        };
        jobs.hold = Jobs::Hold::UntilReleased;
        rig.follower->follow( "/logs/a.log", jobs.job() );
        REQUIRE( jobs.started.tryAcquire( 1, 10000 ) );
        rig.changes->fire();

        WHEN( "the run finishes" )
        {
            jobs.release.release();
            rig.settle();

            THEN( "the new job ran without the old rerun or the old result" )
            {
                REQUIRE( rig.delivered == std::vector<int>{ 1, 100 } );
                REQUIRE( newJobs.previous() == std::vector<std::optional<int>>{ std::nullopt } );
                REQUIRE( rig.changes->file == "/logs/b.log" );
                REQUIRE_FALSE( rig.clock->wakeTime() );
                rig.clock->advance( 10 * kGap );
                REQUIRE( rig.started() == 2 );
            }
        }
    }
}

SCENARIO( "FileFollower follows again after it was stopped", "[filefollower]" )
{
    GIVEN( "a follower that read a file and was stopped" )
    {
        Jobs jobs;
        Rig rig;
        rig.follower->follow( "/logs/a.log", jobs.job() );
        rig.settle();
        rig.follower->stop();

        WHEN( "it is told to follow the file again" )
        {
            rig.follower->follow( "/logs/a.log", jobs.job() );
            rig.settle();

            THEN( "it reads and follows it afresh" )
            {
                REQUIRE( rig.delivered == std::vector<int>{ 1, 2 } );
                REQUIRE( jobs.previous()
                         == std::vector<std::optional<int>>{ std::nullopt, std::nullopt } );
                REQUIRE( rig.changes->file == "/logs/a.log" );
                rig.changes->fire();
                rig.clock->advance( kGap );
                rig.settle();
                REQUIRE( rig.delivered == std::vector<int>{ 1, 2, 3 } );
            }
        }
    }
}

SCENARIO( "FileFollower says whether the file is missing", "[filefollower]" )
{
    Rig rig;
    REQUIRE_FALSE( rig.follower->status().waitingForFile );
    rig.changes->waiting = true;
    REQUIRE( rig.follower->status().waitingForFile );
}
