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

#include "basehosttable.h"
#include "eventloopwait.h"
#include "fifo.h"
#include "footerconfig.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"
#include "footerscanner.h"
#include "logsquirl_plugin_api.h"
#include "plugin.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

#include <cstddef>
#include <stdexcept>
#include <thread>

using namespace custom_footer;

namespace {

/// A menu action the plugin registered.
struct MenuItem {
    QString path;
    QString label;
    void ( *callback )( void* ) = nullptr;
    void* userData = nullptr;
};

/// What the fake host was given by the plugin.
struct FakeHost {
    QByteArray configDir;
    QByteArray activeFile;
    QPointer<QWidget> footerWidget;
    QWidget* unregisteredWidget = nullptr;
    void ( *activeFileCallback )( void*, const char* ) = nullptr;
    void* activeFileUserData = nullptr;
    /// The action that opens the rule editor: "Custom Footer…", or "Edit
    /// Rules…" in the Custom Footer submenu.
    void ( *menuAction )( void* ) = nullptr;
    void* menuActionUserData = nullptr;
    /// Every menu action registered, in order.
    QList<MenuItem> menuItems;
    /// The selected Log Lines get_selected_log_lines() hands over, joined
    /// by '\n'; none selected while empty.
    QByteArray selectedLines;
    QStringList warnings; ///< The messages logged as warnings.
    QStringList infos;    ///< The messages logged as information.
    QStringList errors;   ///< The messages logged as errors.
    /// Every call of the plugin, in order: the function's name, and for a
    /// log message its level ("log info", "log warning", "log error").
    QStringList calls;
    /// register_active_file_callback() throws, as a failing host function
    /// would: the plugin's initialisation fails at that point.
    bool failActiveFileCallback = false;
    /// What the last open_regex_lab() got, to answer as the user would.
    QString regexLabPattern;
    int regexLabFlags = 0;
    LogSquirlRegexLabCallbackFn regexLabCallback = nullptr;
    void* regexLabUserData = nullptr;
    /// The lines go_to_log_line() was asked for, in order.
    QList<uint64_t> wentTo;
    /// The encoding get_log_file_encoding() names for every Log File.
    QByteArray encoding = "UTF-8";
    LogSquirlEncodingCallbackFn encodingCallback = nullptr;
    void* encodingUserData = nullptr;
};

FakeHost& host()
{
    static FakeHost instance;
    return instance;
}

QString levelName( int level )
{
    switch ( level ) {
    case LOGSQUIRL_LOG_WARNING:
        return "warning";
    case LOGSQUIRL_LOG_ERROR:
        return "error";
    default:
        return "info";
    }
}

LogSquirlHostApi fakeApi()
{
    LogSquirlHostApi api{};
    api.api_version = LOGSQUIRL_PLUGIN_API_VERSION;
    api.log_message = []( void*, int level, const char* message ) {
        host().calls.append( "log " + levelName( level ) );
        if ( level == LOGSQUIRL_LOG_WARNING ) {
            host().warnings.append( QString::fromUtf8( message ) );
        }
        else if ( level == LOGSQUIRL_LOG_INFO ) {
            host().infos.append( QString::fromUtf8( message ) );
        }
        else if ( level == LOGSQUIRL_LOG_ERROR ) {
            host().errors.append( QString::fromUtf8( message ) );
        }
    };
    api.get_config_dir = []( void* ) {
        host().calls.append( "get_config_dir" );
        return host().configDir.constData();
    };
    api.register_menu_action = []( void*, const char* path, const char* label,
                                   void ( *callback )( void* ), void* userData ) {
        host().calls.append( "register_menu_action" );
        host().menuItems.append(
            { QString::fromUtf8( path ), QString::fromUtf8( label ), callback, userData } );
        const auto name = QString::fromUtf8( label );
        if ( name == "Custom Footer…" || name == "Edit Rules…" ) {
            host().menuAction = callback;
            host().menuActionUserData = userData;
        }
    };
    api.register_footer_widget = []( void*, void* widget ) {
        host().calls.append( "register_footer_widget" );
        host().footerWidget = static_cast<QWidget*>( widget );
    };
    api.unregister_footer_widget = []( void*, void* widget ) {
        host().calls.append( "unregister_footer_widget" );
        host().unregisteredWidget = static_cast<QWidget*>( widget );
    };
    api.get_active_file_path = []( void* ) {
        host().calls.append( "get_active_file_path" );
        return host().activeFile.constData();
    };
    api.register_active_file_callback
        = []( void*, void ( *callback )( void*, const char* ), void* userData ) {
              host().calls.append( "register_active_file_callback" );
              if ( host().failActiveFileCallback ) {
                  throw std::runtime_error( "no callbacks today" );
              }
              host().activeFileCallback = callback;
              host().activeFileUserData = userData;
          };
    // The functions of LogSquirl 26.11, which the plugin reads only when the
    // size the host passes reports them.
    api.open_regex_lab = []( void*, const char* pattern, int flags,
                             LogSquirlRegexLabCallbackFn callback, void* userData ) {
        host().calls.append( "open_regex_lab" );
        host().regexLabPattern = QString::fromUtf8( pattern );
        host().regexLabFlags = flags;
        host().regexLabCallback = callback;
        host().regexLabUserData = userData;
        return 0;
    };
    api.go_to_log_line = []( void*, uint64_t lineNumber ) {
        host().calls.append( "go_to_log_line" );
        host().wentTo.append( lineNumber );
        return static_cast<int>( LOGSQUIRL_LOG_LINES_OK );
    };
    api.get_selected_log_lines = []( void*, const char** text, size_t* length, size_t* ) {
        host().calls.append( "get_selected_log_lines" );
        if ( host().selectedLines.isEmpty() ) {
            *text = nullptr;
            return static_cast<int>( LOGSQUIRL_LOG_LINES_NO_SELECTION );
        }
        *text = host().selectedLines.constData();
        if ( length ) {
            *length = static_cast<size_t>( host().selectedLines.size() );
        }
        return static_cast<int>( LOGSQUIRL_LOG_LINES_OK );
    };
    return api;
}

/// fakeApi() with the functions of LogSquirl 26.12: the encoding of a Log
/// File, and the callback that tells of a change of it.
LogSquirlHostApi fakeApiWithEncoding()
{
    auto api = fakeApi();
    api.get_log_file_encoding = []( void*, const char*, const char** encoding ) {
        host().calls.append( "get_log_file_encoding" );
        *encoding = host().encoding.constData();
        return static_cast<int>( LOGSQUIRL_LOG_LINES_OK );
    };
    api.register_encoding_callback
        = []( void*, LogSquirlEncodingCallbackFn callback, void* userData ) {
              host().calls.append( "register_encoding_callback" );
              host().encodingCallback = callback;
              host().encodingUserData = userData;
          };
    return api;
}

/// fakeApi() with the base function @p name set to NULL, as a broken host's table.
LogSquirlHostApi fakeApiWithout( const QString& name )
{
    auto api = fakeApi();
    if ( name == "get_config_dir" ) {
        api.get_config_dir = nullptr;
    }
    else if ( name == "register_menu_action" ) {
        api.register_menu_action = nullptr;
    }
    else if ( name == "register_footer_widget" ) {
        api.register_footer_widget = nullptr;
    }
    else if ( name == "unregister_footer_widget" ) {
        api.unregister_footer_widget = nullptr;
    }
    else if ( name == "get_active_file_path" ) {
        api.get_active_file_path = nullptr;
    }
    else if ( name == "register_active_file_callback" ) {
        api.register_active_file_callback = nullptr;
    }
    else {
        FAIL( "no such host function: " << name.toStdString() );
    }
    return api;
}

/// Process events until the footer shows a value containing @p text.
bool waitForText( const QString& text, int timeoutMs = 5000 )
{
    return processUntil(
        [ &text ] {
            if ( const auto* footer = qobject_cast<FooterDisplayWidget*>( host().footerWidget ) ) {
                for ( const auto& value : footer->values() ) {
                    if ( value.value.contains( text ) ) {
                        return true;
                    }
                }
            }
            return false;
        },
        timeoutMs );
}

void writeFile( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    file.write( content );
}

/// The open rule editor, found as the user sees it: as an open window of
/// the application. Null if none is open.
FooterEditor* openEditorWindow()
{
    for ( auto* widget : QApplication::topLevelWidgets() ) {
        auto* editor = qobject_cast<FooterEditor*>( widget );
        if ( editor && editor->isVisible() ) {
            return editor;
        }
    }
    return nullptr;
}

/// The editor and everything it owns, to check that they are gone.
QList<QPointer<QObject>> partsOf( FooterEditor& editor )
{
    QList<QPointer<QObject>> parts;
    for ( auto* part : editor.findChildren<QObject*>() ) {
        parts.append( part );
    }
    return parts;
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
            QPointer<FooterEditor> editor = openEditorWindow();

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
                    REQUIRE( openEditorWindow() == editor );
                }
            }

            AND_WHEN( "it is opened again from another window, e.g. the plugin dialog" )
            {
                QWidget pluginDialog;
                logsquirl_plugin_configure( &pluginDialog );

                THEN( "the same editor moves over that window, still an open dialog" )
                {
                    REQUIRE( openEditorWindow() == editor );
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
                    REQUIRE( openEditorWindow() );
                }
            }

            AND_WHEN( "it is closed" )
            {
                editor->reject();
                QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );

                THEN( "it is deleted" )
                {
                    REQUIRE_FALSE( editor );
                    REQUIRE_FALSE( openEditorWindow() );
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
            QPointer<FooterEditor> editor = openEditorWindow();
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
            QPointer<FooterEditor> editor = openEditorWindow();
            REQUIRE( editor );
            REQUIRE( editor->parentWidget() == &mainWindow );

            QDialog pluginDialog( &mainWindow );
            pluginDialog.setWindowModality( Qt::ApplicationModal );
            pluginDialog.show();
            logsquirl_plugin_configure( &mainWindow );

            THEN( "the same editor moves over the plugin dialog" )
            {
                REQUIRE( openEditorWindow() == editor );
                REQUIRE( editor->parentWidget() == &pluginDialog );
                REQUIRE( editor->isVisible() );
            }
            pluginDialog.reject();
        }

        WHEN( "the rule editor is open while the host switches files" )
        {
            logsquirl_plugin_configure( nullptr );
            QPointer<FooterEditor> editor = openEditorWindow();
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
            QPointer<FooterEditor> editor = openEditorWindow();
            REQUIRE( editor );
            REQUIRE( editor->findChild<QWidget*>( "previewUpdating" )->isVisible() );
            const auto parts = partsOf( *editor );

            // As a host would: from its event loop, with no plugin frame on the stack.
            bool shutDown = false;
            QTimer::singleShot( 0, [ &shutDown ] {
                logsquirl_plugin_shutdown();
                shutDown = true;
            } );
            REQUIRE( processUntil( [ &shutDown ] { return shutDown; } ) );

            THEN( "the editor and everything it owns are gone at once" )
            {
                REQUIRE_FALSE( editor );
                for ( const auto& part : parts ) {
                    REQUIRE_FALSE( part );
                }
                REQUIRE_FALSE( openEditorWindow() );
            }
        }

        WHEN( "the plugin is shut down from the event loop while the import dialog is open" )
        {
            logsquirl_plugin_configure( nullptr );
            QPointer<FooterEditor> editor = openEditorWindow();
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
                REQUIRE_FALSE( openEditorWindow() );
            }
        }

        WHEN( "the plugin is shut down from the event loop while the template dialog is open" )
        {
            logsquirl_plugin_configure( nullptr );
            QPointer<FooterEditor> editor = openEditorWindow();
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

        WHEN( "the rule editor is opened from the Plugins menu, and a rule is added and saved" )
        {
            REQUIRE( host().menuAction );
            host().menuAction( host().menuActionUserData );
            QPointer<FooterEditor> editor = openEditorWindow();
            REQUIRE( editor );
            REQUIRE( editor->activeFile() == logDir.path() + "/first.log" );
            editor->appendEntries( { { "Mode", "Mode:\\s+(\\S+)", "", true, {} } } );
            editor->accept();

            THEN( "the rules are saved to the host's config dir" )
            {
                const auto saved = FooterConfig::loadEntries( configDir.path() );
                REQUIRE( saved.size() == 2 );
                REQUIRE( saved.at( 1 ).key == "Mode" );
                REQUIRE( host().warnings.isEmpty() );
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

#if defined( Q_OS_UNIX )

SCENARIO( "The plugin shuts down at once while the active file is not a regular file",
          "[plugin][fifo]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );
    const auto fifo = logDir.path() + "/fifo.log";
    REQUIRE( makeFifo( fifo ) );
    // Declared first, so that it outlives the plugin: a scan or preview
    // opening the FIFO fails the test instead of hanging it.
    FifoWatchdog watchdog( fifo );

    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    host().activeFile = fifo.toUtf8();
    const auto api = fakeApi();
    int handle = 0;

    GIVEN( "an initialised plugin, and its rule editor open, with a FIFO as the active file" )
    {
        REQUIRE( logsquirl_plugin_init( &api, &handle ) == 0 );
        logsquirl_plugin_configure( nullptr );
        QPointer<FooterEditor> editor = openEditorWindow();
        REQUIRE( editor );
        auto* message = editor->findChild<QLabel*>( "previewMessage" );
        REQUIRE( message );

        THEN( "the preview says the file cannot be read, and the footer shows no values" )
        {
            REQUIRE( processUntil(
                [ message ] { return message->text().contains( "cannot be read" ); } ) );
            const auto* footer = qobject_cast<FooterDisplayWidget*>( host().footerWidget );
            REQUIRE( footer );
            REQUIRE( footer->values().isEmpty() );
            REQUIRE_FALSE( watchdog.fired() );
        }

        WHEN( "the plugin is shut down" )
        {
            logsquirl_plugin_shutdown();

            THEN( "it returns at once, and the editor is gone" )
            {
                REQUIRE_FALSE( watchdog.fired() );
                REQUIRE_FALSE( editor );
            }
        }

        logsquirl_plugin_shutdown();
    }
}

#endif

SCENARIO( "The plugin logs what is wrong through the host", "[plugin]" )
{
    host() = FakeHost();
    const auto api = fakeApi();
    int handle = 0;

    GIVEN( "a host that gives no config directory" )
    {
        WHEN( "the plugin is initialised" )
        {
            REQUIRE( logsquirl_plugin_init( &api, &handle ) == 0 );

            THEN( "it warns once that rules are neither loaded nor saved" )
            {
                REQUIRE( host().warnings
                         == QStringList{ "Custom Footer: the host gave no config directory; rules "
                                         "are neither loaded nor saved" } );
            }

            THEN( "before it registers anything" )
            {
                REQUIRE( host().calls
                         == QStringList{ "log info", "get_config_dir", "log warning",
                                         "register_menu_action", "register_footer_widget",
                                         "register_active_file_callback", "get_active_file_path",
                                         "log info" } );
            }
        }
    }

    GIVEN( "a config dir with a rule whose pattern is invalid" )
    {
        QTemporaryDir configDir;
        REQUIRE( configDir.isValid() );
        REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                            { { "Broken", "broken=(", "", true, {} } } ) );
        host().configDir = configDir.path().toUtf8();

        WHEN( "the plugin is initialised" )
        {
            REQUIRE( logsquirl_plugin_init( &api, &handle ) == 0 );

            THEN( "the rule's problem is logged as a warning" )
            {
                REQUIRE( host().warnings.size() == 1 );
                REQUIRE( host().warnings.at( 0 )
                         == "Custom Footer: Rule 1: invalid line pattern: "
                                + FooterScanner::patternError( "broken=(" ) );
            }

            THEN( "when the rules are loaded, after the footer widget is registered" )
            {
                REQUIRE( host().calls
                         == QStringList{ "log info", "get_config_dir", "register_menu_action",
                                         "register_footer_widget", "log warning",
                                         "register_active_file_callback", "get_active_file_path",
                                         "log info" } );
            }
        }
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin's entry points cope with the host's unusual calls", "[plugin]" )
{
    QTemporaryDir configDir;
    REQUIRE( configDir.isValid() );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    const auto api = fakeApi();
    int handle = 0;

    WHEN( "init() is called without the host's table or handle" )
    {
        THEN( "it fails without calling the host" )
        {
            REQUIRE( logsquirl_plugin_init( nullptr, &handle ) == 1 );
            REQUIRE( logsquirl_plugin_init( &api, nullptr ) == 1 );
            REQUIRE( host().calls.isEmpty() );
        }
    }

    WHEN( "shutdown() and configure() are called without init()" )
    {
        logsquirl_plugin_shutdown();
        logsquirl_plugin_configure( nullptr );

        THEN( "nothing happens: the host is not called and no editor opens" )
        {
            REQUIRE( host().calls.isEmpty() );
            REQUIRE_FALSE( openEditorWindow() );
        }
    }

    WHEN( "init() is called twice" )
    {
        REQUIRE( logsquirl_plugin_init( &api, &handle ) == 0 );
        QWidget* const first = host().footerWidget;
        const QPointer<QWidget> firstAlive = first;
        host().calls.clear();
        REQUIRE( logsquirl_plugin_init( &api, &handle ) == 0 );

        THEN( "the first footer widget is unregistered and deleted before the second one is "
              "registered" )
        {
            REQUIRE( host().unregisteredWidget == first );
            REQUIRE( host().footerWidget );
            REQUIRE_FALSE( firstAlive );
            const auto unregistered = host().calls.indexOf( "unregister_footer_widget" );
            REQUIRE( unregistered >= 0 );
            REQUIRE( unregistered < host().calls.indexOf( "register_footer_widget" ) );
        }

        THEN( "one editor opens" )
        {
            logsquirl_plugin_configure( nullptr );
            logsquirl_plugin_configure( nullptr );
            int open = 0;
            for ( auto* widget : QApplication::topLevelWidgets() ) {
                if ( qobject_cast<FooterEditor*>( widget ) && widget->isVisible() ) {
                    ++open;
                }
            }
            REQUIRE( open == 1 );
        }
    }

    WHEN( "init() fails part way, as a host function throws" )
    {
        host().failActiveFileCallback = true;
        const int result = logsquirl_plugin_init( &api, &handle );

        THEN( "it reports the failure, logs it, and takes the footer widget back" )
        {
            REQUIRE( result == 1 );
            REQUIRE( host().calls
                     == QStringList{ "log info", "get_config_dir", "register_menu_action",
                                     "register_footer_widget", "register_active_file_callback",
                                     "log error", "log info", "unregister_footer_widget" } );
            REQUIRE_FALSE( host().footerWidget );
        }

        AND_WHEN( "the host shuts it down all the same, and the menu action is clicked" )
        {
            host().calls.clear();
            logsquirl_plugin_shutdown();
            host().menuAction( host().menuActionUserData );

            THEN( "nothing happens" )
            {
                REQUIRE( host().calls.isEmpty() );
                REQUIRE_FALSE( openEditorWindow() );
            }
        }
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin refuses a host table with a missing function", "[plugin]" )
{
    QTemporaryDir configDir;
    REQUIRE( configDir.isValid() );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    int handle = 0;
    const QString missing = GENERATE( as<QString>{}, "get_config_dir", "register_menu_action",
                                      "register_footer_widget", "unregister_footer_widget",
                                      "get_active_file_path", "register_active_file_callback" );
    const auto api = fakeApiWithout( missing );

    WHEN( "init() is called with a table whose " + missing.toStdString() + " is NULL" )
    {
        const int result = logsquirl_plugin_init( &api, &handle );

        THEN( "it fails, logging which function is missing, and registers nothing" )
        {
            REQUIRE( result == 1 );
            REQUIRE( host().errors
                     == QStringList{ "Custom Footer: the host's table has no " + missing
                                     + "; the plugin is not initialised" } );
            for ( const auto& call : host().calls ) {
                REQUIRE( call.startsWith( "log " ) );
            }
            REQUIRE_FALSE( host().footerWidget );
            REQUIRE( host().menuItems.isEmpty() );
            REQUIRE_FALSE( host().activeFileCallback );
        }

        AND_WHEN( "the host shuts it down all the same, and calls configure()" )
        {
            host().calls.clear();
            logsquirl_plugin_shutdown();
            logsquirl_plugin_configure( nullptr );

            THEN( "nothing happens" )
            {
                REQUIRE( host().calls.isEmpty() );
                REQUIRE_FALSE( openEditorWindow() );
            }
        }
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin ignores an active-file callback off the UI thread", "[plugin]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );
    writeFile( logDir.path() + "/first.log", "VIN: FIRST\n" );
    writeFile( logDir.path() + "/second.log", "VIN: SECOND\n" );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    host().activeFile = ( logDir.path() + "/first.log" ).toUtf8();
    const auto api = fakeApi();
    int handle = 0;

    GIVEN( "an initialised plugin showing the first file, with the rule editor open" )
    {
        REQUIRE( logsquirl_plugin_init( &api, &handle ) == 0 );
        REQUIRE( waitForText( "FIRST" ) );
        logsquirl_plugin_configure( nullptr );
        QPointer<FooterEditor> editor = openEditorWindow();
        REQUIRE( editor );

        WHEN( "the host calls the active-file callback from a worker thread" )
        {
            const auto second = ( logDir.path() + "/second.log" ).toUtf8();
            std::thread worker( [ &second ] {
                host().activeFileCallback( host().activeFileUserData, second.constData() );
            } );
            worker.join();

            THEN( "it is logged as an error and ignored: the session keeps the first file" )
            {
                REQUIRE( host().errors
                         == QStringList{ "Custom Footer: the host reported the active file off "
                                         "the UI thread; ignored" } );
                REQUIRE( editor );
                REQUIRE( editor->activeFile() == logDir.path() + "/first.log" );
                REQUIRE_FALSE( waitForText( "SECOND", 200 ) );
                REQUIRE( waitForText( "FIRST" ) );
            }
        }
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin ignores a menu action off the UI thread", "[plugin]" )
{
    QTemporaryDir configDir;
    REQUIRE( configDir.isValid() );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    host().selectedLines = "VIN: WDB1234567";
    const auto api = fakeApi();
    int handle = 0;
    REQUIRE( logsquirl_plugin_init_ex( &api, &handle, sizeof( LogSquirlHostApi ) ) == 0 );
    REQUIRE( host().menuItems.size() == 2 );
    const auto item = GENERATE( 0, 1 );
    const auto menuItem = host().menuItems.at( item );

    GIVEN( "the menu item " + menuItem.label.toStdString() )
    {
        WHEN( "the host calls it from a worker thread" )
        {
            host().calls.clear();
            std::thread worker( [ &menuItem ] { menuItem.callback( menuItem.userData ); } );
            worker.join();
            QCoreApplication::processEvents();

            THEN( "it is logged as an error and ignored: no editor opens, the host is not asked "
                  "for the selected line" )
            {
                REQUIRE( host().errors
                         == QStringList{ "Custom Footer: the host called a menu action off the "
                                         "UI thread; ignored" } );
                REQUIRE_FALSE( host().calls.contains( "get_selected_log_lines" ) );
                REQUIRE_FALSE( openEditorWindow() );
            }
        }
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin learns from the host's table which host functions it offers", "[plugin]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );
    writeFile( logDir.path() + "/vin.log", "VIN: OLDHOST\n" );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    host().activeFile = ( logDir.path() + "/vin.log" ).toUtf8();
    const auto api = fakeApi();
    int handle = 0;
    const QString all = "Custom Footer plugin ready. Host functions: Regex Lab, Go to line, "
                        "selected Log Lines.";
    const QString none = "Custom Footer plugin ready. Host functions: none.";

    WHEN( "LogSquirl 26.11 loads it through init_ex(), with its whole table" )
    {
        REQUIRE( logsquirl_plugin_init_ex( &api, &handle, sizeof( LogSquirlHostApi ) ) == 0 );

        THEN( "the session reports all three host functions, and none is called yet" )
        {
            REQUIRE( host().infos.contains( all ) );
            REQUIRE_FALSE( host().calls.contains( "open_regex_lab" ) );
            REQUIRE_FALSE( host().calls.contains( "go_to_log_line" ) );
            REQUIRE_FALSE( host().calls.contains( "get_selected_log_lines" ) );
        }

        THEN( "it registers Edit Rules… and Rule from Selected Line… in a Custom Footer "
              "submenu instead of Custom Footer…, once the session knows the host offers the "
              "selected Log Lines" )
        {
            REQUIRE( host().menuItems.size() == 2 );
            REQUIRE( host().menuItems.at( 0 ).path == "Custom Footer" );
            REQUIRE( host().menuItems.at( 0 ).label == "Edit Rules…" );
            REQUIRE( host().menuItems.at( 1 ).path == "Custom Footer" );
            REQUIRE( host().menuItems.at( 1 ).label == "Rule from Selected Line…" );
            REQUIRE( host().calls
                     == QStringList{ "log info", "get_config_dir", "register_footer_widget",
                                     "register_menu_action", "register_menu_action",
                                     "register_active_file_callback", "get_active_file_path",
                                     "log info" } );
        }

        AND_WHEN( "Edit Rules… is clicked" )
        {
            host().menuItems.at( 0 ).callback( host().menuItems.at( 0 ).userData );

            THEN( "the rule editor opens" )
            {
                REQUIRE( openEditorWindow() );
            }
        }
    }

    WHEN( "a host passes init_ex() a table that ends before the selected Log Lines" )
    {
        REQUIRE( logsquirl_plugin_init_ex( &api, &handle,
                                           offsetof( LogSquirlHostApi, get_selected_log_lines ) )
                 == 0 );

        THEN( "the session reports the Regex Lab and Go to line only" )
        {
            REQUIRE( host().infos.contains(
                "Custom Footer plugin ready. Host functions: Regex Lab, Go to line." ) );
        }

        THEN( "it does not offer Rule from Selected Line…" )
        {
            REQUIRE( host().menuItems.size() == 1 );
            REQUIRE( host().menuItems.at( 0 ).label == "Custom Footer…" );
        }
    }

    WHEN( "LogSquirl 26.10 loads it through init(), with the base table" )
    {
        BaseHostTable base( api );
        REQUIRE( logsquirl_plugin_init( base.api(), &handle ) == 0 );

        THEN( "the session reports none, nothing reads past the table, and the footer works" )
        {
            REQUIRE( host().infos.contains( none ) );
            REQUIRE( host().footerWidget );
            REQUIRE( waitForText( "OLDHOST" ) );
        }

        THEN( "it registers only the Custom Footer… action, as 0.4.0 did" )
        {
            REQUIRE( host().menuItems.size() == 1 );
            REQUIRE( host().menuItems.at( 0 ).path == "Plugins" );
            REQUIRE( host().menuItems.at( 0 ).label == "Custom Footer…" );
        }

        logsquirl_plugin_shutdown();
    }

    WHEN( "a host passes init_ex() the base table" )
    {
        BaseHostTable base( api );
        REQUIRE( logsquirl_plugin_init_ex( base.api(), &handle, LOGSQUIRL_HOST_API_BASE_SIZE )
                 == 0 );

        THEN( "the session reports none, nothing reads past the table, and only the Custom "
              "Footer… action is registered" )
        {
            REQUIRE( host().infos.contains( none ) );
            REQUIRE( waitForText( "OLDHOST" ) );
            REQUIRE( host().menuItems.size() == 1 );
        }

        logsquirl_plugin_shutdown();
    }

    WHEN( "a host passes init_ex() a table smaller than the base table" )
    {
        BaseHostTable base( api );
        const int result
            = logsquirl_plugin_init_ex( base.api(), &handle, LOGSQUIRL_HOST_API_BASE_SIZE - 1 );

        THEN( "it fails without calling the host" )
        {
            REQUIRE( result == 1 );
            REQUIRE( host().calls.isEmpty() );
        }
    }

    WHEN( "init_ex() is called without the host's table or handle" )
    {
        const int withoutTable
            = logsquirl_plugin_init_ex( nullptr, &handle, sizeof( LogSquirlHostApi ) );
        const int withoutHandle
            = logsquirl_plugin_init_ex( &api, nullptr, sizeof( LogSquirlHostApi ) );

        THEN( "it fails without calling the host" )
        {
            REQUIRE( withoutTable == 1 );
            REQUIRE( withoutHandle == 1 );
            REQUIRE( host().calls.isEmpty() );
        }
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin tests a rule's pattern in the host's Regex Lab", "[plugin][regexlab]" )
{
    QTemporaryDir configDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    const auto api = fakeApi();
    int handle = 0;

    GIVEN( "the plugin loaded by LogSquirl 26.11, with the rule editor open" )
    {
        REQUIRE( logsquirl_plugin_init_ex( &api, &handle, sizeof( LogSquirlHostApi ) ) == 0 );
        logsquirl_plugin_configure( nullptr );
        QPointer<FooterEditor> editor = openEditorWindow();
        REQUIRE( editor );
        auto* test = editor->findChild<QToolButton*>( "testLinePatternButton" );
        REQUIRE( test );

        WHEN( "Test… is clicked" )
        {
            test->click();

            THEN( "the host opens its Regex Lab with the rule's pattern, matching case" )
            {
                REQUIRE( host().calls.contains( "open_regex_lab" ) );
                REQUIRE( host().regexLabPattern == "VIN:\\s+(\\S+)" );
                REQUIRE( host().regexLabFlags == LOGSQUIRL_REGEX_LAB_MATCH_CASE );
            }

            AND_WHEN( "the user applies a pattern without Match case" )
            {
                host().regexLabCallback( host().regexLabUserData, LOGSQUIRL_REGEX_LAB_APPLIED,
                                         "vin=(\\S+)", 0 );

                THEN( "the rule has it, ignoring case" )
                {
                    REQUIRE( editor->entries().at( 0 ).linePattern == "(?i)vin=(\\S+)" );
                    REQUIRE( editor->findChild<QLineEdit*>( "linePatternEdit" )->text()
                             == "(?i)vin=(\\S+)" );
                }
            }

            AND_WHEN( "the user cancels" )
            {
                host().regexLabCallback( host().regexLabUserData, LOGSQUIRL_REGEX_LAB_CANCELLED,
                                         nullptr, 0 );

                THEN( "the rule is unchanged, and Test… is offered again" )
                {
                    REQUIRE( editor->entries().at( 0 ).linePattern == "VIN:\\s+(\\S+)" );
                    REQUIRE( test->isEnabled() );
                }
            }

            AND_WHEN( "the plugin is shut down while the Lab is open, and an answer comes late" )
            {
                logsquirl_plugin_shutdown();
                host().regexLabCallback( host().regexLabUserData, LOGSQUIRL_REGEX_LAB_APPLIED,
                                         "vin=(\\S+)", 0 );

                THEN( "the editor is gone, and nothing is saved" )
                {
                    REQUIRE_FALSE( editor );
                    REQUIRE( FooterConfig::loadEntries( configDir.path() ).at( 0 ).linePattern
                             == "VIN:\\s+(\\S+)" );
                }
            }
        }
    }

    GIVEN( "the plugin loaded by LogSquirl 26.10, with the rule editor open" )
    {
        BaseHostTable base( api );
        REQUIRE( logsquirl_plugin_init( base.api(), &handle ) == 0 );
        logsquirl_plugin_configure( nullptr );
        QPointer<FooterEditor> editor = openEditorWindow();
        REQUIRE( editor );
        auto* test = editor->findChild<QToolButton*>( "testLinePatternButton" );
        REQUIRE( test );

        THEN( "Test… is disabled, saying what it needs" )
        {
            REQUIRE_FALSE( test->isEnabled() );
            REQUIRE( test->toolTip() == "Needs LogSquirl 26.11 or later" );
        }

        logsquirl_plugin_shutdown();
    }

    logsquirl_plugin_shutdown();
}

namespace {

/// The "Go to Log Line" entry of the context menu of the footer's value of
/// @p key, if it is offered; null otherwise.
QAction* offeredGoToLine( const QString& key )
{
    auto* footer = qobject_cast<FooterDisplayWidget*>( host().footerWidget );
    if ( !footer ) {
        return nullptr;
    }
    for ( const auto* item : footer->findChildren<FooterValueItem*>() ) {
        if ( item->value().key != key ) {
            continue;
        }
        for ( auto* action : item->contextMenu()->actions() ) {
            if ( action->objectName() == QLatin1String( "goToLogLine" ) && action->isVisible() ) {
                return action;
            }
        }
    }
    return nullptr;
}

} // namespace

SCENARIO( "The plugin takes the user to the Log Line a footer value came from", "[plugin]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );
    writeFile( logDir.path() + "/vin.log", "starting\nnoise\nVIN: JUMP\n" );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    host().activeFile = ( logDir.path() + "/vin.log" ).toUtf8();
    const auto api = fakeApi();
    int handle = 0;

    WHEN( "LogSquirl 26.11 loads it, and the user goes to the Log Line of a value" )
    {
        REQUIRE( logsquirl_plugin_init_ex( &api, &handle, sizeof( LogSquirlHostApi ) ) == 0 );
        REQUIRE( waitForText( "JUMP" ) );
        auto* goTo = offeredGoToLine( "VIN" );
        REQUIRE( goTo );
        goTo->trigger();

        THEN( "the host is asked to go to the line it was found in, counting from 1" )
        {
            REQUIRE( host().wentTo == QList<uint64_t>{ 3 } );
        }
    }

    WHEN( "LogSquirl 26.10 loads it, with the base table" )
    {
        BaseHostTable base( api );
        REQUIRE( logsquirl_plugin_init( base.api(), &handle ) == 0 );
        REQUIRE( waitForText( "JUMP" ) );

        THEN( "the values do not offer it" )
        {
            REQUIRE( offeredGoToLine( "VIN" ) == nullptr );
        }

        logsquirl_plugin_shutdown();
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin makes a rule from the Log Line selected in LogSquirl 26.11",
          "[plugin][rulefromline]" )
{
    QTemporaryDir configDir;
    REQUIRE( configDir.isValid() );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    const auto api = fakeApi();
    int handle = 0;
    REQUIRE( logsquirl_plugin_init_ex( &api, &handle, sizeof( LogSquirlHostApi ) ) == 0 );
    REQUIRE( host().menuItems.size() == 2 );
    const auto ruleFromLine = host().menuItems.at( 1 );

    GIVEN( "two Log Lines selected in the host" )
    {
        host().selectedLines = "VIN: WDB1234567\nsecond line";

        WHEN( "Rule from Selected Line… is clicked" )
        {
            host().calls.clear();
            ruleFromLine.callback( ruleFromLine.userData );
            QPointer<FooterEditor> editor = openEditorWindow();

            THEN( "the host's selection is read then, and the editor opens with the first line "
                  "for the new rule" )
            {
                REQUIRE( host().calls.contains( "get_selected_log_lines" ) );
                REQUIRE( editor );
                auto* line = editor->findChild<QLineEdit*>( "selectedLineEdit" );
                REQUIRE( line );
                REQUIRE( line->text() == "VIN: WDB1234567" );
                REQUIRE( line->isVisibleTo( editor.data() ) );
            }

            AND_WHEN( "the value is marked and the editor is accepted" )
            {
                auto* line = editor->findChild<QLineEdit*>( "selectedLineEdit" );
                REQUIRE( line );
                line->setSelection( 5, 10 );
                editor->accept();

                THEN( "the new rule is saved to the host's config dir" )
                {
                    const auto saved = FooterConfig::loadEntries( configDir.path() );
                    REQUIRE( saved.size() == 1 );
                    REQUIRE( saved.at( 0 ).key == "VIN" );
                    REQUIRE( saved.at( 0 ).linePattern == "VIN:\\s*(\\S+)" );
                }
            }
        }
    }

    GIVEN( "no Log Line selected in the host" )
    {
        WHEN( "Rule from Selected Line… is clicked" )
        {
            ruleFromLine.callback( ruleFromLine.userData );

            THEN( "no editor opens, and a message, opened without blocking, says what to do" )
            {
                REQUIRE_FALSE( openEditorWindow() );
                QPointer<QWidget> message;
                for ( auto* widget : QApplication::topLevelWidgets() ) {
                    if ( widget->objectName() == "noSelectedLineBox" && widget->isVisible() ) {
                        message = widget;
                    }
                }
                REQUIRE( message );

                AND_THEN( "the plugin's shutdown takes it away" )
                {
                    logsquirl_plugin_shutdown();
                    REQUIRE_FALSE( message );
                }
            }
        }
    }

    logsquirl_plugin_shutdown();
}

SCENARIO( "The plugin reads the active file in the encoding LogSquirl 26.12 reports",
          "[plugin][hostencoding]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "VIN", "VIN:\\s+(\\S+)", "", true, {} } } ) );
    // windows-1252 "VIN: GRÜN", no UTF-8.
    const auto path = logDir.path() + "/latin.log";
    writeFile( path, "VIN: GR\xDCN\n" );
    host() = FakeHost();
    host().configDir = configDir.path().toUtf8();
    host().activeFile = path.toUtf8();
    host().encoding = "windows-1252";
    const auto api = fakeApiWithEncoding();
    int handle = 0;

    WHEN( "LogSquirl 26.12 loads it through init_ex(), with its whole table" )
    {
        REQUIRE( logsquirl_plugin_init_ex( &api, &handle, sizeof( LogSquirlHostApi ) ) == 0 );

        THEN( "it registers for changes of the encoding, and asks for the active file's" )
        {
            REQUIRE( host().encodingCallback );
            REQUIRE( host().calls.contains( "get_log_file_encoding" ) );
            REQUIRE( host().infos.contains(
                "Custom Footer plugin ready. Host functions: Regex Lab, Go to line, "
                "selected Log Lines, Log File encoding." ) );
        }

        THEN( "the footer decodes the file in that encoding" )
        {
            REQUIRE( waitForText( QStringLiteral( "GRÜN" ) ) );
        }

        AND_WHEN( "the user chooses UTF-8 for the tab" )
        {
            REQUIRE( waitForText( QStringLiteral( "GRÜN" ) ) );
            const auto file = path.toUtf8();
            host().encoding = "UTF-8";
            host().encodingCallback( host().encodingUserData, file.constData(), "UTF-8" );

            THEN( "the file is scanned again, in UTF-8" )
            {
                REQUIRE( waitForText( QString::fromUtf8( "GR\xEF\xBF\xBDN" ) ) );
            }
        }

        AND_WHEN( "the host reports the change from a worker thread" )
        {
            REQUIRE( waitForText( QStringLiteral( "GRÜN" ) ) );
            const auto file = path.toUtf8();
            host().encoding = "UTF-8";
            host().calls.clear();
            std::thread worker( [ &file ] {
                host().encodingCallback( host().encodingUserData, file.constData(), "UTF-8" );
            } );
            worker.join();

            THEN( "it is logged as an error and ignored: the file is not scanned again" )
            {
                REQUIRE( host().errors
                         == QStringList{ "Custom Footer: the host reported an encoding off the "
                                         "UI thread; ignored" } );
                REQUIRE_FALSE( host().calls.contains( "get_log_file_encoding" ) );
                REQUIRE_FALSE( waitForText( QString::fromUtf8( "GR\xEF\xBF\xBDN" ), 200 ) );
                REQUIRE( waitForText( QStringLiteral( "GRÜN" ) ) );
            }
        }

        logsquirl_plugin_shutdown();
    }

    WHEN( "a 26.11 table, which ends before the encoding, is passed" )
    {
        REQUIRE( logsquirl_plugin_init_ex( &api, &handle,
                                           offsetof( LogSquirlHostApi, get_log_file_encoding ) )
                 == 0 );

        THEN( "it neither asks for the encoding nor registers for its changes" )
        {
            REQUIRE_FALSE( host().calls.contains( "get_log_file_encoding" ) );
            REQUIRE_FALSE( host().calls.contains( "register_encoding_callback" ) );
        }

        logsquirl_plugin_shutdown();
    }
}
