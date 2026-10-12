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

#pragma once

#include "changesource.h"
#include "filechangesource.h"
#include "followerclock.h"
#include "latestjob.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace custom_footer {

/**
 * What a FileFollower is made of besides its owner's job: the clock and
 * the change source. A part left null is the production adapter
 * (QtFollowerClock, FileChangeSource); tests pass their own.
 */
struct FollowerParts {
    /// The one definition of the pause after a change of the file, and of
    /// the gap between two runs. The footer's own pause
    /// (FooterController::kRescanDelayMs) goes in favour of it with #35.
    static constexpr int kGapMs = 500;

    std::unique_ptr<FollowerClock> clock;
    std::unique_ptr<ChangeSource> changes;
    int gapMs = kGapMs;
};

/**
 * Follows the active file for an owner that needs the latest result of
 * reading it: the editor's preview, and later the footer.
 *
 * The owner provides the job, which reads the file on a worker thread, and
 * optionally a predicate saying whether a change can matter at all. It says
 * whether its inputs changed (follow()) or only the file did (nudge()), and
 * receives the latest result through its sink. Everything else is the
 * follower's:
 *
 * - follow() makes every earlier run outdated and sets its cancel flag,
 *   drops a pending rerun or wake-up, forgets the previous result, hands
 *   the file to the change source, and starts the job without a gap.
 *   follow() without a job is idle: nothing runs and nothing is followed.
 * - nudge(), or a change the change source reports, while a run is going
 *   flags one rerun and cancels nothing; while idle, it wakes up a gap from
 *   now; while a wake-up is pending, it changes nothing, so a busy log never
 *   postpones it. A flagged rerun wakes up a gap after its run finished: the
 *   file is never read twice back to back.
 * - When the wake-up comes, the change source is rearmed first. Then, if
 *   the predicate says the change cannot matter to the last result, nothing
 *   runs; otherwise the job runs. A change made while the predicate or the
 *   job reads the file is therefore always reported.
 * - The change source is rearmed once just before every run's job starts,
 *   never between the predicate and the job: the file is watched whenever
 *   it is read.
 * - The job gets a copy of the last result delivered since follow() as the
 *   previous one, or null. An outdated or failed run never becomes it.
 * - The sink only ever gets results of runs that are not outdated, and
 *   never from within follow(), nudge() or stop(). A job that throws counts
 *   as finished, without a result; the gap and a rerun still apply.
 * - Everything runs on the context's thread except the job, which runs on
 *   one worker thread (LatestJob).
 * - After stop(), or the destructor, no job code runs, the sink is not
 *   called, no wake-up is pending, and the change source follows nothing.
 *   Owners declare the follower after what its sink touches, or call stop()
 *   in their destructor. follow() starts afresh after stop().
 */
template <typename Result>
class FileFollower {
public:
    /// Reads @p file on the worker thread. @p previous is the last result
    /// delivered for the same inputs, or null; @p cancelled is set once the
    /// result is no longer wanted.
    using Job = std::function<Result( const QString& file, const Result* previous,
                                      const std::atomic_bool& cancelled )>;
    /// Whether a change of @p file can alter @p last, the last result.
    using Matters = std::function<bool( const QString& file, const Result& last )>;
    using Sink = std::function<void( const Result& )>;

    struct Status {
        /// A run is going, flagged or waiting for its wake-up.
        bool busy = false;
        /// The file is missing, and its appearance is waited for.
        bool waitingForFile = false;
        /// Runs started, and runs finished, delivered or dropped.
        int started = 0;
        int finished = 0;
    };

    FileFollower( QObject* context, Sink sink, FollowerParts parts = {} )
        : sink_( std::move( sink ) )
        , clock_( parts.clock ? std::move( parts.clock ) : std::make_unique<QtFollowerClock>() )
        , changes_( parts.changes ? std::move( parts.changes )
                                  : std::make_unique<FileChangeSource>() )
        , gapMs_( parts.gapMs )
        , runs_( context )
    {
        changes_->changed = [ this ] { nudge(); };
    }

