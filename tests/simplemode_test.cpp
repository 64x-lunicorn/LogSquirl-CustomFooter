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
 * @file simplemode_test.cpp
 * @brief BDD tests for simple and advanced mode in the rule editor.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footereditor.h"
#include "footerscanner.h"
#include "rulelistmodel.h"
#include "simplerule.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>

#include <memory>

using namespace custom_footer;

namespace {

/// Test access to the editor's widgets for the patterns, by object name.
struct SimpleUi {
    template <typename Widget>
    static Widget* find( FooterEditor& editor, const char* name )
    {
        auto* widget = editor.findChild<Widget*>( name );
        REQUIRE( widget );
        return widget;
    }

    explicit SimpleUi( FooterEditor& editor )
        : dialog( &editor )
        , list( find<QTableView>( editor, "ruleList" ) )
        , model( qobject_cast<RuleListModel*>( list->model() ) )
        , addRule( find<QToolButton>( editor, "addRuleButton" ) )
        , key( find<QLineEdit>( editor, "keyEdit" ) )
        , advanced( find<QCheckBox>( editor, "advancedCheck" ) )
        , textBefore( find<QLineEdit>( editor, "textBeforeEdit" ) )
        , valueEnd( find<QComboBox>( editor, "valueEndCombo" ) )
        , endCharacter( find<QLineEdit>( editor, "endCharacterEdit" ) )
        , endCharacterProblem( find<QLabel>( editor, "endCharacterProblem" ) )
        , linePattern( find<QLineEdit>( editor, "linePatternEdit" ) )
        , valuePattern( find<QLineEdit>( editor, "valuePatternEdit" ) )
        , buttons( editor.findChild<QDialogButtonBox*>() )
    {
        REQUIRE( model );
        REQUIRE( buttons );
    }

    void select( int row ) const
    {
        list->setCurrentIndex( model->index( row, RuleListModel::KeyColumn ) );
    }

    QString listPattern( int row ) const
    {
        return model->index( row, RuleListModel::LinePatternColumn ).data().toString();
    }

    void chooseValueEnd( ValueEnd end ) const
    {
        const int index = valueEnd->findData( static_cast<int>( end ) );
        REQUIRE( index >= 0 );
        valueEnd->setCurrentIndex( index );
    }

    ValueEnd shownValueEnd() const
    {
        return static_cast<ValueEnd>( valueEnd->currentData().toInt() );
    }

    /// The simple fields are shown and the patterns only shown.
    bool inSimpleMode() const
    {
        return !advanced->isChecked() && textBefore->isVisibleTo( dialog )
               && valueEnd->isVisibleTo( dialog ) && linePattern->isReadOnly()
               && valuePattern->isReadOnly();
    }

    /// The patterns are editable and the simple fields hidden.
    bool inAdvancedMode() const
    {
        return advanced->isChecked() && !textBefore->isVisibleTo( dialog )
               && !valueEnd->isVisibleTo( dialog ) && !linePattern->isReadOnly()
               && !valuePattern->isReadOnly();
    }

    bool canAccept() const
    {
        return buttons->button( QDialogButtonBox::Ok )->isEnabled()
               && buttons->button( QDialogButtonBox::Apply )->isEnabled();
    }

    /// Whether the end character is marked, with the reason next to it.
    bool endCharacterMarked() const
    {
        return !endCharacter->toolTip().isEmpty()
               && endCharacterProblem->text() == endCharacter->toolTip()
               && endCharacterProblem->isVisibleTo( dialog );
    }

