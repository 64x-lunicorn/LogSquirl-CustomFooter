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
 * @file rulefromselectedline_test.cpp
 * @brief BDD tests for "Rule from selected line": the rule editor makes a
 *        new simple rule from a Log Line, and the footer session starts it
 *        with the line the host functions hand over.
 */

#include <catch2/catch.hpp>

#include "eventloopwait.h"
#include "footerconfig.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"
#include "footersession.h"
#include "hostfunctions.h"
#include "rulelistmodel.h"
#include "simplerule.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>

#include <memory>
#include <optional>
#include <utility>

using namespace custom_footer;

namespace {

void writeFile( const QString& path, const QString& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    const auto bytes = content.toUtf8();
    REQUIRE( file.write( bytes ) == bytes.size() );
}

template <typename Widget>
Widget* child( QObject& parent, const char* name )
{
    auto* widget = parent.findChild<Widget*>( name );
    REQUIRE( widget );
    return widget;
}

const QString kVinLine = "2026-10-12 08:15:02.123 INFO VIN: WDB1234567 speed=5";

/// The editor's widgets for a rule from a line, found as the user sees them.
struct FromLineUi {
    explicit FromLineUi( FooterEditor& editor )
        : dialog( &editor )
        , list( child<QTableView>( editor, "ruleList" ) )
        , model( qobject_cast<RuleListModel*>( list->model() ) )
        , line( child<QLineEdit>( editor, "selectedLineEdit" ) )
        , hint( child<QLabel>( editor, "selectedLineHint" ) )
        , key( child<QLineEdit>( editor, "keyEdit" ) )
        , textBefore( child<QLineEdit>( editor, "textBeforeEdit" ) )
        , valueEnd( child<QComboBox>( editor, "valueEndCombo" ) )
        , linePattern( child<QLineEdit>( editor, "linePatternEdit" ) )
        , previewValue( child<QLabel>( editor, "previewValue" ) )
        , buttons( editor.findChild<QDialogButtonBox*>() )
    {
        REQUIRE( model );
        REQUIRE( buttons );
    }

    /// Mark @p value in the shown line, as the user does with the mouse:
    /// its first occurrence after @p after.
    void mark( const QString& value, const QString& after = QString() ) const
    {
        const auto from = line->text().indexOf( after );
        REQUIRE( from >= 0 );
        const auto start = line->text().indexOf( value, from + after.size() );
        REQUIRE( start >= 0 );
        line->setSelection( static_cast<int>( start ), static_cast<int>( value.size() ) );
    }

    /// Click into the shown line where @p value starts: its first
    /// occurrence after @p after.
    void clickAt( const QString& value, const QString& after = QString() ) const
    {
        const auto from = line->text().indexOf( after );
        REQUIRE( from >= 0 );
        const auto start = line->text().indexOf( value, from + after.size() );
        REQUIRE( start >= 0 );
        line->setCursorPosition( static_cast<int>( start ) );
    }

    void select( int row ) const
    {
        list->setCurrentIndex( model->index( row, RuleListModel::KeyColumn ) );
    }

    int currentRow() const
    {
        return list->currentIndex().row();
    }

    bool lineShown() const
    {
        return line->isVisibleTo( dialog );
    }

    ValueEnd shownValueEnd() const
    {
        return static_cast<ValueEnd>( valueEnd->currentData().toInt() );
    }

    bool canAccept() const
    {
        return buttons->button( QDialogButtonBox::Ok )->isEnabled();
    }

    FooterEditor* dialog;
    QTableView* list;
    RuleListModel* model;
    QLineEdit* line;
    QLabel* hint;
    QLineEdit* key;
    QLineEdit* textBefore;
    QComboBox* valueEnd;
    QLineEdit* linePattern;
    QLabel* previewValue;
    QDialogButtonBox* buttons;
};

/// The open message box that says no Log Line is selected, if one is open.
QMessageBox* noSelectionHint()
{
    for ( auto* widget : QApplication::allWidgets() ) {
        auto* box = qobject_cast<QMessageBox*>( widget );
        if ( box && box->objectName() == "noSelectedLineBox" && box->isVisible() ) {
            return box;
        }
    }
    return nullptr;
}

/// Host functions whose selected Log Lines are @p lines, nothing for none;
/// counts the calls.
struct FakeSelection {
    std::optional<QStringList> lines;
    int calls = 0;

