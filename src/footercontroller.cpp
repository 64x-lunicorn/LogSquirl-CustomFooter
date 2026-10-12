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

#include <utility>

namespace custom_footer {

FooterController::FooterController( FooterDisplayWidget* widget, const QString& configDir,
                                    Warn warn, QObject* parent, FollowerParts parts )
    : QObject( parent )
    , widget_( widget )
    , configDir_( configDir )
    , warn_( std::move( warn ) )
    , follower_(
          this,
          [ this ]( const FooterScanner::Scan& scan ) {
              // Scanned with the current rules: newer rules made it outdated.
              show( scanner_->footerValues( scan.values ) );
          },
          std::move( parts ) )
{
    loadConfig();
}

FooterController::~FooterController()
{
    follower_.stop();
}

void FooterController::setActiveFile( const QString& filePath )
{
    if ( filePath == activeFile_ ) {
        // A change of the file, as one on disk.
        follower_.nudge();
        return;
    }

    activeFile_ = filePath;
    // The previous file's values must not pass for this file's while it is scanned.
    show( {} );
    restartScan();
}

void FooterController::reloadConfig()
{
    loadConfig();
    restartScan();
}

void FooterController::loadConfig()
{
    scanner_ = std::make_shared<const FooterScanner>( FooterConfig::loadEntries( configDir_ ) );
    maxLines_ = FooterConfig::loadMaxLines( configDir_ );

    if ( warn_ ) {
        for ( const auto& problem : scanner_->problems() ) {
            warn_( problem );
        }
    }
}

void FooterController::restartScan()
{
    if ( activeFile_.isEmpty() ) {
        // Without a job the follower is idle: nothing runs or is followed.
        follower_.follow( QString(), nullptr );
        show( {} );
        return;
    }

    using Scan = FooterScanner::Scan;
    // Following anew forgets the previous scan: the file is scanned from its start.
    follower_.follow(
        activeFile_,
        [ scanner = scanner_, maxLines = maxLines_ ]( const QString& filePath, const Scan* previous,
                                                      const std::atomic_bool& cancelled ) {
            try {
                // On from where the previous scan stopped, if the file only grew.
                return scanner->scanFrom( filePath,
                                          previous ? previous->progress : FooterScanner::Progress{},
                                          maxLines, &cancelled );
            } catch ( ... ) {
                // E.g. out of memory: better no values than a dead host.
                return Scan{};
            }
        },
        // Lines appended once every key has a value or beyond the limits, or
        // a change that left the file as it was, cannot alter the values.
        []( const QString& filePath, const Scan& last ) {
            return !FooterScanner::fileUnchangedFor( filePath, last );
        } );
}

void FooterController::show( const Values& values )
{
    if ( widget_ ) {
        widget_->updateValues( values );
    }
}

} // namespace custom_footer