    FooterEditor* dialog;
    QTableView* list;
    RuleListModel* model;
    QToolButton* addRule;
    QLineEdit* key;
    QCheckBox* advanced;
    QLineEdit* textBefore;
    QComboBox* valueEnd;
    QLineEdit* endCharacter;
    QLabel* endCharacterProblem;
    QLineEdit* linePattern;
    QLineEdit* valuePattern;
    QDialogButtonBox* buttons;
};

void press( QWidget* widget, int key )
{
    QKeyEvent event( QEvent::KeyPress, key, Qt::NoModifier );
    QApplication::sendEvent( widget, &event );
}

/// Type @p text into a widget as the user would, one key press.
void type( QWidget* widget, const QString& text )
{
    QKeyEvent event( QEvent::KeyPress, 0, Qt::NoModifier, text );
    QApplication::sendEvent( widget, &event );
}

/// Give a widget the focus as far as the panel can tell.
void focusIn( QWidget* widget )
{
    QFocusEvent event( QEvent::FocusIn, Qt::TabFocusReason );
    QApplication::sendEvent( widget, &event );
}

/// Counts how often a dialog was closed, whichever way.
struct ClosedCounter {
    explicit ClosedCounter( QDialog& dialog )
    {
        QObject::connect( &dialog, &QDialog::finished, [ this ]( int ) { ++count; } );
    }
    int count = 0;
};

const QString kEmoji = QString::fromUtf8( "\xF0\x9F\x98\x80" );

/// The footer values of @p entries on @p log, after saving and loading the
/// rules as the plugin does.
QMap<QString, QString> footerValues( const QList<FooterEntry>& entries, const QByteArray& log )
{
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    REQUIRE( FooterConfig::saveEntries( dir.path(), entries ) );
    const auto path = dir.path() + "/sample.log";
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( log ) == log.size() );
    file.close();
    return FooterScanner::scan( path, FooterConfig::loadEntries( dir.path() ) );
}

const QByteArray kSampleLog = "2026-09-30 10:00:00 boot\n"
                              "2026-09-30 10:00:01 [car] VIN:  WVWZZZ1KZAW000001 model=Golf\n"
                              "2026-09-30 10:00:02 Model name: Golf GTI 2.0  \n"
                              "2026-09-30 10:00:03 user=Jane Doe, id=7; role=admin\n"
                              "2026-09-30 10:00:04 cost($) 12.50 EUR\n";

} // namespace