    HostFunctions host()
    {
        HostFunctions functions;
        functions.selectedLogLines = [ this ]() -> std::optional<SelectedLogLines> {
            ++calls;
            if ( !lines ) {
                return std::nullopt;
            }
            return SelectedLogLines{ *lines, false };
        };
        return functions;
    }
};

} // namespace

SCENARIO( "The rule editor makes a new simple rule from a Log Line",
          "[rulefromline][footereditor]" )
{
    QTemporaryDir logDir;
    REQUIRE( logDir.isValid() );
    const auto log = logDir.path() + "/car.log";
    writeFile( log, "2026-10-12 08:15:01.000 INFO start\n" + kVinLine + "\n" );

    FooterEditor editor( { { "Mode", "Mode:\\s+(\\S+)", "", true, {} } } );
    editor.setActiveFile( log );
    FromLineUi ui( editor );

    WHEN( "it is opened, and no rule is made from a line" )
    {
        THEN( "it shows no line" )
        {
            REQUIRE_FALSE( ui.lineShown() );
        }
    }

    WHEN( "a rule is made from the VIN line" )
    {
        editor.addRuleFromLine( kVinLine );

        THEN( "a new rule is selected, in simple mode, with the line to mark the value in" )
        {
            REQUIRE( editor.entries().size() == 2 );
            REQUIRE( ui.currentRow() == 1 );
            REQUIRE( ui.lineShown() );
            REQUIRE( ui.line->text() == kVinLine );
            REQUIRE( ui.line->isReadOnly() );
            REQUIRE( ui.textBefore->isVisibleTo( &editor ) );
            REQUIRE( ui.hint->text().contains( "Click where the value starts" ) );
            REQUIRE( ui.canAccept() );
        }

        AND_WHEN( "the VIN is marked in the line" )
        {
            ui.mark( "WDB1234567" );

            THEN( "the text before it, where it ends and the key are filled in" )
            {
                REQUIRE( ui.textBefore->text() == "VIN:" );
                REQUIRE( ui.shownValueEnd() == ValueEnd::Whitespace );
                REQUIRE( ui.key->text() == "VIN" );
                REQUIRE( ui.linePattern->text() == "VIN:\\s*(\\S+)" );
                REQUIRE( ui.hint->text().contains( "“WDB1234567”" ) );
            }

            THEN( "the new rule is a simple rule with those patterns, ready to save" )
            {
                const auto entry = editor.entries().at( 1 );
                REQUIRE( entry.key == "VIN" );
                REQUIRE( entry.linePattern == "VIN:\\s*(\\S+)" );
                REQUIRE( entry.valuePattern.isEmpty() );
                REQUIRE( simpleRuleOf( entry ) );
                REQUIRE( ui.canAccept() );
            }

            THEN( "the preview shows the value it extracts from the active file, from this "
                  "line, so the hint does not warn of another" )
            {
                REQUIRE( processUntil(
                    [ &ui ] { return ui.previewValue->text().contains( "“WDB1234567”" ); } ) );
                REQUIRE_FALSE( ui.hint->text().contains( "matches first" ) );
            }

            AND_WHEN( "the key is changed, and another value is marked" )
            {
                ui.key->setText( "Speed" );
                ui.mark( "5", "speed=" );

                THEN( "the rule follows the new mark, but the key typed is kept" )
                {
                    REQUIRE( ui.textBefore->text() == "speed=" );
                    REQUIRE( ui.key->text() == "Speed" );
                }
            }

            AND_WHEN( "another value is marked without changing the key" )
            {
                ui.mark( "5", "speed=" );

                THEN( "the key suggested for it replaces the one suggested before" )
                {
                    REQUIRE( ui.textBefore->text() == "speed=" );
                    REQUIRE( ui.key->text() == "speed" );
                }
            }

            AND_WHEN( "the word at the start of the line is marked, with no text before it" )
            {
                ui.mark( "2026-10-12" );

                THEN( "the rule is kept as it was, and the hint says why" )
                {
                    REQUIRE( ui.textBefore->text() == "VIN:" );
                    REQUIRE( ui.key->text() == "VIN" );
                    REQUIRE( ui.hint->text().contains( "starts the line" ) );
                }
            }
        }

        AND_WHEN( "the user clicks where the VIN starts" )
        {
            ui.clickAt( "WDB1234567" );

            THEN( "the word starting there is the value" )
            {
                REQUIRE( ui.textBefore->text() == "VIN:" );
                REQUIRE( ui.key->text() == "VIN" );
                REQUIRE( editor.entries().at( 1 ).linePattern == "VIN:\\s*(\\S+)" );
            }
        }

        AND_WHEN( "another rule is selected" )
        {
            ui.select( 0 );

            THEN( "the line is not shown for it" )
            {
                REQUIRE_FALSE( ui.lineShown() );
            }

            AND_WHEN( "the rule made from the line is selected again" )
            {
                ui.select( 1 );

                THEN( "the line is shown again" )
                {
                    REQUIRE( ui.lineShown() );
                }
            }
        }

        AND_WHEN( "the rule made from the line is moved up" )
        {
            child<QToolButton>( editor, "moveUpButton" )->click();
            ui.mark( "WDB1234567" );

            THEN( "the line stays with it" )
            {
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.lineShown() );
                REQUIRE( editor.entries().at( 0 ).key == "VIN" );
                REQUIRE( editor.entries().at( 1 ).key == "Mode" );
            }
        }

        AND_WHEN( "the rule made from the line is removed" )
        {
            child<QToolButton>( editor, "removeRuleButton" )->click();

            THEN( "the line goes with it" )
            {
                REQUIRE( editor.entries().size() == 1 );
                REQUIRE_FALSE( ui.lineShown() );
            }
        }
    }

    GIVEN( "a rule made from a line with special characters before the value" )
    {
        const QString line = QString::fromUtf8( "\t  [sensor.temp](°C)*+?^$|{2}\\:\t21.5 ok  " );
        editor.addRuleFromLine( line );

        WHEN( "the value is marked" )
        {
            ui.mark( "21.5" );

            THEN( "the text is taken literally, and the pattern escapes it" )
            {
                REQUIRE( ui.textBefore->text() == "[sensor.temp](°C)*+?^$|{2}\\:" );
                REQUIRE( ui.linePattern->text()
                         == QString::fromUtf8( "\\[sensor\\.temp]\\(°C\\)\\*\\+\\?\\^\\$\\|\\{2}"
                                               "\\\\:\\s*(\\S+)" ) );
                REQUIRE( ui.key->text() == "sensor.temp" );
            }
        }
    }

    WHEN( "a rule is made from the first of several selected lines" )
    {
        editor.addRuleFromLine( kVinLine, 3 );

        THEN( "the hint says that the first of them is shown" )
        {
            REQUIRE( ui.line->text() == kVinLine );
            REQUIRE( ui.hint->text().contains( "3 Log Lines are selected" ) );
        }
    }

    GIVEN( "a rule made from a line, with its value marked" )
    {
        editor.addRuleFromLine( kVinLine );
        ui.mark( "WDB1234567" );

        WHEN( "a rule is made from another line" )
        {
            editor.addRuleFromLine( "user=alice, id=1" );

            THEN( "a third rule shows the new line, and the first one keeps its rule" )
            {
                REQUIRE( editor.entries().size() == 3 );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( ui.line->text() == "user=alice, id=1" );
                REQUIRE( editor.entries().at( 1 ).key == "VIN" );
            }
        }
    }
}

