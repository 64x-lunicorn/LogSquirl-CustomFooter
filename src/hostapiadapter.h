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

#include "hostfunctions.h"
#include "logsquirl_plugin_api.h"

#include <cstddef>

namespace custom_footer {

/**
 * The HostFunctions of the host's C function table: the part of the C ABI
 * adapter that turns the table into C++ functions, and the only code that
 * calls the table's log, 26.11 and 26.12 functions but for the callback
 * registrations.
 *
 * It keeps the host's pointer and never copies the table. It reads only the
 * members that LOGSQUIRL_HOST_API_HAS( apiSize, … ) reports, so a host that
 * passes a shorter table (LogSquirl 26.10, through logsquirl_plugin_init())
 * gets empty openRegexLab, goToLogLine and selectedLogLines, and one before
 * LogSquirl 26.12 an empty logFileEncoding.
 *
 * It owns what a Regex Lab's answer needs until the answer comes. The host
 * gets a number as the callback's user_data, never a pointer, so an answer
 * that comes after the adapter is destroyed finds nothing and does nothing.
 * Made, used and destroyed on the UI thread only.
 */
class HostApiAdapter {
public:
    /// @p api and @p handle must stay valid until the adapter is destroyed.
    /// @p apiSize is the size of the table in bytes, as the host passed it to
    /// logsquirl_plugin_init_ex(), or LOGSQUIRL_HOST_API_BASE_SIZE.
    HostApiAdapter( const LogSquirlHostApi* api, void* handle, std::size_t apiSize );

    /// Releases the done functions of the Regex Labs not answered yet: an
    /// answer that comes later does nothing. The functions() and their
    /// copies must not be called afterwards.
    ~HostApiAdapter();

    HostApiAdapter( const HostApiAdapter& ) = delete;
    HostApiAdapter& operator=( const HostApiAdapter& ) = delete;

    /// The host functions, valid until the adapter is destroyed.
    const HostFunctions& functions() const noexcept
    {
        return functions_;
    }

private:
    bool openRegexLab( const RegexLabPattern& pattern, const QObject* context, RegexLabDone done );

    const LogSquirlHostApi* const api_;
    void* const handle_;
    HostFunctions functions_;
};

} // namespace custom_footer