SCENARIO( "FooterEditor starts a new rule in simple mode", "[footereditor][simplemode]" )
{
    GIVEN( "an editor with a new rule" )
    {
        FooterEditor editor( {} );
        SimpleUi ui( editor );
        ui.addRule->click();

        THEN( "it is in simple mode, with nothing to match yet" )
        {
            REQUIRE( ui.inSimpleMode() );
            REQUIRE( ui.advanced->isEnabled() );
            REQUIRE( ui.textBefore->text().isEmpty() );
            REQUIRE( ui.shownValueEnd() == ValueEnd::Whitespace );
            REQUIRE_FALSE( ui.endCharacter->isEnabled() );
            REQUIRE( ui.linePattern->text().isEmpty() );
            REQUIRE( ui.canAccept() );
        }

        WHEN( "filling in the key and the text before the value" )
        {
            ui.key->setText( "VIN" );
            ui.textBefore->setText( "VIN:" );

            THEN( "the rule gets the generated patterns, shown read-only" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 1 );
                REQUIRE( entries[ 0 ].key == "VIN" );
                REQUIRE( entries[ 0 ].linePattern == "VIN:\\s*(\\S+)" );
                REQUIRE( entries[ 0 ].valuePattern.isEmpty() );
                REQUIRE( ui.linePattern->text() == entries[ 0 ].linePattern );
                REQUIRE( ui.valuePattern->text().isEmpty() );
                REQUIRE( ui.listPattern( 0 ) == entries[ 0 ].linePattern );
                REQUIRE( ui.canAccept() );
            }

            THEN( "the footer shows the value from a sample log" )
            {
                const auto values = footerValues( editor.entries(), kSampleLog );
                REQUIRE( values == QMap<QString, QString>{ { "VIN", "WVWZZZ1KZAW000001" } } );
            }
        }

        WHEN( "the value ends at the end of the line" )
        {
            ui.key->setText( "Model" );
            ui.textBefore->setText( "Model name:" );
            ui.chooseValueEnd( ValueEnd::EndOfLine );

            THEN( "the value is the rest of the line" )
            {
                REQUIRE( editor.entries()[ 0 ].linePattern == "Model name:\\s*(.*\\S)" );
                REQUIRE( footerValues( editor.entries(), kSampleLog ).value( "Model" )
                         == "Golf GTI 2.0" );
            }
        }

        WHEN( "the value ends at a character" )
        {
            ui.key->setText( "User" );
            ui.textBefore->setText( "user=" );
            ui.chooseValueEnd( ValueEnd::Character );

            THEN( "the character can be entered, and is a comma to start with" )
            {
                REQUIRE( ui.endCharacter->isEnabled() );
                REQUIRE( ui.endCharacter->text() == "," );
                REQUIRE( footerValues( editor.entries(), kSampleLog ).value( "User" )
                         == "Jane Doe" );
            }

            AND_WHEN( "another character is entered" )
            {
                ui.endCharacter->setText( ";" );

                THEN( "the value ends there" )
                {
                    REQUIRE( editor.entries()[ 0 ].linePattern == "user=\\s*([^;]*[^;\\s])" );
                    REQUIRE( footerValues( editor.entries(), kSampleLog ).value( "User" )
                             == "Jane Doe, id=7" );
                }
            }

            AND_WHEN( "the character is removed" )
            {
                ui.endCharacter->setText( "" );

                THEN( "the field says what is missing, and the rules cannot be saved" )
                {
                    REQUIRE( ui.endCharacterMarked() );
                    REQUIRE( ui.endCharacterProblem->text()
                             == "Enter the character the value ends at" );
                    REQUIRE_FALSE( ui.canAccept() );
                    REQUIRE( ui.textBefore->text() == "user=" );
                    REQUIRE( editor.entries()[ 0 ].linePattern.isEmpty() );
                }

                AND_WHEN( "another rule is selected, and this one again" )
                {
                    ui.addRule->click();
                    REQUIRE( ui.textBefore->text().isEmpty() );
                    REQUIRE_FALSE( ui.endCharacterMarked() );
                    ui.select( 0 );

                    THEN( "the typed fields and the problem are still there" )
                    {
                        REQUIRE( ui.inSimpleMode() );
                        REQUIRE( ui.key->text() == "User" );
                        REQUIRE( ui.textBefore->text() == "user=" );
                        REQUIRE( ui.shownValueEnd() == ValueEnd::Character );
                        REQUIRE( ui.endCharacter->text().isEmpty() );
                        REQUIRE( ui.endCharacterMarked() );
                        REQUIRE_FALSE( ui.canAccept() );
                    }

                    AND_WHEN( "the character is entered" )
                    {
                        ui.endCharacter->setText( ";" );

                        THEN( "the rule gets its pattern and can be saved" )
                        {
                            REQUIRE( editor.entries()[ 0 ].linePattern
                                     == "user=\\s*([^;]*[^;\\s])" );
                            REQUIRE_FALSE( ui.endCharacterMarked() );
                            REQUIRE( ui.canAccept() );
                        }
                    }
                }

                AND_WHEN( "switching to advanced and back" )
                {
                    ui.advanced->click();
                    REQUIRE( ui.inAdvancedMode() );
                    REQUIRE( ui.canAccept() );
                    ui.advanced->click();

                    THEN( "the typed fields and the problem are back" )
                    {
                        REQUIRE( ui.inSimpleMode() );
                        REQUIRE( ui.textBefore->text() == "user=" );
                        REQUIRE( ui.shownValueEnd() == ValueEnd::Character );
                        REQUIRE( ui.endCharacterMarked() );
                        REQUIRE_FALSE( ui.canAccept() );
                    }
                }
            }

            AND_WHEN( "a tab is entered as the character" )
            {
                ui.endCharacter->setText( "\t" );

                THEN( "it is a problem, and the rules cannot be saved" )
                {
                    REQUIRE( ui.endCharacterMarked() );
                    REQUIRE_FALSE( ui.canAccept() );
                    REQUIRE( editor.entries()[ 0 ].linePattern.isEmpty() );
                }
            }

            AND_WHEN( "NUL is entered as the character" )
            {
                ui.endCharacter->setText( QString( QChar() ) );

                THEN( "it is a problem, and the rules cannot be saved" )
                {
                    REQUIRE( ui.endCharacterMarked() );
                    REQUIRE_FALSE( ui.canAccept() );
                }
            }

            AND_WHEN( "an emoji is typed as the character" )
            {
                ui.endCharacter->clear();
                type( ui.endCharacter, kEmoji );

                THEN( "it is taken whole and ends the value" )
                {
                    REQUIRE( ui.endCharacter->text() == kEmoji );
                    REQUIRE_FALSE( ui.endCharacterMarked() );
                    REQUIRE( ui.canAccept() );
                    REQUIRE( footerValues( editor.entries(),
                                           ( "user=Jane" + kEmoji + " Doe\n" ).toUtf8() )
                                 .value( "User" )
                             == "Jane" );
                }

                THEN( "no second character can be typed" )
                {
                    type( ui.endCharacter, "x" );
                    REQUIRE( ui.endCharacter->text() == kEmoji );
                }
            }

            AND_WHEN( "two characters are typed" )
            {
                ui.endCharacter->clear();
                type( ui.endCharacter, ";" );
                type( ui.endCharacter, "x" );

                THEN( "only the first is taken" )
                {
                    REQUIRE( ui.endCharacter->text() == ";" );
                }
            }
        }

        WHEN( "the text has characters special in regular expressions" )
        {
            ui.key->setText( "Cost" );
            ui.textBefore->setText( "cost($)" );

            THEN( "they are escaped and matched literally" )
            {
                REQUIRE( editor.entries()[ 0 ].linePattern == "cost\\(\\$\\)\\s*(\\S+)" );
                REQUIRE( footerValues( editor.entries(), kSampleLog ).value( "Cost" ) == "12.50" );
            }
        }

        WHEN( "the text is edited and Escape is pressed" )
        {
            ui.textBefore->setText( "VIN:" );
            press( ui.textBefore, Qt::Key_Return );
            ui.textBefore->setText( "VIN" );
            press( ui.textBefore, Qt::Key_Escape );

            THEN( "the text and the patterns go back to the confirmed text" )
            {
                REQUIRE( ui.textBefore->text() == "VIN:" );
                REQUIRE( editor.entries()[ 0 ].linePattern == "VIN:\\s*(\\S+)" );
            }
        }
    }
}

