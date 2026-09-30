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

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace custom_footer {

RulePreviewer::RulePreviewer( QObject* parent )
    : QObject( parent )
    , previewFunction_( &FooterScanner::preview )
{
    setObjectName( "rulePreviewer" );
    pool_.setMaxThreadCount( 1 );

    delay_.setSingleShot( true );
    delay_.setInterval( kDelayMs );
    connect( &delay_, &QTimer::timeout, this, &RulePreviewer::flush );
}

RulePreviewer::~RulePreviewer()
{
    stop();
}

void RulePreviewer::setActiveFile( const QString& filePath )
{
    activeFile_ = filePath;
}

void RulePreviewer::setMaxLines( int maxLines )
{
    maxLines_ = maxLines;
}

void RulePreviewer::schedule( const QList<FooterEntry>& entries, int rule )
{
    dropRunning();
    pending_ = Request{ entries, rule };
    delay_.start();
    Q_EMIT updating();
}

void RulePreviewer::flush()
{
    delay_.stop();
    if ( !pending_ ) {
        return;
    }
    const auto request = std::move( *pending_ );
    pending_.reset();

    if ( activeFile_.isEmpty() ) {
        // Nothing to read: only the patterns are checked.
        Q_EMIT previewed(
            FooterScanner::preview( QString(), request.entries, request.rule, maxLines_ ) );
        return;
    }

    const auto generation = generation_;
    auto cancelled = std::make_shared<std::atomic_bool>( false );
    cancelRunning_ = cancelled;
    running_ = true;
    ++previewsStarted_;

    // The watcher lives on this thread, so its finished() is delivered here.
    auto* watcher = new QFutureWatcher<Preview>( this );
    connect( watcher, &QFutureWatcher<Preview>::finished, this, [ this, watcher, generation ] {
        watcher->deleteLater();
        if ( generation != generation_ ) {
            return;
        }
        running_ = false;
        cancelRunning_.reset();
        const auto preview = watcher->result();
        if ( preview.status != Preview::Status::Cancelled ) {
            Q_EMIT previewed( preview );
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

void RulePreviewer::cancel()
{
    delay_.stop();
    pending_.reset();
    dropRunning();
}

void RulePreviewer::stop()
{
    cancel();
    pool_.waitForDone();
}

void RulePreviewer::setPreviewFunction( PreviewFunction function )
{
    previewFunction_ = std::move( function );
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