SCENARIO( "The rule editor shows only the part of a long Log Line the footer reads",
          "[rulefromline][footereditor]" )
{
    FooterEditor editor( {} );
    FromLineUi ui( editor );
    auto* cut = child<QLabel>( editor, "selectedLineCutNote" );

    WHEN( "a rule is made from a short line" )
    {
        editor.addRuleFromLine( kVinLine );

        THEN( "all of it is shown, without a note" )
        {
            REQUIRE( ui.line->text() == kVinLine );
            REQUIRE_FALSE( cut->isVisibleTo( &editor ) );
        }
    }

    WHEN( "a rule is made from a line of 1 MiB" )
    {
        // Two bytes per "ä" in UTF-8: the footer reads 32768 of them.
        const QString line = "VIN: WDB1 " + QString( 1024 * 1024, QChar( 0xE4 ) );
        editor.addRuleFromLine( line );

        THEN( "its first 64 KiB are shown, cut at a character, with a note that the rest is cut "
              "off" )
        {
            REQUIRE( ui.line->text() == line.left( 10 + 32763 ) );
            REQUIRE( ui.line->text().toUtf8().size() <= 64 * 1024 );
            REQUIRE( cut->isVisibleTo( &editor ) );
            REQUIRE( cut->text().contains( "64 KiB" ) );
        }

        AND_WHEN( "the value is marked" )
        {
            ui.mark( "WDB1" );

            THEN( "the rule is made" )
            {
                REQUIRE( ui.textBefore->text() == "VIN:" );
            }
        }
    }
}