SCENARIO( "FooterEditor switches rules between simple and advanced mode",
          "[footereditor][simplemode]" )
{
    GIVEN( "a simple rule and an advanced one" )
    {
        const FooterEntry simple{ "VIN", "VIN:\\s*(\\S+)", "", true, {} };
        const FooterEntry advancedRule{ "Build", "build=(\\d+)", "", true, {} };
        FooterEditor editor( { simple, advancedRule } );
        SimpleUi ui( editor );

        THEN( "the simple rule opens in simple mode, with its fields" )
        {
            REQUIRE( ui.inSimpleMode() );
            REQUIRE( ui.textBefore->text() == "VIN:" );
            REQUIRE( ui.shownValueEnd() == ValueEnd::Whitespace );
            REQUIRE( ui.linePattern->text() == simple.linePattern );
        }

        THEN( "the advanced rule opens in advanced mode, and cannot be made simple" )
        {
            ui.select( 1 );
            REQUIRE( ui.inAdvancedMode() );
            REQUIRE_FALSE( ui.advanced->isEnabled() );
            REQUIRE( ui.linePattern->text() == advancedRule.linePattern );
        }

        WHEN( "switching the simple rule to advanced" )
        {
            ui.advanced->click();

            THEN( "the patterns are kept and can be edited" )
            {
                REQUIRE( ui.inAdvancedMode() );
                REQUIRE( ui.linePattern->text() == simple.linePattern );
                REQUIRE( editor.entries()[ 0 ].linePattern == simple.linePattern );
                REQUIRE( editor.entries()[ 0 ].valuePattern.isEmpty() );
            }

            THEN( "switching back is offered while the patterns are unchanged" )
            {
                REQUIRE( ui.advanced->isEnabled() );
            }

            AND_WHEN( "switching back" )
            {
                ui.advanced->click();

                THEN( "the rule is simple again, unchanged" )
                {
                    REQUIRE( ui.inSimpleMode() );
                    REQUIRE( ui.textBefore->text() == "VIN:" );
                    REQUIRE( editor.entries()[ 0 ].linePattern == simple.linePattern );
                }
            }

            AND_WHEN( "the line pattern is edited beyond the simple form" )
            {
                ui.linePattern->setText( "VIN:\\s+(\\S+)" );

                THEN( "switching back is not offered, and the rule has the edited pattern" )
                {
                    REQUIRE( ui.inAdvancedMode() );
                    REQUIRE_FALSE( ui.advanced->isEnabled() );
                    REQUIRE( editor.entries()[ 0 ].linePattern == "VIN:\\s+(\\S+)" );
                }

                THEN( "clicking the disabled switch does nothing" )
                {
                    ui.advanced->click();
                    REQUIRE( ui.inAdvancedMode() );
                }

                AND_WHEN( "selecting another rule and the edited one again" )
                {
                    ui.select( 1 );
                    ui.select( 0 );

                    THEN( "it stays in advanced mode" )
                    {
                        REQUIRE( ui.inAdvancedMode() );
                        REQUIRE( ui.linePattern->text() == "VIN:\\s+(\\S+)" );
                    }
                }

                AND_WHEN( "the pattern is given the simple form again" )
                {
                    ui.linePattern->setText( "id=\\s*(.*\\S)" );

                    THEN( "switching back is offered, and fills in the simple fields" )
                    {
                        REQUIRE( ui.advanced->isEnabled() );
                        ui.advanced->click();
                        REQUIRE( ui.inSimpleMode() );
                        REQUIRE( ui.textBefore->text() == "id=" );
                        REQUIRE( ui.shownValueEnd() == ValueEnd::EndOfLine );
                        REQUIRE( editor.entries()[ 0 ].linePattern == "id=\\s*(.*\\S)" );
                    }
                }
            }

            AND_WHEN( "a value pattern is added" )
            {
                ui.valuePattern->setText( "(\\S+)$" );

                THEN( "switching back is not offered" )
                {
                    REQUIRE_FALSE( ui.advanced->isEnabled() );
                    REQUIRE( editor.entries()[ 0 ].valuePattern == "(\\S+)$" );
                }
            }

            AND_WHEN( "the rule is disabled and enabled in the list" )
            {
                REQUIRE( ui.model->setData( ui.model->index( 0, RuleListModel::KeyColumn ),
                                            Qt::Unchecked, Qt::CheckStateRole ) );
                REQUIRE( ui.model->setData( ui.model->index( 0, RuleListModel::KeyColumn ),
                                            Qt::Checked, Qt::CheckStateRole ) );

                THEN( "the panel stays in advanced mode" )
                {
                    REQUIRE( ui.inAdvancedMode() );
                }
            }
        }

        WHEN( "the advanced rule's pattern gets the simple form" )
        {
            ui.select( 1 );
            ui.linePattern->setText( "build=\\s*([^,]*[^,\\s])" );

            THEN( "it can be switched to simple mode, with its fields" )
            {
                REQUIRE( ui.advanced->isEnabled() );
                ui.advanced->click();
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( ui.textBefore->text() == "build=" );
                REQUIRE( ui.shownValueEnd() == ValueEnd::Character );
                REQUIRE( ui.endCharacter->text() == "," );
            }
        }

        WHEN( "editing the simple rule's text" )
        {
            ui.textBefore->setText( "Vehicle ID:" );

            THEN( "the list shows the generated pattern, and the other rule is unchanged" )
            {
                REQUIRE( ui.listPattern( 0 ) == "Vehicle ID:\\s*(\\S+)" );
                REQUIRE( editor.entries()[ 1 ].linePattern == advancedRule.linePattern );
            }
        }
    }
}

