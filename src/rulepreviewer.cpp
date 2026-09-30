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

#include "rulepreviewer.h"

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace custom_footer {

RulePreviewer::RulePreviewer( QObject* parent )
    : QObject( parent )
    , previewFunction_( &FooterScanner::preview )
    , startTimer_( new QTimer( this ) )
    , fileWatcher_( new QFileSystemWatcher( this ) )
    , rescanTimer_( new QTimer( this ) )
{
    setObjectName( "rulePreviewer" );
    pool_.setMaxThreadCount( 1 );

    startTimer_->setSingleShot( true );
    startTimer_->setInterval( kDelayMs );
    connect( startTimer_, &QTimer::timeout, this, &RulePreviewer::flush );

    rescanTimer_->setSingleShot( true );
    rescanTimer_->setInterval( kRescanDelayMs );
    connect( rescanTimer_, &QTimer::timeout, this, [ this ] {
        if ( running_ ) {
            rerun_ = true;
        }
        else {
            start();
        }
    } );
    connect( fileWatcher_, &QFileSystemWatcher::fileChanged, this, [ this ] {
        // The file may have been replaced, and some watchers would keep
        // following the old one: the next preview watches the path again.
        fileWatcher_->removePath( activeFile_ );
        fileChanged();
    } );
}

RulePreviewer::~RulePreviewer()
{
    stop();
}

void RulePreviewer::setActiveFile( const QString& filePath )
{
    if ( filePath == activeFile_ ) {
        fileChanged();
        return;
    }

    const auto watched = fileWatcher_->files();
    if ( !watched.isEmpty() ) {
        fileWatcher_->removePaths( watched );
    }
    activeFile_ = filePath;
    if ( request_ ) {
        auto request = *request_;
        schedule( request.entries, request.rule, Start::Soon );
    }
}

void RulePreviewer::setMaxLines( int maxLines )
{
    if ( maxLines == maxLines_ ) {
        return;
    }
    maxLines_ = maxLines;
    if ( request_ ) {
        auto request = *request_;
        schedule( request.entries, request.rule, Start::Soon );
    }
}

void RulePreviewer::setDelays( int editMs, int fileChangeMs )
{
    editDelayMs_ = editMs;
    rescanTimer_->setInterval( fileChangeMs );
}

void RulePreviewer::schedule( const QList<FooterEntry>& entries, int rule, Start start )
{
    dropRunning();
    rescanTimer_->stop();
    rerun_ = false;

    request_ = Request{ entries, rule };
    startPending_ = true;
    // Coalesced with an earlier request that was to start soon, e.g. the
    // selection moving off a rule that is then removed.
    startSoon_ = startSoon_ || start == Start::Soon;
    startTimer_->start( startSoon_ ? 0 : editDelayMs_ );
    Q_EMIT updating();
}

void RulePreviewer::flush()
{
    if ( !startPending_ ) {
        return;
    }
    startTimer_->stop();
    startPending_ = false;
    startSoon_ = false;
    start();
}

void RulePreviewer::clear()
{
    startTimer_->stop();
    rescanTimer_->stop();
    request_.reset();
    startPending_ = false;
    startSoon_ = false;
    rerun_ = false;
    dropRunning();
}

void RulePreviewer::stop()
{
    clear();
    pool_.waitForDone();
    // Their results are outdated: none may arrive later.
    const auto watchers = findChildren<QFutureWatcherBase*>( Qt::FindDirectChildrenOnly );
    qDeleteAll( watchers );
}

bool RulePreviewer::isBusy() const
{
    return startPending_ || running_ || rerun_ || rescanTimer_->isActive();
}

void RulePreviewer::setPreviewFunction( PreviewFunction function )
{
    previewFunction_ = std::move( function );
}

void RulePreviewer::start()
{
    if ( !request_ ) {
        return;
    }
    const auto& request = *request_;

    if ( activeFile_.isEmpty() ) {
        // Nothing to read: only the patterns are checked.
        Q_EMIT previewed(
            FooterScanner::preview( QString(), request.entries, request.rule, maxLines_ ) );
        return;
    }
    watchActiveFile();

    const auto generation = generation_;
    auto cancelled = std::make_shared<std::atomic_bool>( false );
    cancelRunning_ = cancelled;
    running_ = true;
    ++previewsStarted_;

    // The watcher lives on this thread, so its finished() is delivered here.
    auto* watcher = new QFutureWatcher<Preview>( this );
    connect( watcher, &QFutureWatcher<Preview>::finished, this, [ this, watcher, generation ] {
        watcher->deleteLater();
        ++previewsFinished_;
        if ( generation != generation_ ) {
            return;
        }
        running_ = false;
        cancelRunning_.reset();
        const auto preview = watcher->result();
        if ( preview.status != Preview::Status::Cancelled ) {
            Q_EMIT previewed( preview );
        }
        if ( rerun_ ) {
            // The file changed meanwhile.
            rerun_ = false;
            start();
        }
    } );
    watcher->setFuture(
        QtConcurrent::run( &pool_,
                           [ function = previewFunction_, filePath = activeFile_, request,
                             maxLines = maxLines_, cancelled ]() -> Preview {
                               try {
                                   return function( filePath, request.entries, request.rule,
                                                    maxLines, cancelled.get() );
                               } catch ( ... ) {
                                   // E.g. out of memory: better no preview than a dead host.
                                   Preview failed;
                                   failed.status = Preview::Status::Unreadable;
                                   failed.filePath = filePath;
                                   return failed;
                               }
                           } ) );
}

void RulePreviewer::fileChanged()
{
    if ( !request_ || activeFile_.isEmpty() || startPending_ ) {
        // Nothing to preview again, or a preview of the file starts anyway.
        return;
    }
    if ( running_ ) {
        // Let it finish, or on a busy log no preview would ever finish.
        rerun_ = true;
        return;
    }
    if ( !rescanTimer_->isActive() ) {
        rescanTimer_->start();
    }
}

void RulePreviewer::watchActiveFile()
{
    if ( QFileInfo::exists( activeFile_ ) && !fileWatcher_->files().contains( activeFile_ ) ) {
        fileWatcher_->addPath( activeFile_ );
    }
}

void RulePreviewer::dropRunning()
{
    ++generation_;
    running_ = false;
    if ( cancelRunning_ ) {
        cancelRunning_->store( true );
        cancelRunning_.reset();
    }
}

} // namespace custom_footer
