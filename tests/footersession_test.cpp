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
 * @file footersession_test.cpp
 * @brief BDD tests for the footer session: one loaded plugin's rules, active
 *        file, footer and rule editor, driven directly with a temp config dir.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"
#include "footersession.h"
#include "hostfunctions.h"
#include "logsquirl_plugin_api.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <memory>

using namespace custom_footer;

namespace {

/// Process events until @p done, for at most a generous bound: a hung
/// worker fails the test instead of hanging it. Nothing depends on timing.
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
        QThread::msleep( 1 );
    }
    return true;
}

void writeFile( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

FooterEntry rule( const QString& key )
{
    return { key, key + ":\\s+(\\S+)", "", true, {} };
}

/// The footer's values as "key=value", in the order shown.
QStringList shown( const FooterDisplayWidget& footer )
{
    QStringList values;
    for ( const auto& value : footer.values() ) {
        values.append( value.key + "=" + value.value );
    }
    return values;
}

template <typename Widget>
Widget* child( QObject& parent, const char* name )
{
    auto* widget = parent.findChild<Widget*>( name );
    REQUIRE( widget );
    return widget;
}

/// Click the editor's Apply button, as the user does.
void clickApply( FooterEditor& editor )
{
    auto* buttons = editor.findChild<QDialogButtonBox*>();
    REQUIRE( buttons );
    buttons->button( QDialogButtonBox::Apply )->click();
}

/// What a session logged through its host functions.
struct Log {
    QStringList warnings;
    QStringList all;

    HostFunctions host()
    {
        HostFunctions functions;
        functions.log = [ this ]( int level, const QString& message ) {
            all.append( message );
            if ( level == LOGSQUIRL_LOG_WARNING ) {
                warnings.append( message );
            }
        };
        return functions;
    }
};

} // namespace

SCENARIO( "A footer session shows the values of the active file", "[footersession]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), { rule( "VIN" ) } ) );
    const auto first = logDir.path() + "/first.log";
    const auto second = logDir.path() + "/second.log";
    writeFile( first, "VIN: FIRST\n" );
    writeFile( second, "VIN: SECOND\n" );

    FooterDisplayWidget footer;
    Log log;

    GIVEN( "a session on the config dir and the footer" )
    {
        FooterSession session( configDir.path(), &footer, log.host() );

        THEN( "nothing is shown without an active file, and nothing is wrong" )
        {
            REQUIRE( footer.values().isEmpty() );
            REQUIRE( log.warnings.isEmpty() );
        }

        WHEN( "it is given the active file" )
        {
            session.setActiveFile( first );

            THEN( "the file's values are shown, with the rules from the config dir" )
            {
                REQUIRE( processUntil(
                    [ & ] { return shown( footer ) == QStringList{ "VIN=FIRST" }; } ) );
            }

            AND_WHEN( "it is given another file" )
            {
                REQUIRE( processUntil( [ & ] { return !footer.values().isEmpty(); } ) );
                session.setActiveFile( second );

                THEN( "that file's values are shown" )
                {
                    REQUIRE( processUntil(
                        [ & ] { return shown( footer ) == QStringList{ "VIN=SECOND" }; } ) );
                }
            }

            AND_WHEN( "it is told there is no active file" )
            {
                REQUIRE( processUntil( [ & ] { return !footer.values().isEmpty(); } ) );
                session.setActiveFile( {} );

                THEN( "the footer is cleared" )
                {
                    REQUIRE( footer.values().isEmpty() );
                }
            }
        }
    }

    GIVEN( "a rule with an invalid pattern in the config dir" )
    {
        REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                            { { "Broken", "broken=(", "", true, {} } } ) );

        WHEN( "a session is made" )
        {
            FooterSession session( configDir.path(), &footer, log.host() );

            THEN( "the problem is logged as a warning through the host functions" )
            {
                REQUIRE( log.warnings.size() == 1 );
                REQUIRE( log.warnings.at( 0 ).startsWith( "Custom Footer: " ) );
                REQUIRE( log.warnings.at( 0 ).contains( "Broken" ) );
            }
        }

        WHEN( "a session is made without a log function, as for a host that logs nothing" )
        {
            FooterSession session( configDir.path(), &footer );

            THEN( "it still runs" )
            {
                session.setActiveFile( first );
                REQUIRE( footer.values().isEmpty() );
            }
        }
    }

    GIVEN( "no config dir" )
    {
        WHEN( "a session is made" )
        {
            FooterSession session( {}, &footer, log.host() );

            THEN( "it runs without rules, and leaves the warning to the C entry points" )
            {
                session.setActiveFile( first );
                REQUIRE( footer.values().isEmpty() );
                REQUIRE( log.warnings.isEmpty() );
            }
        }
    }
}