    ~FileFollower()
    {
        stop();
        changes_->changed = nullptr;
    }

    FileFollower( const FileFollower& ) = delete;
    FileFollower& operator=( const FileFollower& ) = delete;

    /// The inputs changed: cancel, forget the previous result, and run
    /// @p job on @p file now. Without a job, follow nothing.
    void follow( const QString& file, Job job, Matters matters = {} )
    {
        runs_.drop();
        clock_->cancelWake();
        wakePending_ = false;
        rerun_ = false;
        previous_.reset();
        job_ = std::move( job );
        matters_ = std::move( matters );

        const auto followed = job_ ? file : QString();
        if ( followed != file_ ) {
            file_ = followed;
            changes_->setFile( file_ );
        }
        if ( job_ ) {
            // Reading the file now: watch it again, or its directory while it is missing.
            changes_->rearm();
            run();
        }
    }

    /// Only the file changed: read it again, after the running run and the gap.
    void nudge()
    {
        if ( !job_ ) {
            return;
        }
        if ( runs_.isRunning() ) {
            // Let it finish, or on a busy log no run would ever finish.
            rerun_ = true;
            return;
        }
        if ( !wakePending_ ) {
            wakeAfterGap();
        }
    }

    /// Cancel, wait for the worker, and deliver nothing more.
    void stop()
    {
        follow( QString(), nullptr );
        runs_.stop();
    }

    Status status() const
    {
        Status status;
        status.busy = runs_.isRunning() || rerun_ || wakePending_;
        status.waitingForFile = changes_->waitingForFile();
        status.started = runs_.started();
        status.finished = runs_.finished();
        return status;
    }

private:
    /// Run the job; the change source was rearmed just before.
    void run()
    {
        runs_.run(
            [ job = job_, file = file_,
              previous = previous_ ]( const std::atomic_bool* cancelled ) -> std::optional<Result> {
                try {
                    return job( file, previous.get(), *cancelled );
                } catch ( ... ) {
                    // E.g. out of memory: better no result than a dead host.
                    return std::nullopt;
                }
            },
            [ this ]( const std::optional<Result>& result ) { finished( result ); } );
    }

    void finished( const std::optional<Result>& result )
    {
        if ( result ) {
            previous_ = std::make_shared<const Result>( *result );
        }
        if ( std::exchange( rerun_, false ) ) {
            // The file changed meanwhile: again, but not back to back.
            wakeAfterGap();
        }
        // Last, as the sink may follow() something else.
        if ( result ) {
            sink_( *result );
        }
    }

    /// Wake up a gap from now. While a run goes, nothing wakes up (a rerun
    /// is flagged instead), so this is never sooner than a gap after the
    /// last run finished.
    void wakeAfterGap()
    {
        wakePending_ = true;
        clock_->wakeAt( clock_->now() + gapMs_, [ this ] { woke(); } );
    }

    void woke()
    {
        wakePending_ = false;
        if ( !job_ ) {
            return;
        }
        // Watch first: the predicate and the job read the file, and a
        // change made meanwhile must be reported, not taken as known.
        changes_->rearm();
        // Lines appended beyond what the result depends on, or a change
        // that left the file as it was, cannot alter it.
        if ( matters_ && previous_ && !matters_( file_, *previous_ ) ) {
            return;
        }
        run();
    }

    Sink sink_;
    std::unique_ptr<FollowerClock> clock_;
    std::unique_ptr<ChangeSource> changes_;
    const int gapMs_;

    QString file_;
    Job job_;
    Matters matters_;
    /// The last result delivered since follow(), shared with the runs
    /// rather than copied for each.
    std::shared_ptr<const Result> previous_;
    /// The file changed during the running run: run again after it.
    bool rerun_ = false;
    bool wakePending_ = false;

    /// The runs, on one worker thread: only the latest one is delivered.
    LatestJob<std::optional<Result>> runs_;
};

} // namespace custom_footer
