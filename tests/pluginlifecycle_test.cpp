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
#include "rulepreviewer.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
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
            QWidget* editorParent = nullptr;
            QTimer::singleShot( 0, [ &editorParent ] {
                if ( auto* dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() ) ) {
                    editorParent = dialog->parentWidget();
                    dialog->reject();
                }
            } );
            logsquirl_plugin_configure( &hostWindow );

            THEN( "it is parented to the widget the host passed" )
            {
                REQUIRE( editorParent == &hostWindow );
            }
        }

        WHEN( "the rule editor is open while the host switches files" )
        {
            QString openedWith;
            QString followed;
            QTimer::singleShot( 0, [ &openedWith, &followed, &logDir ] {
                if ( auto* editor
                     = qobject_cast<FooterEditor*>( QApplication::activeModalWidget() ) ) {
                    openedWith = editor->activeFile();
                    const auto second = ( logDir.path() + "/second.log" ).toUtf8();
                    host().activeFileCallback( host().activeFileUserData, second.constData() );
                    followed = editor->activeFile();
                    editor->reject();
                }
            } );
            logsquirl_plugin_configure( nullptr );

            THEN( "its preview uses the host's active file, and follows it" )
            {
                REQUIRE( openedWith == logDir.path() + "/first.log" );
                REQUIRE( followed == logDir.path() + "/second.log" );
            }
        }

        WHEN( "the plugin is shut down while the rule editor is open" )
        {
            QPointer<FooterEditor> openEditor;
            bool busyBefore = false;
            bool busyAfter = true;
            bool visibleAfter = true;
            QTimer::singleShot( 0, [ & ] {
                openEditor = qobject_cast<FooterEditor*>( QApplication::activeModalWidget() );
                auto* previewer = openEditor
                                      ? openEditor->findChild<RulePreviewer*>( "rulePreviewer" )
                                      : nullptr;
                if ( previewer ) {
                    // A preview of the active file is scheduled or running.
                    openEditor->setActiveFile( openEditor->activeFile() );
                    busyBefore = previewer->isBusy();
                    logsquirl_plugin_shutdown();
                    busyAfter = previewer->isBusy();
                    visibleAfter = openEditor->isVisible();
                }
            } );
            logsquirl_plugin_configure( nullptr );

            THEN( "its preview was stopped, and the editor closed and deleted" )
            {
                REQUIRE( busyBefore );
                REQUIRE_FALSE( busyAfter );
                REQUIRE_FALSE( visibleAfter );
                REQUIRE_FALSE( openEditor );
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