SCENARIO( "FooterEditor opens rules of existing configs in the right mode",
          "[footereditor][simplemode]" )
{
    GIVEN( "rules with each simple form, and ones that only look simple" )
    {
        FooterEditor editor( {
            { "A", "a:\\s*(\\S+)", "", true, {} },
            { "B", "b:\\s*(.*\\S)", "", false, {} },
            { "C", "c:\\s*([^;]*[^;\\s])", "", true, { ValueMapping{ "x", "y" } } },
            { "D", "d\\:\\s*(\\S+)", "", true, {} }, // needless escape
            { "E", "e:\\s*(\\S+)", "(\\S+)", true, {} },
            { "F", "", "", true, {} },
        } );
        SimpleUi ui( editor );

        THEN( "each opens in the mode its patterns have" )
        {
            const QList<bool> simple{ true, true, true, false, false, true };
            for ( int row = 0; row < simple.size(); ++row ) {
                INFO( "row " << row );
                ui.select( row );
                REQUIRE( ( simple[ row ] ? ui.inSimpleMode() : ui.inAdvancedMode() ) );
            }
        }

        THEN( "simple ones show their fields" )
        {
            ui.select( 1 );
            REQUIRE( ui.textBefore->text() == "b:" );
            REQUIRE( ui.shownValueEnd() == ValueEnd::EndOfLine );
            ui.select( 2 );
            REQUIRE( ui.textBefore->text() == "c:" );
            REQUIRE( ui.shownValueEnd() == ValueEnd::Character );
            REQUIRE( ui.endCharacter->text() == ";" );
        }
    }
}