SCENARIO( "The rule editor says when an earlier Log Line supplies the rule's value",
          "[rulefromline][footereditor]" )
{
    QTemporaryDir logDir;
    REQUIRE( logDir.isValid() );
    const auto log = logDir.path() + "/car.log";
    writeFile( log, "start\nVIN: EARLY1\n" + kVinLine + "\n" );

    GIVEN( "a rule made from a line, in a file where an earlier line has the same text" )
    {
        FooterEditor editor( {} );
        editor.setActiveFile( log );
        FromLineUi ui( editor );
        editor.addRuleFromLine( kVinLine );

        WHEN( "the value is marked, and the preview is shown" )
        {
            ui.mark( "WDB1234567" );
            REQUIRE( processUntil(
                [ &ui ] { return ui.previewValue->text().contains( "“EARLY1”" ); } ) );

            THEN( "the hint says which Log Line matches first, and its value" )
            {
                REQUIRE( ui.hint->text().contains( "Log Line 2 matches first" ) );
                REQUIRE( ui.hint->text().contains( "“EARLY1”" ) );
            }

            AND_WHEN( "a mark that makes no rule follows" )
            {
                ui.mark( "2026-10-12" );

                THEN( "the hint says why, and still that Log Line 2 matches first, as the rule "
                      "is unchanged" )
                {
                    REQUIRE( ui.hint->text().contains( "starts the line" ) );
                    REQUIRE( ui.hint->text().contains( "Log Line 2 matches first" ) );
                }
            }

            AND_WHEN( "the rule is changed by hand so that only the selected line matches" )
            {
                ui.textBefore->setText( "speed=" );
                REQUIRE(
                    processUntil( [ &ui ] { return ui.previewValue->text().contains( "“5”" ); } ) );

                THEN( "the hint no longer describes the old mark, nor an earlier Log Line" )
                {
                    REQUIRE_FALSE( ui.hint->text().contains( "WDB1234567" ) );
                    REQUIRE_FALSE( ui.hint->text().contains( "matches first" ) );
                    REQUIRE( ui.hint->text().contains( "Click where the value starts" ) );
                }
            }
        }
    }
}

SCENARIO( "The rule editor takes no mark in the line while the Regex Lab is open",
          "[rulefromline][footereditor][regexlab]" )
{
    FooterEditor editor( {} );
    RegexLabDone answer;
    editor.setRegexLab( [ &answer ]( const RegexLabPattern&, const QObject*, RegexLabDone done ) {
        answer = std::move( done );
        return true;
    } );
    FromLineUi ui( editor );

    GIVEN( "a rule made from the VIN line, open in the Regex Lab" )
    {
        editor.addRuleFromLine( kVinLine );
        ui.mark( "WDB1234567" );
        child<QToolButton>( editor, "testLinePatternButton" )->click();
        REQUIRE( answer );

        WHEN( "another value is marked" )
        {
            ui.mark( "5", "speed=" );

            THEN( "the rule is kept for the Lab's answer, and the hint says why" )
            {
                REQUIRE( ui.textBefore->text() == "VIN:" );
                REQUIRE( editor.entries().at( 0 ).linePattern == "VIN:\\s*(\\S+)" );
                REQUIRE( ui.hint->text().contains( "Regex Lab is open" ) );
            }

            AND_WHEN( "the Lab is cancelled, and the value is marked anew" )
            {
                std::exchange( answer, nullptr )( std::nullopt );
                // As a new drag starts: the mark changes on its way.
                ui.clickAt( "INFO" );
                ui.mark( "5", "speed=" );

                THEN( "the mark makes the rule" )
                {
                    REQUIRE( ui.textBefore->text() == "speed=" );
                }
            }
        }
    }
}

SCENARIO( "The rule editor offers a rule from the selected line only when asked to",
          "[rulefromline][footereditor]" )
{
    FooterEditor editor( {} );
    auto* button = child<QToolButton>( editor, "ruleFromLineButton" );

    WHEN( "it is not offered" )
    {
        THEN( "the button is hidden" )
        {
            REQUIRE_FALSE( button->isVisibleTo( &editor ) );
        }
    }

    WHEN( "it is offered, and the button is clicked" )
    {
        editor.offerRuleFromLine( true );
        int requests = 0;
        QObject::connect( &editor, &FooterEditor::ruleFromLineRequested,
                          [ &requests ] { ++requests; } );
        button->click();

        THEN( "the button is shown, and the editor asks for the selected line" )
        {
            REQUIRE( button->isVisibleTo( &editor ) );
            REQUIRE( requests == 1 );
        }
    }
}

