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
        if ( !rescanTimer_.isActive() ) {
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
        if ( !activeFile_.isEmpty() ) {
            fileWatcher_.removePath( activeFile_ );
        }
        activeFile_ = filePath;
    }
    rescan();
}

void FooterController::reloadConfig()
{
    loadConfig();
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
    auto* watcher = new QFutureWatcher<Values>( this );
    connect( watcher, &QFutureWatcher<Values>::finished, this, [ this, watcher, generation ] {
        watcher->deleteLater();
        if ( generation == generation_ ) {
            show( watcher->result() );
        }
    } );
    watcher->setFuture( QtConcurrent::run( &pool_,
                                           [ scanner = scanner_, filePath = activeFile_,
                                             maxLines = maxLines_, cancelled ]() -> Values {
                                               try {
                                                   return scanner->inRuleOrder( scanner->scanFile(
                                                       filePath, maxLines, cancelled.get() ) );
                                               } catch ( ... ) {
                                                   // E.g. out of memory: better no values than a
                                                   // dead host.
                                                   return {};
                                               }
                                           } ) );
}

void FooterController::watchActiveFile()
{
    // A log that was rotated or replaced drops out of the watcher.
    if ( !fileWatcher_.files().contains( activeFile_ ) && QFileInfo::exists( activeFile_ ) ) {
        fileWatcher_.addPath( activeFile_ );
    }
}

void FooterController::show( const Values& values )
{
    if ( widget_ ) {
        widget_->updateValues( values );
    }
}

} // namespace custom_footer
