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
#include <QStringList>

#include <cstdint>
#include <exception>
#include <functional>
#include <optional>

class QObject;

namespace custom_footer {

/// A regular expression as the Regex Lab tests it: a Perl-compatible
/// pattern, as QRegularExpression reads it, and whether it matches case.
struct RegexLabPattern {
    QString pattern;
    bool matchesCase = true;
};

/// The answer of a Regex Lab: the pattern the user applied, or nothing when
/// the user cancelled the Lab or closed it.
using RegexLabDone = std::function<void( const std::optional<RegexLabPattern>& applied )>;

/// The Log Lines selected in the host's tab in front, in the order of the
/// Log File.
struct SelectedLogLines {
    QStringList lines;
    /// Fewer lines than are selected: the host hands over at most
    /// LOGSQUIRL_SELECTED_LOG_LINES_MAX_LINES lines and _MAX_BYTES bytes.
    bool truncated = false;
};

/// A host function the plugin uses where the running host offers it.
enum class HostFeature {
    RegexLab,         ///< HostFunctions::openRegexLab
    GoToLogLine,      ///< HostFunctions::goToLogLine
    SelectedLogLines, ///< HostFunctions::selectedLogLines
};

/**
 * What the plugin needs from its host, as a value that is copied freely.
 *
 * The C entry points fill it from the host's function table; tests fill it
 * with lambdas. A member the running host does not offer stays empty, and
 * the plugin then goes without it, so an older host is just the default
 * value.
 *
 * openRegexLab, goToLogLine and selectedLogLines must be called on the UI
 * thread only, as the host requires; menu actions, host callbacks and widget
 * signals all run there. Filled by the C entry points, they are valid until
 * the plugin is shut down.
 */
struct HostFunctions {
    /// Log a message at a LOGSQUIRL_LOG_* level. May be empty: then nothing is logged.
    std::function<void( int level, const QString& message )> log;

    /**
     * Open the host's Regex Lab with @p pattern, for the user to test,
     * change, and apply or cancel. Returns at once: true when the Lab opened,
     * false when it did not, and @p done is then never called.
     *
     * @p done is called at most once, later, on the UI thread: with the
     * applied pattern, or with nothing when the user cancelled the Lab or
     * closed it. It is never called once @p context is destroyed (the editor
     * that asked, for instance) or the plugin is shut down; it is released
     * once called, or at shutdown at the latest. @p context must not be null.
     *
     * Empty when the host does not offer it (before LogSquirl 26.11).
     */
    std::function<bool( const RegexLabPattern& pattern, const QObject* context, RegexLabDone done )>
        openRegexLab;

    /**
     * Select Log Line @p lineNumber in the host's tab in front and scroll it
     * into view, as Go to line does. Lines count from 1, as the host shows
     * them. Returns false when nothing was done: no Log File is open, or it
     * has no such line (yet).
     *
     * Empty when the host does not offer it (before LogSquirl 26.11).
     */
    std::function<bool( std::uint64_t lineNumber )> goToLogLine;

    /**
     * The Log Lines selected in the host's tab in front, copied at once;
     * nothing when no Log Line is selected or no Log File is open.
     *
     * Empty when the host does not offer it (before LogSquirl 26.11).
     */
    std::function<std::optional<SelectedLogLines>()> selectedLogLines;

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
