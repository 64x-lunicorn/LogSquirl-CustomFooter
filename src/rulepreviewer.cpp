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

#include <QElapsedTimer>
#include <QTimer>

#include <memory>

#include <utility>

namespace custom_footer {

RulePreviewer::RulePreviewer( QObject* parent )
    : QObject( parent )
    , previewFunction_( &FooterScanner::preview )
    , startTimer_( new QTimer( this ) )
    , fileWatcher_( new ActiveFileWatcher( kRescanDelayMs, this ) )
{
    setObjectName( "rulePreviewer" );

    startTimer_->setSingleShot( true );
    startTimer_->setInterval( kDelayMs );
    connect( startTimer_, &QTimer::timeout, this, &RulePreviewer::startScheduled );

    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    clock_ = [ elapsed ] { return elapsed->elapsed(); };
    connect( fileWatcher_, &ActiveFileWatcher::changed, this, &RulePreviewer::previewChangedFile );
}

RulePreviewer::~RulePreviewer()
{
    stop();
}

void RulePreviewer::setActiveFile( const QString& filePath )
{
    if ( filePath == fileWatcher_->file() ) {
        noteChange();
        return;
    }

    fileWatcher_->setFile( filePath );
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
    fileChangeDelayMs_ = fileChangeMs;
    fileWatcher_->setDelay( fileChangeMs );
}

void RulePreviewer::schedule( const QList<FooterEntry>& entries, int rule, Start start )
{
    previews_.drop();
    fileWatcher_->cancelPending();
    rerun_ = false;
    lastFinished_.reset();
    lastPreview_.reset();

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
    fileWatcher_->cancelPending();
    request_.reset();
    startSoon_ = false;
    rerun_ = false;
    lastFinished_.reset();
    lastPreview_.reset();
    previews_.drop();
}

void RulePreviewer::stop()
{
    clear();
    previews_.stop();
}

bool RulePreviewer::isBusy() const
{
    return startTimer_->isActive() || previews_.isRunning() || rerun_ || fileWatcher_->isPending();
}

void RulePreviewer::setPreviewFunction( PreviewFunction function )
{
    previewFunction_ = std::move( function );
}

void RulePreviewer::setClock( std::function<qint64()> clock )
{
    clock_ = std::move( clock );
}

void RulePreviewer::start()
{
    if ( !request_ ) {
        return;
    }
    const auto& request = *request_;
    const auto& filePath = fileWatcher_->file();

    if ( filePath.isEmpty() ) {
        // Nothing to read: only the patterns are checked.
        Q_EMIT previewed(
            FooterScanner::preview( QString(), request.entries, request.rule, maxLines_ ) );
        return;
    }
    // Reading the file now: watch it again, or its directory while it is missing.
    fileWatcher_->watch();

    previews_.run(
        [ function = previewFunction_, filePath, request,
          maxLines = maxLines_ ]( const std::atomic_bool* cancelled ) -> Preview {
            try {
                return function( filePath, request.entries, request.rule, maxLines, cancelled );
            } catch ( ... ) {
                // E.g. out of memory: better no preview than a dead host.
                Preview failed;
                failed.status = Preview::Status::Unreadable;
                failed.filePath = filePath;
                return failed;
            }
        },
        [ this ]( const Preview& preview ) {
            lastFinished_ = clock_();
            lastPreview_ = preview;
            if ( preview.status != Preview::Status::Cancelled ) {
                Q_EMIT previewed( preview );
            }
            if ( rerun_ ) {
                // The file changed meanwhile: again, but not back to back.
                rerun_ = false;
                fileWatcher_->schedule();
            }
        } );
}

bool RulePreviewer::followsFileChanges() const
{
    // Without a request or a file there is nothing to preview again, and a
    // scheduled preview reads the file anyway.
    return request_ && !fileWatcher_->file().isEmpty() && !startTimer_->isActive();
}

void RulePreviewer::noteChange()
{
    if ( !followsFileChanges() ) {
        return;
    }
    if ( previews_.isRunning() ) {
        // Let it finish, or on a busy log no preview would ever finish.
        rerun_ = true;
        return;
    }
    fileWatcher_->schedule();
}

void RulePreviewer::previewChangedFile()
{
    if ( !followsFileChanges() ) {
        return;
    }
    if ( previews_.isRunning() ) {
        rerun_ = true;
        return;
    }
    // Never back to back: a full pause after the last preview finished,
    // even if the change's pause ends just after it.
    if ( lastFinished_ ) {
        const auto since = clock_() - *lastFinished_;
        if ( since < fileChangeDelayMs_ ) {
            fileWatcher_->scheduleIn( static_cast<int>( fileChangeDelayMs_ - since ) );
            return;
        }
    }
    // Lines appended beyond the limits, or a change that left the file as
    // it was, cannot alter the preview.
    if ( lastPreview_ && FooterScanner::fileUnchangedFor( fileWatcher_->file(), *lastPreview_ ) ) {
        return;
    }
    start();
}

} // namespace custom_footer
