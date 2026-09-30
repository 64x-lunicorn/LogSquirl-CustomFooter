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
 *   3. When the active file changes, onActiveFileChanged() hands it to the
 *      FooterController, which scans it in the background.
 *   4. Host calls shutdown() — we stop scanning, unregister and delete
 *      everything.
 *
 * No exception may leave an entry point or a callback: the host is C.
 */

#include "plugin.h"

#include "footerconfig.h"
#include "footercontroller.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"

#include <QString>

#include <exception>

// ── Global state ─────────────────────────────────────────────────────────

namespace custom_footer {
PluginState g_state;

void hostLog( int level, const char* message )
{
    if ( g_state.api && g_state.handle ) {
        g_state.api->log_message( g_state.handle, level, message );
    }
}

void hostLog( int level, const QString& message )
{
    hostLog( level, message.toUtf8().constData() );
}
} // namespace custom_footer

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

// ── Internal helpers ─────────────────────────────────────────────────────

/// Log that work for the host failed.
static void logFailure( const char* what, const char* reason ) noexcept
{
    try {
        custom_footer::hostLog(
            LOGSQUIRL_LOG_ERROR,
            QStringLiteral( "Custom Footer: %1 failed: %2" ).arg( what, reason ) );
    } catch ( ... ) {
        custom_footer::hostLog( LOGSQUIRL_LOG_ERROR, "Custom Footer: an operation failed" );
    }
}

/// Run work for the host, logging instead of throwing. Returns false if it threw.
template <typename Work>
static bool guarded( const char* what, Work&& work ) noexcept
{
    try {
        work();
        return true;
    } catch ( const std::exception& e ) {
        logFailure( what, e.what() );
    } catch ( ... ) {
        logFailure( what, "unknown exception" );
    }
    return false;
}

/// Return the plugin config directory (from host API).
static QString configDir()
{
    if ( !custom_footer::g_state.api || !custom_footer::g_state.handle ) {
        return {};
    }
    const char* dir = custom_footer::g_state.api->get_config_dir( custom_footer::g_state.handle );
    return dir ? QString::fromUtf8( dir ) : QString();
}

/// Save the edited rules and show their results.
static void saveEntries( const QList<custom_footer::FooterEntry>& entries )
{
    auto* controller = custom_footer::g_state.controller;
    if ( !controller ) {
        return;
    }
    if ( !custom_footer::FooterConfig::saveEntries( controller->configDir(), entries ) ) {
        custom_footer::hostLog( LOGSQUIRL_LOG_WARNING,
                                "Custom Footer: could not save the rules to the config directory" );
    }
    controller->reloadConfig();
}

/// The host's active file, or an empty string without one.
static QString activeFilePath()
{
    const auto& st = custom_footer::g_state;
    if ( !st.api || !st.handle ) {
        return {};
    }
    const char* filePath = st.api->get_active_file_path( st.handle );
    return filePath ? QString::fromUtf8( filePath ) : QString();
}

/// Open the rule editor as a modal dialog over the given parent.
static void showEditorDialog( QWidget* parent )
{
    const auto dir = configDir();
    custom_footer::FooterEditor editor( custom_footer::FooterConfig::loadEntries( dir ), parent );
    editor.setMaxLines( custom_footer::FooterConfig::loadMaxLines( dir ) );
    editor.setActiveFile( activeFilePath() );

    // Known while open, so that the preview follows the active file and
    // shutdown can stop it; forgotten however the dialog ends.
    struct OpenEditor {
        explicit OpenEditor( custom_footer::FooterEditor* editor )
        {
            custom_footer::g_state.editor = editor;
        }
        ~OpenEditor()
        {
            custom_footer::g_state.editor = nullptr;
        }
        OpenEditor( const OpenEditor& ) = delete;
        OpenEditor& operator=( const OpenEditor& ) = delete;
    } open( &editor );

    // Apply button: save and rescan without closing the dialog.
    QObject::connect( &editor, &custom_footer::FooterEditor::applied,
                      [ &editor ]() { saveEntries( editor.entries() ); } );

    if ( editor.exec() == QDialog::Accepted ) {
        saveEntries( editor.entries() );
    }
}

/// Called by the host when the active file changes.
static void onActiveFileChanged( void* /* userData */, const char* filePath )
{
    guarded( "scanning the active file", [ filePath ] {
        const auto path = filePath ? QString::fromUtf8( filePath ) : QString();
        if ( custom_footer::g_state.controller ) {
            custom_footer::g_state.controller->setActiveFile( path );
        }
        if ( custom_footer::g_state.editor ) {
            custom_footer::g_state.editor->setActiveFile( path );
        }
    } );
}

/// Called when the user clicks "Custom Footer…" in the Plugins menu.
static void onEditorMenuAction( void* /* userData */ )
{
    guarded( "the rule editor", [] {
        const auto* footer = custom_footer::g_state.footerWidget;
        showEditorDialog( footer ? footer->window() : nullptr );
    } );
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

    const bool ok = guarded( "initialisation", [ api, handle ] {
        auto& st = custom_footer::g_state;
        api->log_message( handle, LOGSQUIRL_LOG_INFO, "Custom Footer plugin initialising…" );

        const auto dir = configDir();
        if ( dir.isEmpty() ) {
            api->log_message( handle, LOGSQUIRL_LOG_WARNING,
                              "Custom Footer: the host gave no config directory; rules are "
                              "neither loaded nor saved" );
        }

        // Register menu action to open the rule editor.
        api->register_menu_action( handle, "Plugins", "Custom Footer…", &onEditorMenuAction,
                                   nullptr );

        // Create footer display widget, and the controller that fills it.
        st.footerWidget = new custom_footer::FooterDisplayWidget();
        api->register_footer_widget( handle, static_cast<void*>( st.footerWidget ) );
        st.controller = new custom_footer::FooterController( st.footerWidget, dir );

        // Register callback for active file changes.
        api->register_active_file_callback( handle, &onActiveFileChanged, nullptr );

        // Initial scan if a file is already open.
        st.controller->setActiveFile( activeFilePath() );

        api->log_message( handle, LOGSQUIRL_LOG_INFO, "Custom Footer plugin ready." );
    } );

    if ( !ok ) {
        logsquirl_plugin_shutdown();
        return 1;
    }
    return 0;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_shutdown( void )
{
    guarded( "shutdown", [] {
        auto& st = custom_footer::g_state;
        custom_footer::hostLog( LOGSQUIRL_LOG_INFO, "Custom Footer plugin shutting down…" );

        // First stop scanning: the host unloads the library after this returns.
        // An open editor's preview is stopped too, and the editor closed.
        if ( st.editor ) {
            st.editor->stopPreview();
            st.editor->reject();
        }
        delete st.controller;
        st.controller = nullptr;

        if ( st.footerWidget ) {
            st.api->unregister_footer_widget( st.handle, static_cast<void*>( st.footerWidget ) );
            delete st.footerWidget;
            st.footerWidget = nullptr;
        }
    } );

    custom_footer::g_state.api = nullptr;
    custom_footer::g_state.handle = nullptr;
    custom_footer::g_state.initialised = false;
}

LOGSQUIRL_PLUGIN_EXPORT void logsquirl_plugin_configure( void* parent_widget )
{
    // The editor is opened via the Plugins menu action.
    // configure() also opens it as a convenience, over the host's window.
    guarded( "the rule editor",
             [ parent_widget ] { showEditorDialog( static_cast<QWidget*>( parent_widget ) ); } );
}

} // extern "C"
