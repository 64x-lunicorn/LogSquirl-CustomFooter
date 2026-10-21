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
 *   - logsquirl_plugin_init_ex()    → register widgets and callbacks (26.11)
 *   - logsquirl_plugin_init()       → the same, for older hosts
 *   - logsquirl_plugin_shutdown()   → clean up widgets
 *   - logsquirl_plugin_configure()  → open rule editor dialog
 *
 * PLUGIN LIFECYCLE
 * ────────────────
 *   1. Host calls get_info() to read metadata.
 *   2. Host calls init_ex(api, handle, api_size), or an older host
 *      init(api, handle), which is init_ex() with the base table's size —
 *      we turn the table into HostFunctions (HostApiAdapter, which reads
 *      only the members api_size reports), refuse a table that lacks a base
 *      function we call, make the footer widget and a
 *      FooterSession on it, and register the widget, a menu action and an
 *      active-file callback. The menu action is "Custom Footer…", or, where
 *      the host hands over the selected Log Lines (26.11), a submenu
 *      "Custom Footer" with "Edit Rules…" and "Rule from Selected Line…".
 *   3. When the active file changes, onActiveFileChanged() hands it to the
 *      session, for the footer and the rule editor; a call off the UI
 *      thread is logged and ignored.
 *   4. Host calls shutdown() — destroying the session stops everything of
 *      the plugin; then the footer widget is unregistered and deleted, and
 *      the adapter drops the Regex Lab answers still awaited.
 *
 * This file only translates between the host API and the session. No
 * exception may leave an entry point or a callback: the host is C.
 */

#include "plugin.h"

#include "footerdisplaywidget.h"
#include "footersession.h"
#include "hostapiadapter.h"
#include "hostfunctions.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QThread>

#include <memory>

using custom_footer::FooterDisplayWidget;
using custom_footer::FooterSession;
using custom_footer::HostApiAdapter;
using custom_footer::HostFeature;
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
    /// The host functions of api; destroyed last, after the session.
    std::unique_ptr<HostApiAdapter> hostApi;

    /// hostApi's functions; empty while the plugin is not initialised.
    const HostFunctions& host() const noexcept
    {
        static const HostFunctions none;
        return hostApi ? hostApi->functions() : none;
    }

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

/// "Custom Footer plugin ready.", with the host functions the session has.
QString readyMessage( const FooterSession& session )
{
    QStringList offered;
    if ( session.has( HostFeature::RegexLab ) ) {
        offered.append( QStringLiteral( "Regex Lab" ) );
    }
    if ( session.has( HostFeature::GoToLogLine ) ) {
        offered.append( QStringLiteral( "Go to line" ) );
    }
    if ( session.has( HostFeature::SelectedLogLines ) ) {
        offered.append( QStringLiteral( "selected Log Lines" ) );
    }
    return QStringLiteral( "Custom Footer plugin ready. Host functions: %1." )
        .arg( offered.isEmpty() ? QStringLiteral( "none" )
                                : offered.join( QStringLiteral( ", " ) ) );
}

/// A string from the host, or an empty one for none.
QString fromHost( const char* text )
{
    return text ? QString::fromUtf8( text ) : QString();
}

/// The names of the base table's functions that the plugin calls without
/// asking, and @p api has as NULL.
QStringList missingFunctions( const LogSquirlHostApi& api )
{
    QStringList missing;
    const auto need = [ &missing ]( bool present, const char* name ) {
        if ( !present ) {
            missing.append( QString::fromLatin1( name ) );
        }
    };
    need( api.get_config_dir, "get_config_dir" );
    need( api.register_menu_action, "register_menu_action" );
    need( api.register_footer_widget, "register_footer_widget" );
    need( api.unregister_footer_widget, "unregister_footer_widget" );
    need( api.get_active_file_path, "get_active_file_path" );
    need( api.register_active_file_callback, "register_active_file_callback" );
    return missing;
}

/// Whether this is the thread of the application, where the widgets live.
bool onUiThread()
{
    const auto* app = QCoreApplication::instance();
    return app && QThread::currentThread() == app->thread();
}

} // namespace

// ── Callbacks ───────────────────────────────────────────────────────────

/// Called by the host, on the UI thread, when the active file changes. The
/// plugin API does not promise that thread, and the session's widgets and
/// timers live there: a call from another thread is logged (log_message
/// may be called from any thread) and ignored.
static void onActiveFileChanged( void* /* userData */, const char* filePath )
{
    guarded( plugin().host(), "scanning the active file", [ filePath ] {
        if ( !onUiThread() ) {
            plugin().host().logMessage( LOGSQUIRL_LOG_ERROR,
                                        QStringLiteral( "Custom Footer: the host reported the "
                                                        "active file off the UI thread; ignored" ) );
            return;
        }
        if ( auto* session = plugin().session.get() ) {
            session->setActiveFile( fromHost( filePath ) );
        }
    } );
}

/// Whether a menu action is called on the UI thread, where it opens the
/// editor; logs an error if not, as onActiveFileChanged() does.
static bool menuActionOnUiThread()
{
    if ( onUiThread() ) {
        return true;
    }
    plugin().host().logMessage( LOGSQUIRL_LOG_ERROR,
                                QStringLiteral( "Custom Footer: the host called a menu action off "
                                                "the UI thread; ignored" ) );
    return false;
}

