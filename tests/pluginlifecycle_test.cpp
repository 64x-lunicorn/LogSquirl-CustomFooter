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
 * @file pluginlifecycle_test.cpp
 * @brief BDD tests driving the plugin through its C ABI with a fake host.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"
#include "logsquirl_plugin_api.h"
#include "plugin.h"
#include "rulepreviewer.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>
#include <QPointer>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

extern "C" int logsquirl_plugin_init( const LogSquirlHostApi* api, void* handle );
extern "C" void logsquirl_plugin_shutdown( void );
extern "C" void logsquirl_plugin_configure( void* parent_widget );

using namespace custom_footer;

namespace {

/// What the fake host was given by the plugin.
struct FakeHost {
    QByteArray configDir;
    QByteArray activeFile;
    QPointer<QWidget> footerWidget;
    QWidget* unregisteredWidget = nullptr;
    void ( *activeFileCallback )( void*, const char* ) = nullptr;
    void* activeFileUserData = nullptr;
};

FakeHost& host()
{
    static FakeHost instance;
    return instance;
}

LogSquirlHostApi fakeApi()
{
    LogSquirlHostApi api{};
    api.api_version = LOGSQUIRL_PLUGIN_API_VERSION;
    api.log_message = []( void*, int, const char* ) {};
    api.get_config_dir = []( void* ) { return host().configDir.constData(); };
    api.register_menu_action = []( void*, const char*, const char*, void ( * )( void* ), void* ) {};
    api.register_footer_widget
        = []( void*, void* widget ) { host().footerWidget = static_cast<QWidget*>( widget ); };
    api.unregister_footer_widget = []( void*, void* widget ) {
        host().unregisteredWidget = static_cast<QWidget*>( widget );
    };
    api.get_active_file_path = []( void* ) { return host().activeFile.constData(); };
    api.register_active_file_callback
        = []( void*, void ( *callback )( void*, const char* ), void* userData ) {
              host().activeFileCallback = callback;
              host().activeFileUserData = userData;
          };
    return api;
}

template <typename Predicate>
bool processUntil( Predicate done, int boundMs = 10000 )
{
    QElapsedTimer timer;
    timer.start();
    while ( !done() ) {
        if ( timer.elapsed() > boundMs ) {
            return false;
        }
        QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
    }
    return true;
}

bool waitForText( const QString& text, int timeoutMs = 5000 )
{
    QElapsedTimer timer;
    timer.start();
    while ( timer.elapsed() < timeoutMs ) {
        if ( const auto* footer = qobject_cast<FooterDisplayWidget*>( host().footerWidget ) ) {
            for ( const auto& value : footer->values() ) {
                if ( value.value.contains( text ) ) {
                    return true;
                }
            }
        }
        QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
        QThread::msleep( 5 );
    }
    return false;
}

void writeFile( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    file.write( content );
}

} // namespace