SCENARIO( "FooterEditor never reverts the read-only patterns of a simple rule",
          "[footereditor][simplemode]" )
{
    GIVEN( "a simple rule whose text was changed" )
    {
        FooterEditor editor( { { "VIN", "VIN:\\s*(\\S+)", "", true, {} } } );
        SimpleUi ui( editor );
        focusIn( ui.linePattern );
        ui.textBefore->setText( "ID:" );
        REQUIRE( ui.linePattern->text() == "ID:\\s*(\\S+)" );
        const int validations = editor.ruleValidations();

        WHEN( "pressing Escape in the read-only pattern fields" )
        {
            press( ui.linePattern, Qt::Key_Escape );
            press( ui.valuePattern, Qt::Key_Escape );

            THEN( "nothing changes, and the rule was not edited" )
            {
                REQUIRE( ui.linePattern->text() == "ID:\\s*(\\S+)" );
                REQUIRE( editor.entries()[ 0 ].linePattern == "ID:\\s*(\\S+)" );
                REQUIRE( editor.ruleValidations() == validations );
                REQUIRE( ui.inSimpleMode() );
            }
        }
    }

    GIVEN( "a simple rule switched to advanced, with the focus in its line pattern" )
    {
        FooterEditor editor( { { "VIN", "VIN:\\s*(\\S+)", "", true, {} } } );
        SimpleUi ui( editor );
        ui.advanced->click();
        focusIn( ui.linePattern );

        WHEN( "Advanced is switched off, the text changed, and Escape pressed in the pattern" )
        {
            ui.advanced->click();
            ui.textBefore->setText( "ID:" );
            press( ui.linePattern, Qt::Key_Escape );

            THEN( "the pattern still matches the simple fields" )
            {
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( ui.linePattern->text() == "ID:\\s*(\\S+)" );
                REQUIRE( editor.entries()[ 0 ].linePattern == "ID:\\s*(\\S+)" );
            }

            AND_WHEN( "Advanced is switched on again, the pattern edited, and Escape pressed" )
            {
                ui.advanced->click();
                ui.linePattern->setText( "ID:(\\d+)" );
                press( ui.linePattern, Qt::Key_Escape );

                THEN( "the pattern goes back to what it was when it became editable" )
                {
                    REQUIRE( ui.linePattern->text() == "ID:\\s*(\\S+)" );
                    REQUIRE( editor.entries()[ 0 ].linePattern == "ID:\\s*(\\S+)" );
                }
            }
        }
    }
}

