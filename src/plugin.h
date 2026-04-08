/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "logsquirl_plugin_api.h"

#include "footerdisplaywidget.h"

namespace costume_footer {

/// Global plugin state — bridges C ABI entry points to C++ implementation.
struct PluginState {
    const LogSquirlHostApi* api = nullptr;
    void* handle = nullptr;
    bool initialised = false;

    FooterDisplayWidget* statusWidget = nullptr;  ///< Registered in status bar.
    FooterDisplayWidget* sidebarWidget = nullptr;  ///< Registered in sidebar tab.
};

extern PluginState g_state;

/// Convenience wrapper: log via host API.
void hostLog( int level, const char* message );

} // namespace costume_footer