SCENARIO( "A footer session makes a rule from the Log Line selected in the host",
          "[rulefromline][footersession]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                        { { "Mode", "Mode:\\s+(\\S+)", "", true, {} } } ) );
    const auto log = logDir.path() + "/car.log";
    writeFile( log, kVinLine + "\n" );

    FooterDisplayWidget footer;
    FakeSelection selection;
    QWidget window;

    GIVEN( "a host with a Log Line selected" )
    {
        selection.lines = QStringList{ kVinLine };
        FooterSession session( configDir.path(), &footer, selection.host() );
        session.setActiveFile( log );

        WHEN( "a rule is made from the selected line, with no editor open" )
        {
            QPointer<FooterEditor> editor = session.ruleFromSelectedLine( &window );

            THEN( "the editor opens over the window with a new rule made from that line" )
            {
                REQUIRE( selection.calls == 1 );
                REQUIRE( editor );
                REQUIRE( editor->isVisible() );
                REQUIRE( editor->parentWidget() == &window );
                FromLineUi ui( *editor );
                REQUIRE( editor->entries().size() == 2 );
                REQUIRE( ui.currentRow() == 1 );
                REQUIRE( ui.line->text() == kVinLine );
                REQUIRE( ui.lineShown() );
            }

            THEN( "the editor offers the same action as a button" )
            {
                REQUIRE( child<QToolButton>( *editor, "ruleFromLineButton" )
                             ->isVisibleTo( editor.data() ) );
            }

            AND_WHEN( "the VIN is marked and the editor is accepted" )
            {
                FromLineUi( *editor ).mark( "WDB1234567" );
                editor->accept();

                THEN( "the rule is saved and the footer shows the VIN" )
                {
                    const auto saved = FooterConfig::loadEntries( configDir.path() );
                    REQUIRE( saved.size() == 2 );
                    REQUIRE( saved.at( 1 ).key == "VIN" );
                    REQUIRE( processUntil( [ &footer ] {
                        for ( const auto& value : footer.values() ) {
                            if ( value.key == "VIN" && value.value == "WDB1234567" ) {
                                return true;
                            }
                        }
                        return false;
                    } ) );
                }
            }

            AND_WHEN( "the editor's button is clicked" )
            {
                child<QToolButton>( *editor, "ruleFromLineButton" )->click();

                THEN( "another rule is made from the line in the same editor" )
                {
                    REQUIRE( selection.calls == 2 );
                    REQUIRE( editor->entries().size() == 3 );
                    REQUIRE( FromLineUi( *editor ).currentRow() == 2 );
                }
            }
        }

        WHEN( "a rule is made from the selected line while the editor is open" )
        {
            QPointer<FooterEditor> open = session.openEditor( &window );
            open->appendEntries( { { "Edited", "", "", true, {} } } );
            QPointer<FooterEditor> editor = session.ruleFromSelectedLine( &window );

            THEN( "the open editor gets the new rule, with its edits kept" )
            {
                REQUIRE( editor == open );
                REQUIRE( editor->entries().size() == 3 );
                REQUIRE( editor->entries().at( 1 ).key == "Edited" );
            }
        }
    }

    GIVEN( "a host with several Log Lines selected" )
    {
        selection.lines = QStringList{ kVinLine, "second", "third" };
        FooterSession session( configDir.path(), &footer, selection.host() );

        WHEN( "a rule is made from the selected line" )
        {
            QPointer<FooterEditor> editor = session.ruleFromSelectedLine( &window );

            THEN( "it is made from the first one, and the hint says so" )
            {
                REQUIRE( editor );
                FromLineUi ui( *editor );
                REQUIRE( ui.line->text() == kVinLine );
                REQUIRE( ui.hint->text().contains( "3 Log Lines are selected" ) );
            }
        }
    }

    GIVEN( "a host with a Log File open, but no Log Line selected" )
    {
        FooterSession session( configDir.path(), &footer, selection.host() );
        session.setActiveFile( log );

        WHEN( "a rule is made from the selected line" )
        {
            auto* editor = session.ruleFromSelectedLine( &window );

            THEN( "no editor opens; a message over the window, opened without blocking, says what "
                  "to do" )
            {
                REQUIRE_FALSE( editor );
                auto* hint = noSelectionHint();
                REQUIRE( hint );
                REQUIRE( hint->parentWidget() == &window );
                REQUIRE( hint->text().contains( "No Log Line is selected" ) );
                REQUIRE( hint->text().contains( "Rule from Selected Line…" ) );
            }

            AND_WHEN( "it is tried again while the message is open" )
            {
                QPointer<QMessageBox> first = noSelectionHint();
                session.ruleFromSelectedLine( &window );

                THEN( "the same message is shown, not a second one" )
                {
                    REQUIRE( noSelectionHint() == first );
                    int boxes = 0;
                    for ( auto* widget : QApplication::allWidgets() ) {
                        if ( widget->objectName() == "noSelectedLineBox" ) {
                            ++boxes;
                        }
                    }
                    REQUIRE( boxes == 1 );
                }
            }
        }

        WHEN( "the editor's button is clicked" )
        {
            QPointer<FooterEditor> editor = session.openEditor( &window );
            child<QToolButton>( *editor, "ruleFromLineButton" )->click();

            THEN( "the message is shown over the editor, and says to keep the rules with OK or "
                  "Apply before closing it, as the selection cannot change meanwhile; no rule is "
                  "added" )
            {
                auto* hint = noSelectionHint();
                REQUIRE( hint );
                REQUIRE( hint->parentWidget() == editor );
                REQUIRE( hint->text().contains( "cannot change while the rule editor is open" ) );
                REQUIRE( hint->text().contains(
                    "Click OK to keep your rules and close the editor (Cancel discards them)" ) );
                REQUIRE( editor->entries().size() == 1 );
            }
        }
    }

    GIVEN( "a host with no Log File open" )
    {
        FooterSession session( configDir.path(), &footer, selection.host() );

        WHEN( "a rule is made from the selected line" )
        {
            session.ruleFromSelectedLine( &window );

            THEN( "the message says to open a Log File first" )
            {
                auto* hint = noSelectionHint();
                REQUIRE( hint );
                REQUIRE( hint->text().contains( "No Log File is open" ) );
                REQUIRE( hint->text().contains( "Open one" ) );
            }
        }

        WHEN( "the editor's button is clicked" )
        {
            QPointer<FooterEditor> editor = session.openEditor( &window );
            child<QToolButton>( *editor, "ruleFromLineButton" )->click();

            THEN( "the message over the editor says so too" )
            {
                auto* hint = noSelectionHint();
                REQUIRE( hint );
                REQUIRE( hint->text().contains( "No Log File is open" ) );
                REQUIRE( hint->text().contains(
                    "Click OK to keep your rules and close the editor (Cancel discards them)" ) );
            }
        }
    }

    GIVEN( "a host without the selected Log Lines, as before LogSquirl 26.11" )
    {
        FooterSession session( configDir.path(), &footer, HostFunctions() );

        WHEN( "a rule is made from the selected line" )
        {
            auto* editor = session.ruleFromSelectedLine( &window );

            THEN( "nothing happens" )
            {
                REQUIRE_FALSE( editor );
                REQUIRE_FALSE( noSelectionHint() );
            }
        }

        WHEN( "the editor is opened" )
        {
            QPointer<FooterEditor> editor = session.openEditor( &window );

            THEN( "it does not offer the action" )
            {
                REQUIRE_FALSE( child<QToolButton>( *editor, "ruleFromLineButton" )
                                   ->isVisibleTo( editor.data() ) );
            }
        }
    }
}

SCENARIO( "Destroying a footer session closes its message about the selected line",
          "[rulefromline][footersession][lifetime]" )
{
    FooterDisplayWidget footer;
    FakeSelection selection;
    QWidget window;
    auto session = std::make_unique<FooterSession>( QString(), &footer, selection.host() );

    GIVEN( "the message that no Log Line is selected, open over the window" )
    {
        session->ruleFromSelectedLine( &window );
        QPointer<QMessageBox> hint = noSelectionHint();
        REQUIRE( hint );

        WHEN( "the session is destroyed" )
        {
            session.reset();

            THEN( "the message is gone at once" )
            {
                REQUIRE_FALSE( hint );
            }
        }
    }
}
