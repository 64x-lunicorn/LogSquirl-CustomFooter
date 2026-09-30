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

namespace custom_footer {

FooterController::FooterController( FooterDisplayWidget* widget, const QString& configDir,
                                    QObject* parent )
    : QObject( parent )
    , widget_( widget )
    , configDir_( configDir )
{
    // Collect the changes of a busy log, without postponing the scan for as
    // long as it keeps changing.
    connect( &fileWatcher_, &ActiveFileWatcher::changed, this, &FooterController::requestScan );

    loadConfig();
}

FooterController::~FooterController()
{
    scans_.stop();
}

void FooterController::setActiveFile( const QString& filePath )
{
    if ( filePath != activeFile_ ) {
        activeFile_ = filePath;
        fileWatcher_.setFile( filePath );
        progress_ = {};

        // The previous file's values must not pass for this file's while it is scanned.
        show( {} );
        restartScan();
        return;
    }
    requestScan();
}

void FooterController::reloadConfig()
{
    loadConfig();
    progress_ = {};
    restartScan();
}

void FooterController::loadConfig()
{
    scanner_ = std::make_shared<const FooterScanner>( FooterConfig::loadEntries( configDir_ ) );
    maxLines_ = FooterConfig::loadMaxLines( configDir_ );

    for ( const auto& problem : scanner_->problems() ) {
        hostLog( LOGSQUIRL_LOG_WARNING, QStringLiteral( "Custom Footer: %1" ).arg( problem ) );
    }
}

void FooterController::requestScan()
{
    if ( !scans_.isRunning() ) {
        startScan();
        return;
    }

    // Let the running scan of this file finish, or on a busy log no scan
    // would ever finish; then scan on from where it stopped.
    fileWatcher_.cancelPending();
    rescanPending_ = true;
    fileWatcher_.watch();
}

void FooterController::restartScan()
{
    // Whatever runs now is outdated; it is not shown.
    scans_.drop();
    rescanPending_ = false;

    startScan();
}

void FooterController::startScan()
{
    fileWatcher_.cancelPending();
    rescanPending_ = false;

    if ( activeFile_.isEmpty() ) {
        scans_.drop();
        show( {} );
        return;
    }
    fileWatcher_.watch();

    using Scan = FooterScanner::Scan;
    scans_.run(
        [ scanner = scanner_, filePath = activeFile_, progress = progress_,
          maxLines = maxLines_ ]( const std::atomic_bool* cancelled ) -> Scan {
            try {
                return scanner->scanFrom( filePath, progress, maxLines, cancelled );
            } catch ( ... ) {
                // E.g. out of memory: better no values than a dead host.
                return {};
            }
        },
        [ this ]( const Scan& scan ) {
            progress_ = scan.progress;
            show( scanner_->footerValues( scan.values ) );
            if ( rescanPending_ ) {
                startScan();
            }
        } );
}

void FooterController::show( const Values& values )
{
    if ( widget_ ) {
        widget_->updateValues( values );
    }
}

} // namespace custom_footer
