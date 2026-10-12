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

#include "footersession.h"

#include "footercontroller.h"
#include "footerdisplaywidget.h"

namespace custom_footer {

FooterSession::FooterSession( const QString& configDir, FooterDisplayWidget* footer,
                              HostFunctions host )
    : configDir_( configDir )
    , host_( std::move( host ) )
{
    if ( configDir_.isEmpty() ) {
        host_.logMessage( LOGSQUIRL_LOG_WARNING,
                          QStringLiteral( "Custom Footer: the host gave no config directory; "
                                          "rules are neither loaded nor saved" ) );
    }
    controller_ = std::make_unique<FooterController>(
        footer, configDir_, [ host = host_ ]( const QString& warning ) {
            host.logMessage( LOGSQUIRL_LOG_WARNING,
                             QStringLiteral( "Custom Footer: %1" ).arg( warning ) );
        } );
}

FooterSession::~FooterSession()
{
    // Cancels a running scan and waits for it; results not yet shown are dropped.
    controller_.reset();
}

void FooterSession::setActiveFile( const QString& filePath )
{
    activeFile_ = filePath;
    controller_->setActiveFile( filePath );
}

} // namespace custom_footer
