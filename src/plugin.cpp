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
 *   2. Host calls init(api, handle) — we register a status widget,
 *      an optional sidebar tab, a menu action, and an active-file callback.
 *   3. When the active file changes, onActiveFileChanged() re-scans.
 *   4. Host calls shutdown() — we unregister and delete everything.
 */

#include "plugin.h"

#include "footerconfig.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"
#include "footerscanner.h"

#include <QString>

// ── Global state ─────────────────────────────────────────────────────────

namespace costume_footer {
PluginState g_state;

void hostLog( int level, const char* message )
{
    if ( g_state.api && g_state.handle ) {
        g_state.api->log_message( g_state.handle, level, message );
    }
}
} // namespace costume_footer

// ── Static plugin info ──────────────────────────────────────────────────

static const LogSquirlPluginInfo kPluginInfo = {
    /* id          */ "io.github.logsquirl.costume-footer",
    /* name        */ "Custom Footer",
    /* version     */ "0.1.0",
    /* description */ "Extract key-value pairs from logs via regex and display in footer/sidebar",
    /* author      */ "LogSquirl Contributors",
    /* license     */ "GPL-3.0-or-later",
    /* type        */ LOGSQUIRL_PLUGIN_UI,
    /* api_version */ LOGSQUIRL_PLUGIN_API_VERSION,
};

// ── Internal helpers ─────────────────────────────────────────────────────

/// Return the plugin config directory (from host API).
static QString configDir()
{
    if ( !costume_footer::g_state.api || !costume_footer::g_state.handle ) {
        return {};
    }
    const char* dir
        = costume_footer::g_state.api->get_config_dir( costume_footer::g_state.handle );
    return dir ? QString::fromUtf8( dir ) : QString();
}

/// Re-scan the active file and update display widgets.
static void rescanActiveFile()
{
    const auto& st = costume_footer::g_state;
    if ( !st.api || !st.handle ) {
        return;
    }

    // Get current file path from host.
    const char* pathUtf8 = st.api->get_active_file_path( st.handle );
    const QString filePath = pathUtf8 ? QString::fromUtf8( pathUtf8 ) : QString();

    if ( filePath.isEmpty() ) {
        if ( st.statusWidget ) {
            st.statusWidget->clearValues();
        }
        if ( st.sidebarWidget ) {
            st.sidebarWidget->clearValues();
        }
        return;
    }

    const auto dir = configDir();
    const auto entries = costume_footer::FooterConfig::loadEntries( dir );
    const int maxLines = costume_footer::FooterConfig::loadMaxLines( dir );
    const auto mode = costume_footer::FooterConfig::loadDisplayMode( dir );

    const auto results = costume_footer::FooterScanner::scan( filePath, entries, maxLines );

    // Update the appropriate widget(s) based on display mode.
    if ( st.statusWidget ) {
        if ( mode == costume_footer::DisplayMode::Footer
             || mode == costume_footer::DisplayMode::Both ) {
            st.statusWidget->updateValues( results );
        }
        else {
            st.statusWidget->clearValues();
        }
    }
    if ( st.sidebarWidget ) {
        if ( mode == costume_footer::DisplayMode::Sidebar
             || mode == costume_footer::DisplayMode::Both ) {
            st.sidebarWidget->updateValues( results );
        }
        else {
            st.sidebarWidget->clearValues();
        }
    }
}

/// Called by the host when the active file changes.
static void onActiveFileChanged( void* /* userData */, const char* /* filePath */ )
{
    rescanActiveFile();
}

/// Called when the user clicks "Custom Footer…" in the Plugins menu.
static void showEditorDialog( void* /* userData */ )
{
    const auto dir = configDir();
    auto entries = costume_footer::FooterConfig::loadEntries( dir );
    auto mode = costume_footer::FooterConfig::loadDisplayMode( dir );

    costume_footer::FooterEditor editor( entries, mode, nullptr );

    // Apply button: save and rescan without closing the dialog.
    QObject::connect( &editor, &costume_footer::FooterEditor::applied, [&editor]() {
        const auto d = configDir();
        costume_footer::FooterConfig::saveEntries( d, editor.entries() );
        costume_footer::FooterConfig::saveDisplayMode( d, editor.displayMode() );
        rescanActiveFile();
    } );

    if ( editor.exec() == QDialog::Accepted ) {
        costume_footer::FooterConfig::saveEntries( dir, editor.entries() );
        costume_footer::FooterConfig::saveDisplayMode( dir, editor.displayMode() );
        rescanActiveFile();
    }
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

    costume_footer::g_state.api = api;
    costume_footer::g_state.handle = handle;
    costume_footer::g_state.initialised = true;

    api->log_message( handle, LOGSQUIRL_LOG_INFO, "Custom Footer plugin initialising…" );

    // Register menu action to open the rule editor.
    api->register_menu_action( handle, "Plugins", "Custom Footer\u2026",
                               &showEditorDialog, nullptr );

    // Create display widgets.
    costume_footer::g_state.statusWidget = new costume_footer::FooterDisplayWidget();
    api->register_status_widget( handle,
                                 static_cast<void*>( costume_footer::g_state.statusWidget ) );

    costume_footer::g_state.sidebarWidget = new costume_footer::FooterDisplayWidget();
    api->register_sidebar_tab( handle, "Custom Footer",
                               static_cast<void*>( costume_footer::g_state.sidebarWidget ) );

    // Register callback for active file changes.
    api->register_active_file_callback( handle, &onActiveFileChanged, nullptr );

    // Initial scan if a file is already open.
    rescanActiveFile();

    api->log_message( handle, LOGSQUIRL_LOG_INFO, "Custom Footer plugin ready." );
    return 0;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_shutdown( void )
{
    costume_footer::hostLog( LOGSQUIRL_LOG_INFO, "Custom Footer plugin shutting down…" );

    if ( costume_footer::g_state.sidebarWidget ) {
        costume_footer::g_state.api->unregister_sidebar_tab(
            costume_footer::g_state.handle,
            static_cast<void*>( costume_footer::g_state.sidebarWidget ) );
        delete costume_footer::g_state.sidebarWidget;
        costume_footer::g_state.sidebarWidget = nullptr;
    }

    if ( costume_footer::g_state.statusWidget ) {
        costume_footer::g_state.api->unregister_status_widget(
            costume_footer::g_state.handle,
            static_cast<void*>( costume_footer::g_state.statusWidget ) );
        delete costume_footer::g_state.statusWidget;
        costume_footer::g_state.statusWidget = nullptr;
    }

    costume_footer::g_state.api = nullptr;
    costume_footer::g_state.handle = nullptr;
    costume_footer::g_state.initialised = false;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_configure( void* parent_widget )
{
    (void)parent_widget;
    // The editor is opened via the Plugins menu action.
    // configure() also opens it as a convenience.
    showEditorDialog( nullptr );
}

} // extern "C"
