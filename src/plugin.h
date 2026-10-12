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

/*
 * The plugin's C entry points, which the host looks up by name. They only
 * translate between the host API and a FooterSession; the plugin keeps no
 * state that is visible outside plugin.cpp.
 */
extern "C" {

LOGSQUIRL_PLUGIN_EXPORT const LogSquirlPluginInfo* logsquirl_plugin_get_info( void );
/// Called by LogSquirl 26.10 and older: init_ex() with the base table.
LOGSQUIRL_PLUGIN_EXPORT int logsquirl_plugin_init( const LogSquirlHostApi* api, void* handle );
/// Called instead of init() by LogSquirl 26.11 and newer, with the size of
/// their table, which tells the host functions they offer.
LOGSQUIRL_PLUGIN_EXPORT int logsquirl_plugin_init_ex( const LogSquirlHostApi* api, void* handle,
                                                      size_t api_size );
LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_shutdown( void );
LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_configure( void* parent_widget );

} // extern "C"
