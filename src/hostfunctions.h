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

#pragma once

#include "logsquirl_plugin_api.h"

#include <QString>

#include <exception>
#include <functional>

namespace custom_footer {

/**
 * What the plugin needs from its host, as a value that is copied freely.
 *
 * The C entry points fill it from the host's function table; tests fill it
 * with lambdas. A member the running host does not offer stays empty, and
 * the plugin then goes without it, so an older host is just the default
 * value.
 */
struct HostFunctions {
    /// Log a message at a LOGSQUIRL_LOG_* level. May be empty: then nothing is logged.
    std::function<void( int level, const QString& message )> log;

    /// Log through log(), if set. Never throws.
    void logMessage( int level, const QString& message ) const noexcept
    {
        try {
            if ( log ) {
                log( level, message );
            }
        } catch ( ... ) {
            // Logging is the last resort; there is nobody left to tell.
        }
    }
};

/**
 * Run @p work, logging through @p host instead of throwing. Returns false if
 * it threw. No exception may leave a C entry point, a host callback or a
 * handler the plugin connects to a widget's signal.
 */
template <typename Work>
bool guarded( const HostFunctions& host, const char* what, Work&& work ) noexcept
{
    const auto logFailure = [ &host, what ]( const char* reason ) noexcept {
        try {
            host.logMessage( LOGSQUIRL_LOG_ERROR,
                             QStringLiteral( "Custom Footer: %1 failed: %2" )
                                 .arg( QString::fromUtf8( what ), QString::fromUtf8( reason ) ) );
        } catch ( ... ) {
            host.logMessage( LOGSQUIRL_LOG_ERROR,
                             QStringLiteral( "Custom Footer: an operation failed" ) );
        }
    };
    try {
        work();
        return true;
    } catch ( const std::exception& e ) {
        logFailure( e.what() );
    } catch ( ... ) {
        logFailure( "unknown exception" );
    }
    return false;
}

} // namespace custom_footer