SCENARIO( "A footer session opens the rule editor with its rules and active file",
          "[footersession][footereditor]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), { rule( "VIN" ) } ) );
    REQUIRE( FooterConfig::saveMaxLines( configDir.path(), 3 ) );
    const auto first = logDir.path() + "/first.log";
    const auto second = logDir.path() + "/second.log";
    writeFile( first, "a\nb\nc\nd\nVIN: FIRST\nMode: M1\n" );
    writeFile( second, "VIN: SECOND\nMode: M2\n" );

    FooterDisplayWidget footer;
    Log log;
    FooterSession session( configDir.path(), &footer, log.host() );
    session.setActiveFile( second );

    GIVEN( "the editor, opened over a window" )
    {
        QWidget window;
        QPointer<FooterEditor> editor = session.openEditor( &window );
        REQUIRE( editor );

        THEN( "it is open without blocking, over that window, with the rules of the config dir" )
        {
            REQUIRE( editor->isVisible() );
            REQUIRE( editor->parentWidget() == &window );
            REQUIRE( editor->entries().size() == 1 );
            REQUIRE( editor->entries().at( 0 ).key == "VIN" );
        }

        THEN( "it previews the footer's active file" )
        {
            REQUIRE( editor->activeFile() == second );
        }

        WHEN( "the active file changes" )
        {
            session.setActiveFile( first );

            THEN( "the editor follows it, with the footer's line limit" )
            {
                REQUIRE( editor->activeFile() == first );
                auto* count = child<QLabel>( *editor, "previewCount" );
                REQUIRE( processUntil( [ count ] {
                    return count->text()
                           == "0 matching lines in the first 3 lines: the scan stops at this line "
                              "limit.";
                } ) );
            }
        }

        WHEN( "it is opened again" )
        {
            auto* again = session.openEditor( &window );

            THEN( "the same editor is raised" )
            {
                REQUIRE( again == editor );
                REQUIRE( editor->isVisible() );
            }
        }

        WHEN( "it is opened again from another window" )
        {
            QWidget other;
            auto* again = session.openEditor( &other );

            THEN( "the same editor moves over that window, still an open dialog" )
            {
                REQUIRE( again == editor );
                REQUIRE( editor->parentWidget() == &other );
                REQUIRE( editor->isWindow() );
                REQUIRE( editor->isVisible() );
            }
        }

        WHEN( "a rule is added and Apply is clicked" )
        {
            editor->appendEntries( { rule( "Mode" ) } );
            clickApply( *editor );

            THEN( "the rules are saved, the footer shows them, and the editor stays open" )
            {
                REQUIRE( FooterConfig::loadEntries( configDir.path() ).size() == 2 );
                REQUIRE( processUntil(
                    [ & ] { return shown( footer ) == QStringList{ "VIN=SECOND", "Mode=M2" }; } ) );
                REQUIRE( editor );
                REQUIRE( editor->isVisible() );
                REQUIRE( log.warnings.isEmpty() );
            }
        }

        WHEN( "a rule is added and the editor is accepted" )
        {
            editor->appendEntries( { rule( "Mode" ) } );
            editor->accept();

            THEN( "the rules are saved, the footer shows them, and the editor is deleted" )
            {
                REQUIRE( FooterConfig::loadEntries( configDir.path() ).size() == 2 );
                REQUIRE( processUntil(
                    [ & ] { return shown( footer ) == QStringList{ "VIN=SECOND", "Mode=M2" }; } ) );
                QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );
                REQUIRE_FALSE( editor );
            }

            AND_WHEN( "the editor is opened again before the closed one was deleted" )
            {
                QPointer<FooterEditor> reopened = session.openEditor( &window );

                THEN( "a new one is open, with the saved rules and the active file" )
                {
                    REQUIRE_FALSE( editor );
                    REQUIRE( reopened );
                    REQUIRE( reopened->isVisible() );
                    REQUIRE( reopened->entries().size() == 2 );
                    REQUIRE( reopened->activeFile() == second );
                }
            }
        }

        WHEN( "a rule is added and the editor is cancelled" )
        {
            editor->appendEntries( { rule( "Mode" ) } );
            editor->reject();
            QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );

            THEN( "nothing is saved, and the editor is deleted" )
            {
                REQUIRE( FooterConfig::loadEntries( configDir.path() ).size() == 1 );
                REQUIRE_FALSE( editor );
            }
        }
    }
}

