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

#include "activefilewatcher.h"

#include <QFileInfo>

namespace custom_footer {

ActiveFileWatcher::ActiveFileWatcher( int delayMs, QObject* parent )
    : QObject( parent )
    , delayMs_( delayMs )
{
    timer_.setSingleShot( true );
    connect( &timer_, &QTimer::timeout, this, &ActiveFileWatcher::changed );
    connect( &watcher_, &QFileSystemWatcher::fileChanged, this, &ActiveFileWatcher::fileChanged );
    connect( &watcher_, &QFileSystemWatcher::directoryChanged, this,
             &ActiveFileWatcher::directoryChanged );
}

void ActiveFileWatcher::setFile( const QString& filePath )
{
    timer_.stop();
    unwatch();
    file_ = filePath;
    known_ = {};
}

void ActiveFileWatcher::watch()
{
    if ( file_.isEmpty() ) {
        return;
    }
    // The owner reads the file now: later notifications are compared to it.
    known_ = stampOf( file_ );
    if ( !systemNotifications_ ) {
        return;
    }
    if ( !QFileInfo::exists( file_ ) ) {
        // Rotated away or not created yet: wait in its directory for it.
        watcher_.removePath( file_ );
        const auto dir = QFileInfo( file_ ).absolutePath();
        if ( watchedDir_ != dir && QFileInfo::exists( dir ) ) {
            unwatch();
            if ( watcher_.addPath( dir ) ) {
                watchedDir_ = dir;
            }
        }
        return;
    }

    if ( !watchedDir_.isEmpty() ) {
        watcher_.removePath( watchedDir_ );
        watchedDir_.clear();
    }
    if ( !watcher_.files().contains( file_ ) ) {
        watcher_.addPath( file_ );
    }
}

void ActiveFileWatcher::schedule()
{
    if ( !timer_.isActive() ) {
        timer_.start( delayMs_ );
    }
}

void ActiveFileWatcher::scheduleIn( int delayMs )
{
    timer_.start( delayMs );
}

void ActiveFileWatcher::cancelPending()
{
    timer_.stop();
}

void ActiveFileWatcher::setSystemNotifications( bool enabled )
{
    systemNotifications_ = enabled;
    if ( !enabled ) {
        timer_.stop();
        unwatch();
    }
}

void ActiveFileWatcher::setDelay( int delayMs )
{
    delayMs_ = delayMs;
}

void ActiveFileWatcher::fileChanged( const QString& path )
{
    // A change of a file watched before, still queued.
    if ( path != file_ ) {
        return;
    }
    // Only read, e.g. by the owner's own scan: nothing to read again.
    const auto now = stampOf( file_ );
    if ( now == known_ ) {
        return;
    }
    known_ = now;
    // The file may have been renamed away, and some watchers would keep
    // following it: watch() watches whatever is at the path then.
    watcher_.removePath( file_ );
    schedule();
}

void ActiveFileWatcher::directoryChanged( const QString& path )
{
    if ( path == watchedDir_ && QFileInfo::exists( file_ ) ) {
        schedule();
    }
}

ActiveFileWatcher::Stamp ActiveFileWatcher::stampOf( const QString& filePath )
{
    const QFileInfo info( filePath );
    Stamp stamp;
    stamp.exists = info.exists();
    if ( stamp.exists ) {
        stamp.size = info.size();
        stamp.modified = info.lastModified();
        stamp.birth = info.birthTime();
    }
    return stamp;
}

void ActiveFileWatcher::unwatch()
{
    const auto paths = watchedPaths();
    if ( !paths.isEmpty() ) {
        watcher_.removePaths( paths );
    }
    watchedDir_.clear();
}

} // namespace custom_footer