SCENARIO( "The plugin shows the values of the host's active file", "[plugin]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );

    QList<FooterEntry> entries;
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), entries ) );
    writeFile( logDir.path() + "/first.log", "VIN: FIRST\n" );
    writeFile( logDir.path() + "/second.log", "VIN: SECOND\n" );

    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    host().activeFile = ( logDir.path() + "/first.log" ).toUtf8();
    const auto api = fakeApi();
    int handle = 0;

    GIVEN( "an initialised plugin" )
    {
        REQUIRE( logsquirl_plugin_init( &api, &handle ) == 0 );
        REQUIRE( host().footerWidget );
        REQUIRE( host().activeFileCallback );

        THEN( "the file already open is scanned" )
        {
            REQUIRE( waitForText( "FIRST" ) );
        }

        WHEN( "the host switches to another file" )
        {
            const auto second = ( logDir.path() + "/second.log" ).toUtf8();
            host().activeFileCallback( host().activeFileUserData, second.constData() );

            THEN( "that file's values are shown" )
            {
                REQUIRE( waitForText( "SECOND" ) );
            }
        }

        WHEN( "the rule editor is opened through configure()" )
        {
            QWidget hostWindow;
            logsquirl_plugin_configure( &hostWindow );
            QPointer<FooterEditor> editor = g_state.editor;

            THEN( "it is open without blocking, parented to the widget the host passed" )
            {
                REQUIRE( editor );
                REQUIRE( editor->isVisible() );
                REQUIRE( editor->parentWidget() == &hostWindow );
            }

            AND_WHEN( "it is opened again" )
            {
                logsquirl_plugin_configure( &hostWindow );

                THEN( "the same editor is kept" )
                {
                    REQUIRE( g_state.editor == editor );
                }
            }

            AND_WHEN( "it is opened again from another window, e.g. the plugin dialog" )
            {
                QWidget pluginDialog;
                logsquirl_plugin_configure( &pluginDialog );

                THEN( "the same editor moves over that window, still an open dialog" )
                {
                    REQUIRE( g_state.editor == editor );
                    REQUIRE( editor->parentWidget() == &pluginDialog );
                    REQUIRE( editor->isWindow() );
                    REQUIRE( editor->isVisible() );
                    REQUIRE( editor->isModal() );
                }
            }

            AND_WHEN( "it is closed and opened again before it was deleted" )
            {
                editor->reject();
                logsquirl_plugin_configure( &hostWindow );

                THEN( "a new editor is open" )
                {
                    REQUIRE_FALSE( editor );
                    REQUIRE( g_state.editor );
                    REQUIRE( g_state.editor->isVisible() );
                }
            }

            AND_WHEN( "it is closed" )
            {
                editor->reject();
                QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );

                THEN( "it is deleted" )
                {
                    REQUIRE_FALSE( editor );
                    REQUIRE_FALSE( g_state.editor );
                }
            }
        }

        WHEN( "the host's application-modal plugin dialog is open over the main window" )
        {
            // As in LogSquirl: configure() always gets the main window, even
            // while the Plugins dialog blocks it.
            QWidget mainWindow;
            mainWindow.show();
            auto* pluginDialog = new QDialog( &mainWindow );
            QPointer<QDialog> pluginDialogAlive = pluginDialog;
            pluginDialog->setWindowModality( Qt::ApplicationModal );
            pluginDialog->show();
            REQUIRE( QApplication::activeModalWidget() == pluginDialog );

            logsquirl_plugin_configure( &mainWindow );
            QPointer<FooterEditor> editor = g_state.editor;
            REQUIRE( editor );

            THEN( "the editor opens over the plugin dialog, where it is usable" )
            {
                REQUIRE( editor->parentWidget() == pluginDialog );
                REQUIRE( editor->isWindow() );
                REQUIRE( editor->isVisible() );
            }

            AND_WHEN( "the plugin dialog closes while the editor has unsaved edits" )
            {
                editor->findChild<QLineEdit*>( "keyEdit" )->setText( "Edited" );
                pluginDialog->accept();

                THEN( "the editor moves back over the main window, edits and all" )
                {
                    REQUIRE( editor );
                    REQUIRE( editor->parentWidget() == &mainWindow );
                    REQUIRE( editor->isVisible() );
                    REQUIRE( editor->entries().at( 0 ).key == "Edited" );
                }
            }

            AND_WHEN( "the plugin dialog is deleted while open" )
            {
                delete pluginDialog;

                THEN( "the editor survives, over the main window" )
                {
                    REQUIRE( editor );
                    REQUIRE( editor->parentWidget() == &mainWindow );
                }
            }
            delete pluginDialogAlive.data();
        }

        WHEN( "the editor is open when the plugin dialog opens and opens it again" )
        {
            QWidget mainWindow;
            mainWindow.show();
            logsquirl_plugin_configure( &mainWindow );
            QPointer<FooterEditor> editor = g_state.editor;
            REQUIRE( editor );
            REQUIRE( editor->parentWidget() == &mainWindow );

            QDialog pluginDialog( &mainWindow );
            pluginDialog.setWindowModality( Qt::ApplicationModal );
            pluginDialog.show();
            logsquirl_plugin_configure( &mainWindow );

            THEN( "the same editor moves over the plugin dialog" )
            {
                REQUIRE( g_state.editor == editor );
                REQUIRE( editor->parentWidget() == &pluginDialog );
                REQUIRE( editor->isVisible() );
            }
            pluginDialog.reject();
        }

        WHEN( "the rule editor is open while the host switches files" )
        {
            logsquirl_plugin_configure( nullptr );
            QPointer<FooterEditor> editor = g_state.editor;
            REQUIRE( editor );
            const auto openedWith = editor->activeFile();
            const auto second = ( logDir.path() + "/second.log" ).toUtf8();
            host().activeFileCallback( host().activeFileUserData, second.constData() );

            THEN( "its preview uses the host's active file, and follows it" )
            {
                REQUIRE( openedWith == logDir.path() + "/first.log" );
                REQUIRE( editor->activeFile() == logDir.path() + "/second.log" );
            }
        }

        WHEN( "the plugin is shut down from the event loop while the editor is previewing" )
        {
            logsquirl_plugin_configure( nullptr );
            QPointer<FooterEditor> editor = g_state.editor;
            REQUIRE( editor );
            QPointer<RulePreviewer> previewer = editor->findChild<RulePreviewer*>();
            REQUIRE( previewer );
            REQUIRE( previewer->isBusy() );
            QList<QPointer<QTimer>> timers;
            for ( auto* timer : editor->findChildren<QTimer*>() ) {
                timers.append( timer );
            }
            REQUIRE_FALSE( timers.isEmpty() );

            // As a host would: from its event loop, with no plugin frame on the stack.
            bool shutDown = false;
            QTimer::singleShot( 0, [ &shutDown ] {
                logsquirl_plugin_shutdown();
                shutDown = true;
            } );
            REQUIRE( processUntil( [ &shutDown ] { return shutDown; } ) );

            THEN( "the editor, its previewer and all their timers are gone at once" )
            {
                REQUIRE_FALSE( editor );
                REQUIRE_FALSE( previewer );
                for ( const auto& timer : timers ) {
                    REQUIRE_FALSE( timer );
                }
                REQUIRE_FALSE( g_state.editor );
            }
        }

        WHEN( "the plugin is shut down from the event loop while the import dialog is open" )
        {
            logsquirl_plugin_configure( nullptr );
            QPointer<FooterEditor> editor = g_state.editor;
            REQUIRE( editor );
            auto* import = editor->findChild<QToolButton*>( "importButton" );
            REQUIRE( import );
            import->click();
            QPointer<QDialog> dialog = editor->findChild<QDialog*>( "importDialog" );
            REQUIRE( dialog );
            REQUIRE( dialog->isVisible() );

            bool shutDown = false;
            QTimer::singleShot( 0, [ &shutDown ] {
                logsquirl_plugin_shutdown();
                shutDown = true;
            } );
            REQUIRE( processUntil( [ &shutDown ] { return shutDown; } ) );

            THEN( "the editor and its dialog are gone, and shutdown returned" )
            {
                REQUIRE_FALSE( editor );
                REQUIRE_FALSE( dialog );
                REQUIRE_FALSE( g_state.editor );
            }
        }

        WHEN( "the plugin is shut down from the event loop while the template dialog is open" )
        {
            logsquirl_plugin_configure( nullptr );
            QPointer<FooterEditor> editor = g_state.editor;
            REQUIRE( editor );
            auto* fromTemplate = editor->findChild<QToolButton*>( "templateButton" );
            REQUIRE( fromTemplate );
            fromTemplate->click();
            QPointer<QDialog> dialog = editor->findChild<QDialog*>( "ruleTemplateDialog" );
            REQUIRE( dialog );
            REQUIRE( dialog->isVisible() );

            bool shutDown = false;
            QTimer::singleShot( 0, [ &shutDown ] {
                logsquirl_plugin_shutdown();
                shutDown = true;
            } );
            REQUIRE( processUntil( [ &shutDown ] { return shutDown; } ) );

            THEN( "the editor and the template dialog are gone" )
            {
                REQUIRE_FALSE( editor );
                REQUIRE_FALSE( dialog );
            }
        }

        WHEN( "the plugin is shut down" )
        {
            QWidget* const footer = host().footerWidget;
            logsquirl_plugin_shutdown();

            THEN( "its footer widget is unregistered and deleted" )
            {
                REQUIRE( host().unregisteredWidget == footer );
                REQUIRE_FALSE( host().footerWidget );
            }
        }

        logsquirl_plugin_shutdown();
    }
}