/// Called when the user clicks "Custom Footer…" in the Plugins menu, or
/// "Custom Footer → Edit Rules…" on a host with the selected Log Lines.
/// Ignored, and logged, off the UI thread.
static void onEditorMenuAction( void* /* userData */ )
{
    guarded( plugin().host(), "the rule editor", [] {
        if ( !menuActionOnUiThread() ) {
            return;
        }
        auto& state = plugin();
        if ( state.session ) {
            state.session->openEditor( state.footer ? state.footer->window() : nullptr );
        }
    } );
}

/// Called when the user clicks "Custom Footer → Rule from Selected Line…"
/// in the Plugins menu; registered only where the host offers the
/// selected Log Lines. Ignored, and logged, off the UI thread.
static void onRuleFromLineMenuAction( void* /* userData */ )
{
    guarded( plugin().host(), "making a rule from the selected line", [] {
        if ( !menuActionOnUiThread() ) {
            return;
        }
        auto& state = plugin();
        if ( state.session ) {
            state.session->ruleFromSelectedLine( state.footer ? state.footer->window() : nullptr );
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
    // An older host: its table ends where the functions added later begin.
    return logsquirl_plugin_init_ex( api, handle, LOGSQUIRL_HOST_API_BASE_SIZE );
}

LOGSQUIRL_PLUGIN_EXPORT int logsquirl_plugin_init_ex( const LogSquirlHostApi* api, void* handle,
                                                      size_t api_size )
{
    if ( !api || !handle || api_size < LOGSQUIRL_HOST_API_BASE_SIZE ) {
        return 1;
    }

    // Guard against double-initialisation: clean up previous state.
    if ( plugin().api ) {
        logsquirl_plugin_shutdown();
    }

    auto& state = plugin();
    try {
        // The pointer is kept, the table never copied; only the members
        // api_size reports are read.
        state.hostApi = std::make_unique<HostApiAdapter>( api, handle, api_size );
    } catch ( ... ) {
        return 1;
    }
    state.api = api;
    state.handle = handle;

    bool complete = false;
    const bool ok = guarded( state.host(), "initialisation", [ &state, &complete, api, handle ] {
        const auto& host = state.host();
        host.logMessage( LOGSQUIRL_LOG_INFO,
                         QStringLiteral( "Custom Footer plugin initialising…" ) );

        // Every function called below, and unregister_footer_widget at
        // shutdown, is called without asking; a table that lacks one is
        // refused before anything is registered.
        const auto missing = missingFunctions( *api );
        if ( !missing.isEmpty() ) {
            host.logMessage( LOGSQUIRL_LOG_ERROR,
                             QStringLiteral( "Custom Footer: the host's table has no %1; the "
                                             "plugin is not initialised" )
                                 .arg( missing.join( QStringLiteral( ", " ) ) ) );
            return;
        }

        const auto configDir = fromHost( api->get_config_dir( handle ) );
        if ( configDir.isEmpty() ) {
            host.logMessage( LOGSQUIRL_LOG_WARNING,
                             QStringLiteral( "Custom Footer: the host gave no config directory; "
                                             "rules are neither loaded nor saved" ) );
        }

        // Register menu action to open the rule editor, where a host
        // without the selected Log Lines (26.10) has it, as with 0.4.0. One
        // test for both menus: the host functions the session is made with,
        // so it is what has( HostFeature::SelectedLogLines ) reports.
        const bool ruleFromLine = static_cast<bool>( host.selectedLogLines );
        if ( !ruleFromLine ) {
            api->register_menu_action( handle, "Plugins", "Custom Footer…", &onEditorMenuAction,
                                       nullptr );
        }

        // Create the footer display widget, and the session that fills it;
        // the session loads the rules and logs their problems.
        state.footer = new FooterDisplayWidget();
        api->register_footer_widget( handle, static_cast<void*>( state.footer ) );
        state.session = std::make_unique<FooterSession>( configDir, state.footer, host );

        // Where the host hands over the selected Log Lines (26.11), a rule
        // can be made from one: both actions go into a submenu
        // Plugins → Custom Footer.
        if ( ruleFromLine ) {
            api->register_menu_action( handle, "Custom Footer", "Edit Rules…", &onEditorMenuAction,
                                       nullptr );
            api->register_menu_action( handle, "Custom Footer", "Rule from Selected Line…",
                                       &onRuleFromLineMenuAction, nullptr );
        }

        // Register callback for active file changes.
        api->register_active_file_callback( handle, &onActiveFileChanged, nullptr );

        // Initial scan if a file is already open. Read here, on the UI
        // thread, and pushed into the session, which never asks the host.
        state.session->setActiveFile( fromHost( api->get_active_file_path( handle ) ) );

        host.logMessage( LOGSQUIRL_LOG_INFO, readyMessage( *state.session ) );
        complete = true;
    } );

    if ( !ok || !complete ) {
        logsquirl_plugin_shutdown();
        return 1;
    }
    return 0;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_shutdown( void )
{
    auto& state = plugin();
    guarded( state.host(), "shutdown", [ &state ] {
        state.host().logMessage( LOGSQUIRL_LOG_INFO,
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

    // Last the adapter, which the session's host functions call: a Regex
    // Lab answer the host still sent would now find nothing.
    state.hostApi.reset();
    state.api = nullptr;
    state.handle = nullptr;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_configure( void* parent_widget )
{
    // The editor is opened via the Plugins menu action.
    // configure() also opens it as a convenience, over the host's window.
    guarded( plugin().host(), "the rule editor", [ parent_widget ] {
        if ( auto* session = plugin().session.get() ) {
            session->openEditor( static_cast<QWidget*>( parent_widget ) );
        }
    } );
}

} // extern "C"
