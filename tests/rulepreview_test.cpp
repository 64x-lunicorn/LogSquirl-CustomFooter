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
 * @file rulepreview_test.cpp
 * @brief BDD tests for the rule editor's live preview: debouncing, dropping
 *        outdated results, cancelling, and what the preview section shows.
 */

#include <catch2/catch.hpp>

#include "footereditor.h"
#include "footerscanner.h"
#include "rulelistmodel.h"
#include "rulepreviewer.h"
#include "rulepreviewview.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QSemaphore>
#include <QTableView>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextEdit>
#include <QThread>

#include <atomic>
#include <memory>

using namespace custom_footer;

namespace {

using Preview = FooterScanner::Preview;
using Status = Preview::Status;

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

void writeBytes( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

FooterEntry rule( const QString& key, const QString& linePattern )
{
    return { key, linePattern, "", true, {} };
}

/// A preview naming the line pattern it was made for, instantly.
Preview previewOfPattern( const QList<FooterEntry>& entries, int index )
{
    Preview preview;
    preview.status = Status::Scanned;
    preview.error = entries[ index ].linePattern;
    return preview;
}

/// Collects what a previewer emits.
struct Emitted {
    explicit Emitted( RulePreviewer& previewer )
    {
        QObject::connect(
            &previewer, &RulePreviewer::previewed,
            [ this ]( const Preview& preview ) { patterns.append( preview.error ); } );
    }
    QStringList patterns;
};

/// A preview function that blocks the worker for some patterns until the
/// test releases it, or until the preview is cancelled if so asked.
struct GatedPreview {
    QSemaphore started;
    QSemaphore release;
    std::atomic_bool sawCancel{ false };
    std::atomic_int calls{ 0 };
    bool untilCancelled = false;

    RulePreviewer::PreviewFunction function()
    {
        return [ this ]( const QString&, const QList<FooterEntry>& entries, int index, int,
                         const std::atomic_bool* cancelled ) {
            ++calls;
            if ( entries[ index ].linePattern.startsWith( "slow" ) ) {
                started.release();
                if ( untilCancelled ) {
                    // Bounded, so that a missing cancel fails instead of hanging.
                    for ( int i = 0; i < 10000 && !cancelled->load(); ++i ) {
                        QThread::msleep( 1 );
                    }
                }
                else {
                    release.acquire();
                }
                sawCancel = cancelled->load();
            }
            return previewOfPattern( entries, index );
        };
    }
};

template <typename Widget>
Widget* child( QObject& parent, const char* name )
{
    auto* widget = parent.findChild<Widget*>( name );
    REQUIRE( widget );
    return widget;
}

} // namespace

SCENARIO( "RulePreviewer previews once typing pauses", "[preview][previewer]" )
{
    GIVEN( "a previewer with an active file" )
    {
        RulePreviewer previewer;
        previewer.setPreviewFunction(
            []( const QString&, const QList<FooterEntry>& entries, int index, int,
                const std::atomic_bool* ) { return previewOfPattern( entries, index ); } );
        previewer.setActiveFile( "/some/file.log" );
        Emitted emitted( previewer );

        WHEN( "several edits are requested in a row" )
        {
            for ( const auto* pattern : { "a", "ab", "abc", "abcd" } ) {
                previewer.schedule( { rule( "K", pattern ) }, 0 );
            }

            THEN( "nothing is scanned before the pause" )
            {
                REQUIRE( previewer.previewsStarted() == 0 );
                REQUIRE( previewer.isBusy() );
            }

            AND_WHEN( "the pause ends" )
            {
                previewer.flush();
                REQUIRE( processUntil( [ &previewer ] { return !previewer.isBusy(); } ) );

                THEN( "only the last edit is previewed" )
                {
                    REQUIRE( previewer.previewsStarted() == 1 );
                    REQUIRE( emitted.patterns == QStringList{ "abcd" } );
                }
            }
        }

        WHEN( "an edit is requested and the event loop runs on" )
        {
            previewer.schedule( { rule( "K", "timed" ) }, 0 );
            REQUIRE( processUntil( [ &previewer ] { return !previewer.isBusy(); } ) );

            THEN( "the pause ends by itself, and the edit is previewed" )
            {
                REQUIRE( emitted.patterns == QStringList{ "timed" } );
            }
        }

        THEN( "the pause is short" )
        {
            REQUIRE( RulePreviewer::kDelayMs == 300 );
        }
    }

    GIVEN( "a previewer without an active file" )
    {
        RulePreviewer previewer;
        std::vector<Preview> previews;
        QObject::connect(
            &previewer, &RulePreviewer::previewed,
            [ &previews ]( const Preview& preview ) { previews.push_back( preview ); } );

        WHEN( "a rule is previewed" )
        {
            previewer.schedule( { rule( "K", "k=(\\S+)" ) }, 0 );
            previewer.flush();

            THEN( "it says so at once, without a worker" )
            {
                REQUIRE( previewer.previewsStarted() == 0 );
                REQUIRE( previews.size() == 1 );
                REQUIRE( previews[ 0 ].status == Status::NoFile );
            }
        }
    }
}

SCENARIO( "RulePreviewer never shows an outdated preview", "[preview][previewer]" )
{
    GIVEN( "a preview of an old pattern still running" )
    {
        GatedPreview gate;
        RulePreviewer previewer;
        previewer.setPreviewFunction( gate.function() );
        previewer.setActiveFile( "/some/file.log" );
        Emitted emitted( previewer );

        previewer.schedule( { rule( "K", "slow-old" ) }, 0 );
        previewer.flush();
        gate.started.acquire();

        WHEN( "a newer pattern is previewed, and the old preview finishes first" )
        {
            previewer.schedule( { rule( "K", "new" ) }, 0 );
            previewer.flush();
            gate.release.release();
            REQUIRE( processUntil( [ &previewer ] { return !previewer.isBusy(); } ) );

            THEN( "only the newer preview is shown" )
            {
                REQUIRE( gate.calls == 2 );
                REQUIRE( emitted.patterns == QStringList{ "new" } );
            }
            THEN( "the old preview was told to stop" )
            {
                REQUIRE( gate.sawCancel );
            }
        }

        WHEN( "a newer pattern is typed, and the old preview finishes during the pause" )
        {
            previewer.schedule( { rule( "K", "newer" ) }, 0 );
            gate.release.release();
            // Let the old preview's result arrive while the new one waits.
            previewer.stop();
            QCoreApplication::processEvents();

            THEN( "the old result is not shown" )
            {
                REQUIRE( emitted.patterns.isEmpty() );
            }
        }
    }
}

SCENARIO( "The editor's preview stops when the editor closes", "[preview][footereditor]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/sample.log";
    writeBytes( path, "VIN: ABC\n" );

    GIVEN( "an editor whose preview is running" )
    {
        GatedPreview gate;
        gate.untilCancelled = true;
        auto* editor = new FooterEditor( { rule( "VIN", "slow VIN" ) } );
        auto* previewer = child<RulePreviewer>( *editor, "rulePreviewer" );
        previewer->setPreviewFunction( gate.function() );
        int shown = 0;
        QObject::connect( previewer, &RulePreviewer::previewed, [ &shown ] { ++shown; } );

        editor->setActiveFile( path );
        gate.started.acquire();

        WHEN( "the editor is destroyed mid-preview" )
        {
            delete editor;

            THEN( "the preview was cancelled and waited for, and nothing was shown" )
            {
                REQUIRE( gate.sawCancel );
                QCoreApplication::processEvents();
                REQUIRE( shown == 0 );
            }
        }

        WHEN( "the editor is closed mid-preview" )
        {
            editor->reject();

            THEN( "the preview was cancelled and waited for" )
            {
                REQUIRE( gate.sawCancel );
                REQUIRE_FALSE( previewer->isBusy() );
                QCoreApplication::processEvents();
                REQUIRE( shown == 0 );
            }
            delete editor;
        }
    }
}

SCENARIO( "The editor previews the selected rule against the active file",
          "[preview][footereditor]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/sample.log";
    writeBytes( path, "boot\n"
                      "legacy-mode 4\n"
                      "noise\n"
                      "[init] mode=0x04 set\n"
                      "mode=0x07\n"
                      "tail\n" );

    QList<FooterEntry> entries;
    entries.append( { "Mode", "mode=(\\S+)", "", true, { { "0x04", "Production" } } } );
    entries.append( { "Other", "tail", "", true, {} } );
    entries.append( { "Mode", "legacy-mode (\\S+)", "", true, {} } );

    FooterEditor editor( entries );
    auto* previewer = child<RulePreviewer>( editor, "rulePreviewer" );
    auto* view = child<RulePreviewView>( editor, "rulePreview" );
    auto* message = child<QLabel>( *view, "previewMessage" );
    auto* updating = child<QLabel>( *view, "previewUpdating" );
    auto* location = child<QLabel>( *view, "previewLocation" );
    auto* line = child<QTextEdit>( *view, "previewLine" );
    auto* value = child<QLabel>( *view, "previewValue" );
    auto* count = child<QLabel>( *view, "previewCount" );
    auto* key = child<QLabel>( *view, "previewKey" );
    auto* list = child<QTableView>( editor, "ruleList" );
    auto* linePattern = child<QLineEdit>( editor, "linePatternEdit" );
    auto* keyEdit = child<QLineEdit>( editor, "keyEdit" );

    const auto settle = [ previewer ] {
        previewer->flush();
        REQUIRE( processUntil( [ previewer ] { return !previewer->isBusy(); } ) );
    };
    const auto select = [ list ]( int row ) {
        list->setCurrentIndex( list->model()->index( row, RuleListModel::KeyColumn ) );
    };

    GIVEN( "no active file" )
    {
        THEN( "the preview explains that a file is needed" )
        {
            REQUIRE( editor.activeFile().isEmpty() );
            REQUIRE( message->text().contains( "Open a log file" ) );
            REQUIRE_FALSE( message->isHidden() );
            REQUIRE( line->isHidden() );
            REQUIRE( count->isHidden() );
        }
    }

    GIVEN( "the sample file as the active file, and the first rule selected" )
    {
        editor.setActiveFile( path );
        settle();

        THEN( "the first matching line is shown, with its number and highlights" )
        {
            REQUIRE( editor.activeFile() == path );
            REQUIRE( view->title() == "Preview in sample.log" );
            REQUIRE( location->text() == "First match, line 4:" );
            REQUIRE( line->toPlainText() == "[init] mode=0x04 set" );

            const auto backgroundAt = [ line ]( int position ) {
                QTextCursor cursor( line->document() );
                cursor.setPosition( position + 1 ); // the format of the character before
                return cursor.charFormat().background().color();
            };
            REQUIRE( backgroundAt( 0 ) != RulePreviewView::lineMatchColor() ); // "["
            REQUIRE( backgroundAt( 7 ) == RulePreviewView::lineMatchColor() ); // "mode="
            REQUIRE( backgroundAt( 12 ) == RulePreviewView::valueColor() );    // "0x04"
            REQUIRE( backgroundAt( 15 ) == RulePreviewView::valueColor() );
            REQUIRE( backgroundAt( 17 ) != RulePreviewView::lineMatchColor() ); // "set"
        }
        THEN( "the raw value and the mapped value are shown" )
        {
            REQUIRE( value->text() == "Raw value: “0x04”\nAfter mappings: “Production”" );
        }
        THEN( "the matching lines are counted in the whole file" )
        {
            REQUIRE( count->text() == "2 matching lines in all 6 lines of the file." );
        }
        THEN( "for the shared key, the other rule's earlier line supplies the value" )
        {
            REQUIRE( key->text()
                     == "Rule 3 supplies “Mode” in this file instead, from line 2: “4”" );
        }
        THEN( "a rule found in the file needs no explanation" )
        {
            REQUIRE( message->isHidden() );
            REQUIRE( updating->isHidden() );
        }

        WHEN( "the alternative that supplies the key is selected" )
        {
            select( 2 );
            settle();

            THEN( "the preview says this rule supplies it" )
            {
                REQUIRE( location->text() == "First match, line 2:" );
                REQUIRE( value->text()
                         == "Raw value: “4”\nAfter mappings: “4”, no mapping applies" );
                REQUIRE( key->text().startsWith( "This rule supplies “Mode”" ) );
            }
        }

        WHEN( "a rule of its own key is selected" )
        {
            select( 1 );
            settle();

            THEN( "nothing is said about other rules" )
            {
                REQUIRE( key->isHidden() );
                REQUIRE( count->text() == "1 matching line in all 6 lines of the file." );
            }
        }

        WHEN( "the line pattern is typed" )
        {
            const int started = previewer->previewsStarted();
            linePattern->setText( "noi" );
            linePattern->setText( "nois" );
            linePattern->setText( "noise" );

            THEN( "the shown preview is marked outdated, and nothing is scanned yet" )
            {
                REQUIRE_FALSE( updating->isHidden() );
                REQUIRE( previewer->previewsStarted() == started );
            }

            AND_WHEN( "typing pauses" )
            {
                settle();

                THEN( "the last pattern is previewed once" )
                {
                    REQUIRE( previewer->previewsStarted() == started + 1 );
                    REQUIRE( updating->isHidden() );
                    REQUIRE( location->text() == "First match, line 3:" );
                    REQUIRE( value->text().startsWith( "Raw value: “noise”" ) );
                }
            }
        }

        WHEN( "the key is changed to one no other rule has" )
        {
            keyEdit->setText( "Own" );
            settle();

            THEN( "nothing is said about other rules" )
            {
                REQUIRE( key->isHidden() );
            }
        }

        WHEN( "the line pattern becomes invalid" )
        {
            linePattern->setText( "mode=(" );
            settle();

            THEN( "the preview explains why instead of showing a result" )
            {
                REQUIRE( message->text().startsWith(
                    "This rule cannot match anything until its line pattern is fixed: " ) );
                REQUIRE( line->isHidden() );
                REQUIRE( value->isHidden() );
                REQUIRE( count->isHidden() );
            }
        }

        WHEN( "the line pattern matches nothing" )
        {
            linePattern->setText( "absent" );
            settle();

            THEN( "the preview says so, with the lines scanned" )
            {
                REQUIRE( message->text() == "No line of the file matches this rule." );
                REQUIRE( line->isHidden() );
                REQUIRE( count->text() == "0 matching lines in all 6 lines of the file." );
            }
        }

        WHEN( "the footer's line limit is lower than the file" )
        {
            editor.setMaxLines( 3 );
            editor.setActiveFile( path );
            settle();

            THEN( "the count says the limit was reached" )
            {
                REQUIRE( count->text()
                         == "0 matching lines in the first 3 lines: the scan stops at this line "
                            "limit." );
                REQUIRE( message->text() == "No line matches this rule within the scan limits." );
            }
        }

        WHEN( "the rule is disabled" )
        {
            editor.findChild<QWidget*>( "ruleEnabledCheck" )->setProperty( "checked", false );
            settle();

            THEN( "it is still previewed, with a note" )
            {
                REQUIRE( message->text() == "This rule is disabled: the footer does not use it." );
                REQUIRE( location->text() == "First match, line 4:" );
            }
        }

        WHEN( "the active file goes away" )
        {
            editor.setActiveFile( QString() );

            THEN( "the preview explains that a file is needed" )
            {
                REQUIRE( message->text().contains( "Open a log file" ) );
                REQUIRE( line->isHidden() );
            }
        }
    }
}

SCENARIO( "The preview section counts matches in readable words", "[preview]" )
{
    Preview preview;
    preview.status = Status::Scanned;
    preview.matches = 12;
    preview.lines = 100000;
    preview.lineLimitReached = true;

    THEN( "a reached line limit is named" )
    {
        REQUIRE( RulePreviewView::countText( preview )
                 == QString( "12 matching lines in the first %1 lines: the scan stops at this "
                             "line limit." )
                        .arg( QLocale().toString( 100000 ) ) );
    }
    THEN( "a reached size limit is named" )
    {
        preview.lineLimitReached = false;
        preview.byteLimitReached = true;
        preview.matches = 1;
        preview.lines = 7;
        REQUIRE( RulePreviewView::countText( preview )
                 == "1 matching line in the first 64 MiB (7 lines): the scan stops at this size "
                    "limit." );
    }
}
