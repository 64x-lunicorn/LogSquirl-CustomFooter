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

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <atomic>
#include <memory>
#include <utility>

namespace custom_footer {

/**
 * Runs jobs on one worker thread, of which only the latest counts: the scans
 * of the footer and the editor's previews.
 *
 * run() makes any earlier job outdated and asks it to stop; the result of an
 * outdated job is dropped, whatever order results arrive in. Results are
 * handed over on the thread of the context object, which owns the
 * LatestJob. stop() and the destructor cancel the running job, wait for
 * the worker, and drop results not yet handed over: afterwards no code of
 * the job or its handler runs.
 */
template <typename Result>
class LatestJob {
public:
    explicit LatestJob( QObject* context )
        : context_( context )
    {
        // One worker thread: a new job waits for the cancelled one to stop.
        pool_.setMaxThreadCount( 1 );
    }

    ~LatestJob()
    {
        stop();
    }

    LatestJob( const LatestJob& ) = delete;
    LatestJob& operator=( const LatestJob& ) = delete;

    /**
     * Run @p job( cancelled ) on the worker thread, and @p onResult( result )
     * on the context's thread once it finished, unless another job was run
     * or drop() was called meanwhile.
     */
    template <typename Job, typename OnResult>
    void run( Job job, OnResult onResult )
    {
        drop();
        const auto generation = generation_;
        auto cancelled = std::make_shared<std::atomic_bool>( false );
        cancel_ = cancelled;
        running_ = true;
        ++started_;

        auto* watcher = new QFutureWatcher<Result>( context_ );
        watchers_.append( watcher );
        QObject::connect( watcher, &QFutureWatcher<Result>::finished, context_,
                          [ this, watcher, generation, onResult = std::move( onResult ) ] {
                              watchers_.removeAll( watcher );
                              watcher->deleteLater();
                              ++finished_;
                              if ( generation != generation_ ) {
                                  return;
                              }
                              running_ = false;
                              cancel_.reset();
                              onResult( watcher->result() );
                          } );
        watcher->setFuture( QtConcurrent::run(
            &pool_, [ job = std::move( job ), cancelled ]() { return job( cancelled.get() ); } ) );
    }

    /// Make the running job outdated and ask it to stop, without waiting.
    void drop()
    {
        ++generation_;
        running_ = false;
        if ( cancel_ ) {
            cancel_->store( true );
            cancel_.reset();
        }
    }

    /// drop(), wait for the worker, and drop results not yet handed over.
    void stop()
    {
        drop();
        pool_.waitForDone();
        const auto watchers = std::exchange( watchers_, {} );
        for ( const auto& watcher : watchers ) {
            delete watcher.data();
        }
    }

    /// A job runs whose result is still wanted.
    bool isRunning() const
    {
        return running_;
    }

    /// How many jobs were started, and how many finished, handed over or
    /// dropped; for tests.
    int started() const
    {
        return started_;
    }
    int finished() const
    {
        return finished_;
    }

private:
    QObject* context_;
    QThreadPool pool_;
    quint64 generation_ = 0;
    bool running_ = false;
    std::shared_ptr<std::atomic_bool> cancel_;
    QList<QPointer<QFutureWatcher<Result>>> watchers_;
    int started_ = 0;
    int finished_ = 0;
};

} // namespace custom_footer