SCENARIO( "FooterEditor keeps the dialog open for Return and Escape in the mode controls",
          "[footereditor][simplemode]" )
{
    GIVEN( "a shown editor with a simple rule" )
    {
        FooterEditor editor( { { "VIN", "VIN:\\s*(\\S+)", "", true, {} } } );
        SimpleUi ui( editor );
        editor.show();
        ClosedCounter closed( editor );

        WHEN( "pressing Return in the value end and the Advanced switch" )
        {
            press( ui.valueEnd, Qt::Key_Return );
            press( ui.valueEnd, Qt::Key_Enter );
            press( ui.advanced, Qt::Key_Return );

            THEN( "the dialog stays open and nothing changes" )
            {
                REQUIRE( closed.count == 0 );
                REQUIRE( editor.isVisible() );
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( editor.entries()[ 0 ].linePattern == "VIN:\\s*(\\S+)" );
            }
        }

        WHEN( "choosing another value end and pressing Escape" )
        {
            focusIn( ui.valueEnd );
            ui.chooseValueEnd( ValueEnd::EndOfLine );
            REQUIRE( editor.entries()[ 0 ].linePattern == "VIN:\\s*(.*\\S)" );
            press( ui.valueEnd, Qt::Key_Escape );

            THEN( "the choice is reverted, and the dialog stays open" )
            {
                REQUIRE( closed.count == 0 );
                REQUIRE( ui.shownValueEnd() == ValueEnd::Whitespace );
                REQUIRE( editor.entries()[ 0 ].linePattern == "VIN:\\s*(\\S+)" );
            }
        }

        WHEN( "confirming another value end with Return and pressing Escape" )
        {
            focusIn( ui.valueEnd );
            ui.chooseValueEnd( ValueEnd::EndOfLine );
            press( ui.valueEnd, Qt::Key_Return );
            press( ui.valueEnd, Qt::Key_Escape );

            THEN( "the confirmed choice stays" )
            {
                REQUIRE( closed.count == 0 );
                REQUIRE( ui.shownValueEnd() == ValueEnd::EndOfLine );
            }
        }

        WHEN( "switching to advanced and pressing Escape in the switch" )
        {
            focusIn( ui.advanced );
            ui.advanced->click();
            REQUIRE( ui.inAdvancedMode() );
            press( ui.advanced, Qt::Key_Escape );

            THEN( "the switch is reverted, and the dialog stays open" )
            {
                REQUIRE( closed.count == 0 );
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( editor.entries()[ 0 ].linePattern == "VIN:\\s*(\\S+)" );
            }
        }
    }
}

