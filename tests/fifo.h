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
 * @file fifo.h
 * @brief The tests' FIFO, an active file that is not a regular file, and a
 *        watchdog that keeps a reader of it from hanging the tests. POSIX
 *        only: kFifosAvailable says whether the rest is there.
 */

#pragma once

#include <QtGlobal>

#if defined( Q_OS_UNIX )

#include <QFile>
#include <QString>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace custom_footer {

constexpr bool kFifosAvailable = true;

/// Make a FIFO at @p path. @return whether it was made.
inline bool makeFifo( const QString& path )
{
    return ::mkfifo( QFile::encodeName( path ).constData(), 0600 ) == 0;
}

/**
 * Opening a FIFO to read blocks until a writer opens it, and reading it
 * until a writer writes or closes it. After @p boundMs, the watchdog opens
 * the FIFO's writer end without blocking and closes it again, over and over
 * until it is destroyed: whoever reads the FIFO gets to its end, and a test
 * of code that reads it fails (fired()) instead of hanging. Declare it before
 * what may read the FIFO, so that it outlives it.
 */
class FifoWatchdog {
public:
    explicit FifoWatchdog( const QString& path, int boundMs = 3000 )
        : path_( QFile::encodeName( path ) )
        , thread_( [ this, boundMs ] { watch( boundMs ); } )
    {
    }

    ~FifoWatchdog()
    {
        {
            std::lock_guard<std::mutex> lock( mutex_ );
            stopped_ = true;
        }
        wake_.notify_all();
        thread_.join();
    }

    FifoWatchdog( const FifoWatchdog& ) = delete;
    FifoWatchdog& operator=( const FifoWatchdog& ) = delete;

    /// The bound passed, and a reader of the FIFO, if any, was released.
    bool fired() const
    {
        return fired_;
    }

private:
    void watch( int boundMs )
    {
        std::unique_lock<std::mutex> lock( mutex_ );
        if ( wake_.wait_for( lock, std::chrono::milliseconds( boundMs ),
                             [ this ] { return stopped_; } ) ) {
            return;
        }
        fired_ = true;
        while ( !wake_.wait_for( lock, std::chrono::milliseconds( 1 ),
                                 [ this ] { return stopped_; } ) ) {
            // Fails without a reader; with one, ends its wait and its reading.
            const int fd = ::open( path_.constData(), O_WRONLY | O_NONBLOCK );
            if ( fd >= 0 ) {
                ::close( fd );
            }
        }
    }

    const QByteArray path_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopped_ = false;
    std::atomic_bool fired_{ false };
    std::thread thread_;
};

} // namespace custom_footer

#else

namespace custom_footer {

constexpr bool kFifosAvailable = false;

} // namespace custom_footer

#endif
