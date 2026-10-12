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

/**
 * @file plugin.cpp
 * @brief C ABI entry points for the LogSquirl Custom Footer plugin.
 *
 * Exported symbols:
 *   - logsquirl_plugin_get_info()   → static metadata
 *   - logsquirl_plugin_init()       → register widgets and callbacks
 *   - logsquirl_plugin_shutdown()   → clean up widgets
 *   - logsquirl_plugin_configure()  → open rule editor dialog
 *
 * PLUGIN LIFECYCLE
 * ────────────────
 *   1. Host calls get_info() to read metadata.
 *   2. Host calls init(api, handle) — we make the footer widget and a
 *      FooterSession on it, and register the widget, a menu action and an
 *      active-file callback.
 *   3. When the active file changes, onActiveFileChanged() hands it to the
 *      session, for the footer and the rule editor.
 *   4. Host calls shutdown() — destroying the session stops everything of
 *      the plugin; then the footer widget is unregistered and deleted.
 *
 * This file only translates between the host API and the session. No
 * exception may leave an entry point or a callback: the host is C.
 */

#include "plugin.h"

#include "footerdisplaywidget.h"
#include "footersession.h"
#include "hostfunctions.h"

#include <QString>

#include <memory>

using custom_footer::FooterDisplayWidget;
using custom_footer::FooterSession;
using custom_footer::HostFunctions;

// ── Static plugin info ──────────────────────────────────────────────────

static const LogSquirlPluginInfo kPluginInfo = {
    /* id          */ "io.github.logsquirl.customfooter",
    /* name        */ "Custom Footer",
    /* version     */ LOGSQUIRL_PLUGIN_VERSION,
    /* description */ "Extract key-value pairs from logs via regex and display in footer",
    /* author      */ "LogSquirl Contributors",
    /* license     */ "GPL-3.0-or-later",
    /* type        */ LOGSQUIRL_PLUGIN_UI,
    /* api_version */ LOGSQUIRL_PLUGIN_API_VERSION,
};

// ── State, from init() until shutdown() ─────────────────────────────────

namespace {

struct Plugin {
    const LogSquirlHostApi* api = nullptr;
    void* handle = nullptr;
    /// Built from api; empty while the plugin is not initialised.
    HostFunctions host;
    /// Owned here, as the host must unregister it after the session is gone.
    FooterDisplayWidget* footer = nullptr;
    std::unique_ptr<FooterSession> session;
};

/// Never destroyed: a host that unloads the library without shutdown()
/// must not run the session's destructor during static destruction.
Plugin& plugin()
{
    static auto* instance = new Plugin;
    return *instance;
}

/// What the session needs from the host, from its function table.
HostFunctions hostFunctions( const LogSquirlHostApi* api, void* handle )
{
    HostFunctions host;
    if ( api->log_message ) {
        host.log = [ api, handle ]( int level, const QString& message ) {
            api->log_message( handle, level, message.toUtf8().constData() );
        };
    }
    return host;
}

/// A string from the host, or an empty one for none.
QString fromHost( const char* text )
{
    return text ? QString::fromUtf8( text ) : QString();
}

} // namespace

// ── Callbacks ───────────────────────────────────────────────────────────

/// Called by the host, on the UI thread, when the active file changes.
static void onActiveFileChanged( void* /* userData */, const char* filePath )
{
    guarded( plugin().host, "scanning the active file", [ filePath ] {
        if ( auto* session = plugin().session.get() ) {
            session->setActiveFile( fromHost( filePath ) );
        }
    } );
}

/// Called when the user clicks "Custom Footer…" in the Plugins menu.
static void onEditorMenuAction( void* /* userData */ )
{
    guarded( plugin().host, "the rule editor", [] {
        auto& state = plugin();
        if ( state.session ) {
            state.session->openEditor( state.footer ? state.footer->window() : nullptr );
        }
    } );
}

// ── Exported C entry points ──────────────────────────────────────────────

extern "C" {

LOGSQUIRL_PLUGIN_EXPORT const LogSquirlPluginInfo* logsquirl_plugin_get_info( void )
{
    return &kPluginInfo;
}

LOGSQUIRL_PLUGIN_EXPORT int logsquirl_plugin_init( const LogSquirlHostApi* api, void* handle )
{
    if ( !api || !handle ) {
        return 1;
    }

    // Guard against double-initialisation: clean up previous state.
    if ( plugin().api ) {
        logsquirl_plugin_shutdown();
    }

    auto& state = plugin();
    state.api = api;
    state.handle = handle;
    state.host = hostFunctions( api, handle );

    const bool ok = guarded( state.host, "initialisation", [ &state, api, handle ] {
        const auto& host = state.host;
        host.logMessage( LOGSQUIRL_LOG_INFO,
                         QStringLiteral( "Custom Footer plugin initialising…" ) );

        const auto configDir = fromHost( api->get_config_dir( handle ) );
        if ( configDir.isEmpty() ) {
            host.logMessage( LOGSQUIRL_LOG_WARNING,
                             QStringLiteral( "Custom Footer: the host gave no config directory; "
                                             "rules are neither loaded nor saved" ) );
        }

        // Register menu action to open the rule editor.
        api->register_menu_action( handle, "Plugins", "Custom Footer…", &onEditorMenuAction,
                                   nullptr );

        // Create the footer display widget, and the session that fills it;
        // the session loads the rules and logs their problems.
        state.footer = new FooterDisplayWidget();
        api->register_footer_widget( handle, static_cast<void*>( state.footer ) );
        state.session = std::make_unique<FooterSession>( configDir, state.footer, host );

        // Register callback for active file changes.
        api->register_active_file_callback( handle, &onActiveFileChanged, nullptr );

        // Initial scan if a file is already open. Read here, on the UI
        // thread, and pushed into the session, which never asks the host.
        state.session->setActiveFile( fromHost( api->get_active_file_path( handle ) ) );

        host.logMessage( LOGSQUIRL_LOG_INFO, QStringLiteral( "Custom Footer plugin ready." ) );
    } );

    if ( !ok ) {
        logsquirl_plugin_shutdown();
        return 1;
    }
    return 0;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_shutdown( void )
{
    auto& state = plugin();
    guarded( state.host, "shutdown", [ &state ] {
        state.host.logMessage( LOGSQUIRL_LOG_INFO,
                               QStringLiteral( "Custom Footer plugin shutting down…" ) );

        // First the session: when its destructor returns, no plugin code
        // runs, and the host may unload the library after this returns.
        state.session.reset();

        if ( state.footer ) {
            state.api->unregister_footer_widget( state.handle, static_cast<void*>( state.footer ) );
            delete state.footer;
            state.footer = nullptr;
        }
    } );

    state.host = {};
    state.api = nullptr;
    state.handle = nullptr;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_configure( void* parent_widget )
{
    // The editor is opened via the Plugins menu action.
    // configure() also opens it as a convenience, over the host's window.
    guarded( plugin().host, "the rule editor", [ parent_widget ] {
        if ( auto* session = plugin().session.get() ) {
            session->openEditor( static_cast<QWidget*>( parent_widget ) );
        }
    } );
}

} // extern "C"
