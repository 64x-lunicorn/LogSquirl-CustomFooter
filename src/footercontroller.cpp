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

#include "footercontroller.h"

#include "footerconfig.h"
#include "plugin.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

namespace custom_footer {

FooterController::FooterController( FooterDisplayWidget* widget, const QString& configDir,
                                    QObject* parent )
    : QObject( parent )
    , widget_( widget )
    , configDir_( configDir )
{
    pool_.setMaxThreadCount( 1 );

    // Collect the changes of a busy log, without postponing the scan for as
    // long as it keeps changing.
    rescanTimer_.setSingleShot( true );
    rescanTimer_.setInterval( kRescanDelayMs );
    connect( &rescanTimer_, &QTimer::timeout, this, &FooterController::rescan );
    connect( &fileWatcher_, &QFileSystemWatcher::fileChanged, this, [ this ] {
        // The file may have been renamed away, and some watchers would keep
        // following it: the rescan watches whatever is at the path then.
        fileWatcher_.removePath( activeFile_ );
        if ( !rescanTimer_.isActive() ) {
            rescanTimer_.start();
        }
    } );
    // Only watched while the active file is missing: wait for it to be recreated.
    connect( &fileWatcher_, &QFileSystemWatcher::directoryChanged, this, [ this ] {
        if ( QFileInfo::exists( activeFile_ ) && !rescanTimer_.isActive() ) {
            rescanTimer_.start();
        }
    } );

    loadConfig();
}

FooterController::~FooterController()
{
    if ( cancelRunning_ ) {
        cancelRunning_->store( true );
    }
    pool_.waitForDone();
}

void FooterController::setActiveFile( const QString& filePath )
{
    if ( filePath != activeFile_ ) {
        unwatch();
        activeFile_ = filePath;
        progress_ = {};

        // The previous file's values must not pass for this file's while it is scanned.
        show( {} );
    }
    rescan();
}

void FooterController::reloadConfig()
{
    loadConfig();
    progress_ = {};
    rescan();
}

void FooterController::loadConfig()
{
    scanner_ = std::make_shared<const FooterScanner>( FooterConfig::loadEntries( configDir_ ) );
    maxLines_ = FooterConfig::loadMaxLines( configDir_ );

    for ( const auto& problem : scanner_->problems() ) {
        hostLog( LOGSQUIRL_LOG_WARNING, QStringLiteral( "Custom Footer: %1" ).arg( problem ) );
    }
}

void FooterController::rescan()
{
    rescanTimer_.stop();

    // Whatever runs now is outdated.
    const auto generation = ++generation_;
    if ( cancelRunning_ ) {
        cancelRunning_->store( true );
        cancelRunning_.reset();
    }

    if ( activeFile_.isEmpty() ) {
        show( {} );
        return;
    }
    watchActiveFile();

    auto cancelled = std::make_shared<std::atomic_bool>( false );
    cancelRunning_ = cancelled;

    // The watcher lives on this thread, so its finished() is delivered here.
    using Scan = FooterScanner::Scan;
    auto* watcher = new QFutureWatcher<Scan>( this );
    connect( watcher, &QFutureWatcher<Scan>::finished, this, [ this, watcher, generation ] {
        watcher->deleteLater();
        if ( generation == generation_ ) {
            const auto scan = watcher->result();
            progress_ = scan.progress;
            show( scanner_->inRuleOrder( scan.values ) );
        }
    } );
    watcher->setFuture( QtConcurrent::run(
        &pool_,
        [ scanner = scanner_, filePath = activeFile_, progress = progress_, maxLines = maxLines_,
          cancelled ]() -> Scan {
            try {
                return scanner->scanFrom( filePath, progress, maxLines, cancelled.get() );
            } catch ( ... ) {
                // E.g. out of memory: better no values than a dead host.
                return {};
            }
        } ) );
}

void FooterController::watchActiveFile()
{
    if ( !QFileInfo::exists( activeFile_ ) ) {
        // Rotated away or not created yet: wait in its directory for it.
        fileWatcher_.removePath( activeFile_ );
        const auto dir = QFileInfo( activeFile_ ).absolutePath();
        if ( watchedDir_ != dir && QFileInfo::exists( dir ) ) {
            unwatch();
            if ( fileWatcher_.addPath( dir ) ) {
                watchedDir_ = dir;
            }
        }
        return;
    }

    if ( !watchedDir_.isEmpty() ) {
        fileWatcher_.removePath( watchedDir_ );
        watchedDir_.clear();
    }
    if ( !fileWatcher_.files().contains( activeFile_ ) ) {
        fileWatcher_.addPath( activeFile_ );
    }
}

void FooterController::unwatch()
{
    const auto paths = fileWatcher_.files() + fileWatcher_.directories();
    if ( !paths.isEmpty() ) {
        fileWatcher_.removePaths( paths );
    }
    watchedDir_.clear();
}

void FooterController::show( const Values& values )
{
    if ( widget_ ) {
        widget_->updateValues( values );
    }
}

} // namespace custom_footer
