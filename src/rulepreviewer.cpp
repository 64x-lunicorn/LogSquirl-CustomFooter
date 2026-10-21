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

#include <QTimer>

#include <utility>

namespace custom_footer {

RulePreviewer::RulePreviewer( QObject* parent, FollowerParts parts )
    : QObject( parent )
    , previewFunction_( &FooterScanner::preview )
    , startTimer_( new QTimer( this ) )
    , follower_(
          this,
          [ this ]( const Preview& preview ) {
              if ( preview.status != Preview::Status::Cancelled ) {
                  Q_EMIT previewed( preview );
              }
          },
          std::move( parts ) )
{
    setObjectName( "rulePreviewer" );

    startTimer_->setSingleShot( true );
    startTimer_->setInterval( kDelayMs );
    connect( startTimer_, &QTimer::timeout, this, &RulePreviewer::startScheduled );
}

RulePreviewer::~RulePreviewer()
{
    stop();
}

void RulePreviewer::setActiveFile( const QString& filePath, const QByteArray& hostEncoding )
{
    if ( filePath == activeFile_ && hostEncoding == activeFileEncoding_ ) {
        // A change of the file: followed only while its preview is.
        follower_.nudge();
        return;
    }

    activeFile_ = filePath;
    activeFileEncoding_ = hostEncoding;
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

void RulePreviewer::setEditDelay( int ms )
{
    editDelayMs_ = ms;
}

void RulePreviewer::schedule( const QList<FooterEntry>& entries, int rule, Start start )
{
    // The shown preview is outdated: without a job the follower is idle,
    // and nothing of it runs or is followed on until the request starts.
    follower_.follow( QString(), nullptr );

    request_ = Request{ entries, rule };
    // Coalesced with an earlier request that was to start soon, e.g. the
    // selection moving off a rule that is then removed.
    startSoon_ = startSoon_ || start == Start::Soon;
    startTimer_->start( startSoon_ ? 0 : editDelayMs_ );
    Q_EMIT updating();
}

void RulePreviewer::flush()
{
    if ( startTimer_->isActive() ) {
        startTimer_->stop();
        startScheduled();
    }
}

void RulePreviewer::startScheduled()
{
    startSoon_ = false;
    start();
}

void RulePreviewer::clear()
{
    startTimer_->stop();
    request_.reset();
    startSoon_ = false;
    // Without a job the follower is idle: nothing runs or is followed.
    follower_.follow( QString(), nullptr );
}

void RulePreviewer::stop()
{
    clear();
    follower_.stop();
}

bool RulePreviewer::isBusy() const
{
    return startTimer_->isActive() || follower_.status().busy;
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

    follower_.follow(
        activeFile_,
        [ function = previewFunction_, request, maxLines = maxLines_,
          hostEncoding = activeFileEncoding_ ]( const QString& filePath, const Preview*,
                                                const std::atomic_bool& cancelled ) {
            try {
                return function( filePath, request.entries, request.rule, maxLines, &cancelled,
                                 hostEncoding );
            } catch ( ... ) {
                // E.g. out of memory: better no preview than a dead host.
                Preview failed;
                failed.status = Preview::Status::Unreadable;
                failed.filePath = filePath;
                return failed;
            }
        },
        // Lines appended beyond the limits, or a change that left the file
        // as it was, cannot alter the preview.
        []( const QString& filePath, const Preview& last ) {
            return !FooterScanner::fileUnchangedFor( filePath, last );
        } );
}

} // namespace custom_footer