SCENARIO( "A footer session without a config dir saves nothing", "[footersession]" )
{
    FooterDisplayWidget footer;
    Log log;
    FooterSession session( {}, &footer, log.host() );

    GIVEN( "the open editor with a new rule" )
    {
        QPointer<FooterEditor> editor = session.openEditor( nullptr );
        REQUIRE( editor );
        editor->appendEntries( { rule( "VIN" ) } );

        WHEN( "Apply is clicked" )
        {
            clickApply( *editor );

            THEN( "a warning says the rules could not be saved, and the editor stays open" )
            {
                REQUIRE( log.warnings
                         == QStringList{ "Custom Footer: could not save the rules to the config "
                                         "directory" } );
                REQUIRE( editor->isVisible() );
            }
        }
    }
}

SCENARIO( "Destroying a footer session leaves nothing of the plugin running",
          "[footersession][lifetime]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    // A rule that matches only the last line, and no line limit to speak
    // of, so that the footer's scan and the editor's preview read the whole
    // file, and a finished scan changes the footer.
    REQUIRE( FooterConfig::saveEntries( configDir.path(), { rule( "Late" ) } ) );
    REQUIRE( FooterConfig::saveMaxLines( configDir.path(), 10'000'000 ) );
    const auto big = logDir.path() + "/big.log";
    writeFile( big, QByteArray( "a line of a log that no rule matches\n" ).repeated( 300'000 )
                        + "Late: yes\n" );

    FooterDisplayWidget footer;
    Log log;
    auto session = std::make_unique<FooterSession>( configDir.path(), &footer, log.host() );

    const auto destroyFromEventLoop = [ &session ] {
        // As the host does: from its event loop, with no plugin frame on the stack.
        bool destroyed = false;
        QTimer::singleShot( 0, [ &session, &destroyed ] {
            session.reset();
            destroyed = true;
        } );
        REQUIRE( processUntil( [ &destroyed ] { return destroyed; } ) );
    };
    const auto runPendingWork = [] {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    };

    GIVEN( "the editor open while its preview and the footer's scan run" )
    {
        session->setActiveFile( big );
        QPointer<FooterEditor> editor = session->openEditor( nullptr );
        REQUIRE( editor );
        auto* updating = child<QLabel>( *editor, "previewUpdating" );
        REQUIRE( updating->isVisible() );
        // Let the preview's worker start.
        QCoreApplication::processEvents();
        QList<QPointer<QObject>> parts;
        for ( auto* part : editor->findChildren<QObject*>() ) {
            parts.append( part );
        }

        WHEN( "the session is destroyed" )
        {
            destroyFromEventLoop();

            THEN( "the editor and everything it owns are gone at once" )
            {
                REQUIRE_FALSE( editor );
                for ( const auto& part : parts ) {
                    REQUIRE_FALSE( part );
                }
            }
        }
    }

    GIVEN( "the footer's scan of a big file running" )
    {
        // How long a whole scan of the file takes here, with another session
        // on another footer, which also shows that a finished scan changes
        // the footer.
        qint64 scanMs = 0;
        {
            FooterDisplayWidget otherFooter;
            FooterSession other( configDir.path(), &otherFooter );
            QElapsedTimer scan;
            scan.start();
            other.setActiveFile( big );
            REQUIRE( processUntil(
                [ & ] { return shown( otherFooter ) == QStringList{ "Late=yes" }; } ) );
            scanMs = scan.elapsed();
        }

        QElapsedTimer sinceStart;
        sinceStart.start();
        session->setActiveFile( big );

        WHEN( "the session is destroyed before the scan could finish" )
        {
            REQUIRE( footer.values().isEmpty() );
            session.reset();
            REQUIRE( sinceStart.elapsed() < scanMs );

            THEN( "the scan's result never reaches the footer" )
            {
                runPendingWork();
                REQUIRE_FALSE( processUntil( [ & ] { return !footer.values().isEmpty(); },
                                             int( 3 * scanMs + 200 ) ) );
            }
        }
    }

    GIVEN( "the editor with its import dialog open" )
    {
        QPointer<FooterEditor> editor = session->openEditor( nullptr );
        REQUIRE( editor );
        child<QToolButton>( *editor, "importButton" )->click();
        QPointer<QDialog> dialog = child<QDialog>( *editor, "importDialog" );
        REQUIRE( dialog->isVisible() );

        WHEN( "the session is destroyed" )
        {
            destroyFromEventLoop();

            THEN( "the editor and the dialog are gone" )
            {
                REQUIRE_FALSE( editor );
                REQUIRE_FALSE( dialog );
            }
        }
    }

    GIVEN( "the editor with its template dialog open" )
    {
        QPointer<FooterEditor> editor = session->openEditor( nullptr );
        REQUIRE( editor );
        child<QToolButton>( *editor, "templateButton" )->click();
        QPointer<QDialog> dialog = child<QDialog>( *editor, "ruleTemplateDialog" );
        REQUIRE( dialog->isVisible() );

        WHEN( "the session is destroyed" )
        {
            destroyFromEventLoop();

            THEN( "the editor and the template dialog are gone" )
            {
                REQUIRE_FALSE( editor );
                REQUIRE_FALSE( dialog );
            }
        }
    }

    GIVEN( "an editor that was closed and is waiting to be deleted" )
    {
        QPointer<FooterEditor> editor = session->openEditor( nullptr );
        REQUIRE( editor );
        editor->reject();
        REQUIRE( editor );

        WHEN( "the session is destroyed" )
        {
            session.reset();

            THEN( "the editor is gone at once, and its deferred deletion is harmless" )
            {
                REQUIRE_FALSE( editor );
                runPendingWork();
            }
        }
    }

    GIVEN( "the editor over an application-modal window that blocks its own" )
    {
        QWidget mainWindow;
        mainWindow.show();
        QDialog modal( &mainWindow );
        modal.setWindowModality( Qt::ApplicationModal );
        modal.show();
        REQUIRE( QApplication::activeModalWidget() == &modal );
        QPointer<FooterEditor> editor = session->openEditor( &mainWindow );
        REQUIRE( editor );
        REQUIRE( editor->parentWidget() == &modal );

        WHEN( "the session is destroyed, and then the modal window closes" )
        {
            session.reset();
            modal.reject();
            runPendingWork();

            THEN( "the editor is gone, and closing the modal window reaches no plugin code" )
            {
                REQUIRE_FALSE( editor );
            }
        }
        modal.reject();
    }
}
