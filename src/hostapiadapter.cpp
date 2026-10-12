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

#include "hostapiadapter.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>

#include <map>
#include <utility>

namespace custom_footer {

namespace {

/// A Regex Lab opened through an adapter, until its answer comes.
struct LabRequest {
    const HostApiAdapter* owner;
    /// The answer is dropped once this is gone.
    QPointer<const QObject> context;
    RegexLabDone done;
};

/// The Regex Labs not answered yet, by the number the host got as
/// user_data. LogSquirl's contract already rules out an answer after the
/// plugin is shut down (it drops them with the plugin's answer context,
/// ADR 0017); this table is extra protection, against a second or late
/// answer all the same. Never destroyed: an answer during static
/// destruction finds it still there. Touched on the UI thread only.
std::map<quintptr, LabRequest>& labRequests()
{
    static auto* requests = new std::map<quintptr, LabRequest>;
    return *requests;
}

/// The number for the next Regex Lab; never used twice, so a late answer
/// cannot reach a newer Lab.
quintptr nextLabNumber()
{
    static quintptr last = 0;
    return ++last;
}

/// Called by the host, once, on the UI thread, with a Regex Lab's answer.
/// Nothing may throw out of it: the host is C.
void onRegexLabAnswer( void* userData, int result, const char* pattern, int flags ) noexcept
{
    // Before the answer is taken out, there is no log to report a failure to.
    static const HostFunctions nobody;
    guarded( nobody, "the Regex Lab's answer", [ & ] {
        auto& requests = labRequests();
        const auto found = requests.find( reinterpret_cast<quintptr>( userData ) );
        if ( found == requests.end() ) {
            // Answered before, or its adapter is gone: the plugin was shut down.
            return;
        }
        // Taken out first: done may open another Lab, or shut the plugin
        // down, which destroys the adapter; so its log is copied too.
        auto request = std::move( found->second );
        requests.erase( found );
        HostFunctions host;
        host.log = request.owner->functions().log;

        guarded( host, "the Regex Lab's answer", [ & ] {
            if ( !request.context ) {
                return;
            }
            std::optional<RegexLabPattern> applied;
            if ( result == LOGSQUIRL_REGEX_LAB_APPLIED && pattern ) {
                applied = RegexLabPattern{ QString::fromUtf8( pattern ),
                                           ( flags & LOGSQUIRL_REGEX_LAB_MATCH_CASE ) != 0 };
            }
            request.done( applied );
        } );
    } );
}

} // namespace

HostApiAdapter::HostApiAdapter( const LogSquirlHostApi* api, void* handle, std::size_t apiSize )
    : api_( api )
    , handle_( handle )
{
    if ( api->log_message ) {
        functions_.log = [ api, handle ]( int level, const QString& message ) {
            api->log_message( handle, level, message.toUtf8().constData() );
        };
    }
    if ( LOGSQUIRL_HOST_API_HAS( apiSize, open_regex_lab ) && api->open_regex_lab ) {
        functions_.openRegexLab = [ this ]( const RegexLabPattern& pattern, const QObject* context,
                                            RegexLabDone done ) {
            return openRegexLab( pattern, context, std::move( done ) );
        };
    }
    if ( LOGSQUIRL_HOST_API_HAS( apiSize, go_to_log_line ) && api->go_to_log_line ) {
        functions_.goToLogLine = [ api, handle ]( std::uint64_t lineNumber ) {
            return api->go_to_log_line( handle, lineNumber ) >= LOGSQUIRL_LOG_LINES_OK;
        };
    }
    if ( LOGSQUIRL_HOST_API_HAS( apiSize, get_selected_log_lines )
         && api->get_selected_log_lines ) {
        functions_.selectedLogLines = [ api, handle ]() -> std::optional<SelectedLogLines> {
            const char* text = nullptr;
            size_t length = 0;
            const int result = api->get_selected_log_lines( handle, &text, &length, nullptr );
            if ( result < LOGSQUIRL_LOG_LINES_OK || !text ) {
                return std::nullopt;
            }
            // Copied at once: the host's buffer lasts only until the next call.
            const auto copy = QString::fromUtf8( text, static_cast<qsizetype>( length ) );
            return SelectedLogLines{ copy.split( QLatin1Char( '\n' ) ),
                                     result == LOGSQUIRL_LOG_LINES_TRUNCATED };
        };
    }
}

HostApiAdapter::~HostApiAdapter()
{
    auto& requests = labRequests();
    for ( auto request = requests.begin(); request != requests.end(); ) {
        request = request->second.owner == this ? requests.erase( request ) : std::next( request );
    }
}

bool HostApiAdapter::openRegexLab( const RegexLabPattern& pattern, const QObject* context,
                                   RegexLabDone done )
{
    if ( !context || !done ) {
        return false;
    }
    const auto number = nextLabNumber();
    auto& requests = labRequests();
    requests.emplace( number, LabRequest{ this, context, std::move( done ) } );

    int result = -1;
    try {
        result = api_->open_regex_lab( handle_, pattern.pattern.toUtf8().constData(),
                                       pattern.matchesCase ? LOGSQUIRL_REGEX_LAB_MATCH_CASE : 0,
                                       &onRegexLabAnswer, reinterpret_cast<void*>( number ) );
    } catch ( ... ) {
        requests.erase( number );
        throw;
    }
    if ( result != 0 ) {
        // Not opened: no answer will come.
        requests.erase( number );
        return false;
    }
    return true;
}

} // namespace custom_footer
