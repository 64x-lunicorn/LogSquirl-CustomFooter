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

namespace custom_footer {
PluginState g_state;

void hostLog( int level, const char* message )
{
    if ( g_state.api && g_state.handle ) {
        g_state.api->log_message( g_state.handle, level, message );
    }
}
} // namespace custom_footer

// ── Static plugin info ──────────────────────────────────────────────────

static const LogSquirlPluginInfo kPluginInfo = {
    /* id          */ "io.github.logsquirl.custom-footer",
    /* name        */ "Custom Footer",
    /* version     */ "0.1.0",
    /* description */ "Extract key-value pairs from logs via regex and display in footer",
    /* author      */ "LogSquirl Contributors",
    /* license     */ "GPL-3.0-or-later",
    /* type        */ LOGSQUIRL_PLUGIN_UI,
    /* api_version */ LOGSQUIRL_PLUGIN_API_VERSION,
};

// ── Internal helpers ─────────────────────────────────────────────────────

/// Return the plugin config directory (from host API).
static QString configDir()
{
    if ( !custom_footer::g_state.api || !custom_footer::g_state.handle ) {
        return {};
    }
    const char* dir
        = custom_footer::g_state.api->get_config_dir( custom_footer::g_state.handle );
    return dir ? QString::fromUtf8( dir ) : QString();
}

/// Re-scan the active file and update display widgets.
static void rescanActiveFile()
{
    const auto& st = custom_footer::g_state;
    if ( !st.api || !st.handle ) {
        return;
    }

    // Get current file path from host.
    const char* pathUtf8 = st.api->get_active_file_path( st.handle );
    const QString filePath = pathUtf8 ? QString::fromUtf8( pathUtf8 ) : QString();

    if ( filePath.isEmpty() ) {
        if ( st.footerWidget ) {
            st.footerWidget->clearValues();
        }
        return;
    }

    const auto dir = configDir();
    const auto entries = custom_footer::FooterConfig::loadEntries( dir );
    const int maxLines = custom_footer::FooterConfig::loadMaxLines( dir );

    const auto results = custom_footer::FooterScanner::scan( filePath, entries, maxLines );

    // Build an ordered pair list following the entry definition order.
    QList<QPair<QString, QString>> ordered;
    for ( const auto& entry : entries ) {
        if ( !entry.enabled ) {
            continue;
        }
        auto it = results.find( entry.key );
        if ( it != results.end() ) {
            ordered.append( { entry.key, it.value() } );
        }
    }

    if ( st.footerWidget ) {
        st.footerWidget->updateValues( ordered );
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
    auto entries = custom_footer::FooterConfig::loadEntries( dir );

    custom_footer::FooterEditor editor( entries, nullptr );

    // Apply button: save and rescan without closing the dialog.
    QObject::connect( &editor, &custom_footer::FooterEditor::applied, [&editor]() {
        const auto d = configDir();
        custom_footer::FooterConfig::saveEntries( d, editor.entries() );
        rescanActiveFile();
    } );

    if ( editor.exec() == QDialog::Accepted ) {
        custom_footer::FooterConfig::saveEntries( dir, editor.entries() );
        rescanActiveFile();
    }
}

// ── Exported C entry points ──────────────────────────────────────────────

extern "C" {

LOGSQUIRL_PLUGIN_EXPORT const LogSquirlPluginInfo* logsquirl_plugin_get_info( void );
LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_shutdown( void );

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
    if ( custom_footer::g_state.initialised ) {
        logsquirl_plugin_shutdown();
    }

    custom_footer::g_state.api = api;
    custom_footer::g_state.handle = handle;
    custom_footer::g_state.initialised = true;

    api->log_message( handle, LOGSQUIRL_LOG_INFO, "Custom Footer plugin initialising…" );

    // Register menu action to open the rule editor.
    api->register_menu_action( handle, "Plugins", "Custom Footer\u2026",
                               &showEditorDialog, nullptr );

    // Create footer display widget.
    custom_footer::g_state.footerWidget = new custom_footer::FooterDisplayWidget();
    api->register_footer_widget( handle,
                                 static_cast<void*>( custom_footer::g_state.footerWidget ) );

    // Register callback for active file changes.
    api->register_active_file_callback( handle, &onActiveFileChanged, nullptr );

    // Initial scan if a file is already open.
    rescanActiveFile();

    api->log_message( handle, LOGSQUIRL_LOG_INFO, "Custom Footer plugin ready." );
    return 0;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_shutdown( void )
{
    custom_footer::hostLog( LOGSQUIRL_LOG_INFO, "Custom Footer plugin shutting down…" );

    if ( custom_footer::g_state.footerWidget ) {
        custom_footer::g_state.api->unregister_footer_widget(
            custom_footer::g_state.handle,
            static_cast<void*>( custom_footer::g_state.footerWidget ) );
        delete custom_footer::g_state.footerWidget;
        custom_footer::g_state.footerWidget = nullptr;
    }

    custom_footer::g_state.api = nullptr;
    custom_footer::g_state.handle = nullptr;
    custom_footer::g_state.initialised = false;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_configure( void* parent_widget )
{
    (void)parent_widget;
    // The editor is opened via the Plugins menu action.
    // configure() also opens it as a convenience.
    showEditorDialog( nullptr );
}

} // extern "C"