SCENARIO( "FooterEditor keeps an unfinished simple rule out of the saved rules",
          "[footereditor][simplemode]" )
{
    GIVEN( "a simple rule whose end character was removed, between two others" )
    {
        FooterEditor editor( {
            { "User", "user=\\s*([^,]*[^,\\s])", "", true, { ValueMapping{ "a", "b" } } },
            { "A", "a:\\s*(\\S+)", "", true, {} },
            { "B", "b=(\\d+)", "", false, {} },
        } );
        SimpleUi ui( editor );
        ui.endCharacter->setText( "" );
        ui.textBefore->setText( "name=" );
        REQUIRE( ui.endCharacterMarked() );
        REQUIRE_FALSE( ui.canAccept() );

        THEN( "the rules have nothing of it beyond their last valid patterns" )
        {
            const QList<FooterEntry> expected{
                { "User", "", "", true, { ValueMapping{ "a", "b" } } },
                { "A", "a:\\s*(\\S+)", "", true, {} },
                { "B", "b=(\\d+)", "", false, {} },
            };
            const auto entries = editor.entries();
            REQUIRE( entries.size() == expected.size() );
            for ( int i = 0; i < entries.size(); ++i ) {
                INFO( "rule " << i );
                REQUIRE( entries[ i ].key == expected[ i ].key );
                REQUIRE( entries[ i ].linePattern == expected[ i ].linePattern );
                REQUIRE( entries[ i ].valuePattern == expected[ i ].valuePattern );
                REQUIRE( entries[ i ].enabled == expected[ i ].enabled );
                REQUIRE( entries[ i ].mappings.size() == expected[ i ].mappings.size() );
            }

            AND_THEN( "a save and an export made now are those of the rules alone" )
            {
                QTemporaryDir withDraft;
                QTemporaryDir plain;
                REQUIRE( withDraft.isValid() );
                REQUIRE( plain.isValid() );
                REQUIRE( FooterConfig::saveEntries( withDraft.path(), entries ) );
                REQUIRE( FooterConfig::saveEntries( plain.path(), expected ) );
                REQUIRE( FooterConfig::exportToJson( withDraft.path() + "/r.json", entries ) );
                REQUIRE( FooterConfig::exportToJson( plain.path() + "/r.json", expected ) );
                for ( const auto* name : { "/custom_footer.ini", "/r.json" } ) {
                    QFile saved( withDraft.path() + name );
                    QFile reference( plain.path() + name );
                    REQUIRE( saved.open( QIODevice::ReadOnly ) );
                    REQUIRE( reference.open( QIODevice::ReadOnly ) );
                    const auto bytes = saved.readAll();
                    REQUIRE( bytes == reference.readAll() );
                    REQUIRE_FALSE( bytes.contains( "name=" ) );
                }
            }
        }

        WHEN( "the rule is dragged to the end of the list" )
        {
            auto* model = ui.model;
            const std::unique_ptr<QMimeData> data(
                model->mimeData( { model->index( 0, RuleListModel::KeyColumn ) } ) );
            REQUIRE( data );
            REQUIRE( model->dropMimeData( data.get(), Qt::MoveAction, 3, 0, QModelIndex() ) );
            REQUIRE( model->entry( 2 ).key == "User" );

            THEN( "the unfinished rule and its problem moved with it" )
            {
                REQUIRE( ui.list->currentIndex().row() == 2 );
                REQUIRE( ui.textBefore->text() == "name=" );
                REQUIRE( ui.endCharacterMarked() );
                REQUIRE_FALSE( model->problems( 2 ).isEmpty() );
                REQUIRE( model->problems( 0 ).isEmpty() );
                REQUIRE_FALSE( ui.canAccept() );
            }

            AND_WHEN( "the rules now above it are selected, and it again" )
            {
                ui.select( 0 );
                REQUIRE( ui.key->text() == "A" );
                REQUIRE_FALSE( ui.endCharacterMarked() );
                ui.select( 1 );
                REQUIRE_FALSE( ui.endCharacterMarked() );
                ui.select( 2 );

                THEN( "it shows its typed fields and problem" )
                {
                    REQUIRE( ui.inSimpleMode() );
                    REQUIRE( ui.key->text() == "User" );
                    REQUIRE( ui.textBefore->text() == "name=" );
                    REQUIRE( ui.shownValueEnd() == ValueEnd::Character );
                    REQUIRE( ui.endCharacter->text().isEmpty() );
                    REQUIRE( ui.endCharacterMarked() );
                }
            }
        }
    }
}
