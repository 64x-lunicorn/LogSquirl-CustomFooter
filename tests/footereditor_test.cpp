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
 * @file footereditor_test.cpp
 * @brief BDD tests for the FooterEditor rule list and detail panel.
 */

#include <catch2/catch.hpp>

#include "footereditor.h"
#include "rulelistmodel.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QToolButton>

using namespace custom_footer;

namespace {

/// Test access to the editor's widgets, found by object name.
struct EditorUi {
    template <typename Widget>
    static Widget* find( FooterEditor& editor, const char* name )
    {
        auto* widget = editor.findChild<Widget*>( name );
        REQUIRE( widget );
        return widget;
    }

    explicit EditorUi( FooterEditor& editor )
        : list( find<QTableView>( editor, "ruleList" ) )
        , model( qobject_cast<RuleListModel*>( list->model() ) )
        , panel( find<QGroupBox>( editor, "ruleDetailPanel" ) )
        , enabled( find<QCheckBox>( editor, "ruleEnabledCheck" ) )
        , key( find<QLineEdit>( editor, "keyEdit" ) )
        , linePattern( find<QLineEdit>( editor, "linePatternEdit" ) )
        , valuePattern( find<QLineEdit>( editor, "valuePatternEdit" ) )
        , keyProblem( find<QLabel>( editor, "keyProblem" ) )
        , linePatternProblem( find<QLabel>( editor, "linePatternProblem" ) )
        , valuePatternProblem( find<QLabel>( editor, "valuePatternProblem" ) )
        , mappings( find<QTableWidget>( editor, "mappingTable" ) )
        , addMapping( find<QToolButton>( editor, "addMappingButton" ) )
        , removeMapping( find<QToolButton>( editor, "removeMappingButton" ) )
        , addRule( find<QToolButton>( editor, "addRuleButton" ) )
        , removeRule( find<QToolButton>( editor, "removeRuleButton" ) )
        , moveUp( find<QToolButton>( editor, "moveUpButton" ) )
        , moveDown( find<QToolButton>( editor, "moveDownButton" ) )
        , problems( find<QLabel>( editor, "problemLabel" ) )
        , buttons( editor.findChild<QDialogButtonBox*>() )
    {
        REQUIRE( model );
        REQUIRE( buttons );
    }

    void select( int row ) const
    {
        list->setCurrentIndex( model->index( row, RuleListModel::KeyColumn ) );
    }

    int currentRow() const
    {
        return list->currentIndex().isValid() ? list->currentIndex().row() : -1;
    }

    /// What the list shows for a rule.
    QString listKey( int row ) const
    {
        return model->index( row, RuleListModel::KeyColumn ).data().toString();
    }
    QString listPattern( int row ) const
    {
        return model->index( row, RuleListModel::LinePatternColumn ).data().toString();
    }
    bool listChecked( int row ) const
    {
        return model->index( row, RuleListModel::KeyColumn ).data( Qt::CheckStateRole ).toInt()
               == Qt::Checked;
    }
    void setListChecked( int row, bool checked ) const
    {
        REQUIRE( model->setData( model->index( row, RuleListModel::KeyColumn ),
                                 checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole ) );
    }
    /// Whether the list marks a rule as keeping the rules from being saved.
    bool listMarked( int row ) const
    {
        return !model->problems( row ).isEmpty()
               && model->index( row, RuleListModel::KeyColumn )
                      .data( Qt::BackgroundRole )
                      .isValid();
    }

    /// Whether a field is marked, with the reason next to it.
    static bool marked( const QLineEdit* field, const QLabel* reason )
    {
        return !field->toolTip().isEmpty() && reason->text() == field->toolTip()
               && !reason->isHidden();
    }
    static bool unmarked( const QLineEdit* field, const QLabel* reason )
    {
        return field->toolTip().isEmpty() && reason->isHidden();
    }

    QStringList shownMappingPatterns() const
    {
        QStringList patterns;
        for ( int i = 0; i < mappings->rowCount(); ++i ) {
            patterns.append( mappings->item( i, 0 )->text() );
        }
        return patterns;
    }

    bool canAccept() const
    {
        return buttons->button( QDialogButtonBox::Ok )->isEnabled()
               && buttons->button( QDialogButtonBox::Apply )->isEnabled();
    }

    QTableView* list;
    RuleListModel* model;
    QGroupBox* panel;
    QCheckBox* enabled;
    QLineEdit* key;
    QLineEdit* linePattern;
    QLineEdit* valuePattern;
    QLabel* keyProblem;
    QLabel* linePatternProblem;
    QLabel* valuePatternProblem;
    QTableWidget* mappings;
    QToolButton* addMapping;
    QToolButton* removeMapping;
    QToolButton* addRule;
    QToolButton* removeRule;
    QToolButton* moveUp;
    QToolButton* moveDown;
    QLabel* problems;
    QDialogButtonBox* buttons;
};

FooterEntry entryWithMapping( const QString& key )
{
    return { key, key + "=(\\S+)", "", true, { ValueMapping{ key + "-raw", key + "-shown" } } };
}

QList<FooterEntry> threeRules()
{
    return { entryWithMapping( "A" ), entryWithMapping( "B" ), entryWithMapping( "C" ) };
}

bool sameEntry( const FooterEntry& a, const FooterEntry& b )
{
    if ( a.key != b.key || a.linePattern != b.linePattern || a.valuePattern != b.valuePattern
         || a.enabled != b.enabled || a.mappings.size() != b.mappings.size() ) {
        return false;
    }
    for ( int i = 0; i < a.mappings.size(); ++i ) {
        if ( a.mappings[ i ].pattern != b.mappings[ i ].pattern
             || a.mappings[ i ].displayValue != b.mappings[ i ].displayValue ) {
            return false;
        }
    }
    return true;
}

} // namespace

SCENARIO( "FooterEditor shows the selected rule in the detail panel", "[footereditor]" )
{
    GIVEN( "rules with different fields" )
    {
        const QList<FooterEntry> rules
            = { entryWithMapping( "A" ),
                { "B",
                  "build (\\d+)",
                  ":\\s*(\\d+)",
                  false,
                  { ValueMapping{ "0", "none" }, ValueMapping{ "1", "one" } } } };
        FooterEditor editor( rules );
        EditorUi ui( editor );

        THEN( "the list has a row per rule with its enabled state, key and line pattern" )
        {
            REQUIRE( ui.model->rowCount() == 2 );
            REQUIRE( ui.listKey( 0 ) == "A" );
            REQUIRE( ui.listPattern( 0 ) == "A=(\\S+)" );
            REQUIRE( ui.listChecked( 0 ) );
            REQUIRE( ui.listKey( 1 ) == "B" );
            REQUIRE( ui.listPattern( 1 ) == "build (\\d+)" );
            REQUIRE_FALSE( ui.listChecked( 1 ) );
        }

        THEN( "long patterns are elided in the list rather than wrapped" )
        {
            REQUIRE( ui.list->textElideMode() == Qt::ElideRight );
            REQUIRE_FALSE( ui.list->wordWrap() );
        }

        THEN( "the first rule is selected and shown" )
        {
            REQUIRE( ui.currentRow() == 0 );
            REQUIRE( ui.panel->isEnabled() );
            REQUIRE( ui.key->text() == "A" );
            REQUIRE( ui.shownMappingPatterns() == QStringList{ "A-raw" } );
        }

        WHEN( "selecting the second rule" )
        {
            ui.select( 1 );

            THEN( "the panel shows all its fields and mappings" )
            {
                REQUIRE_FALSE( ui.enabled->isChecked() );
                REQUIRE( ui.key->text() == "B" );
                REQUIRE( ui.linePattern->text() == "build (\\d+)" );
                REQUIRE( ui.valuePattern->text() == ":\\s*(\\d+)" );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "0", "1" } );
                REQUIRE( ui.mappings->item( 1, 1 )->text() == "one" );
            }

            AND_WHEN( "selecting the first one again" )
            {
                ui.select( 0 );

                THEN( "only showing the rules changed none of them" )
                {
                    REQUIRE( ui.key->text() == "A" );
                    const auto entries = editor.entries();
                    REQUIRE( entries.size() == 2 );
                    REQUIRE( sameEntry( entries[ 0 ], rules[ 0 ] ) );
                    REQUIRE( sameEntry( entries[ 1 ], rules[ 1 ] ) );
                }
            }
        }
    }

    GIVEN( "no rules" )
    {
        FooterEditor editor( {} );
        EditorUi ui( editor );

        THEN( "the panel is empty and disabled, and there is nothing to remove or move" )
        {
            REQUIRE( ui.currentRow() == -1 );
            REQUIRE_FALSE( ui.panel->isEnabled() );
            REQUIRE( ui.key->text().isEmpty() );
            REQUIRE( ui.mappings->rowCount() == 0 );
            REQUIRE_FALSE( ui.removeRule->isEnabled() );
            REQUIRE_FALSE( ui.moveUp->isEnabled() );
            REQUIRE_FALSE( ui.moveDown->isEnabled() );
            REQUIRE( ui.canAccept() );
        }

        WHEN( "adding a rule" )
        {
            ui.addRule->click();

            THEN( "a new, enabled, empty rule is selected in the panel" )
            {
                REQUIRE( ui.model->rowCount() == 1 );
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.panel->isEnabled() );
                REQUIRE( ui.enabled->isChecked() );
                REQUIRE( ui.key->text().isEmpty() );
                REQUIRE( ui.listChecked( 0 ) );
                REQUIRE( ui.canAccept() );
            }

            AND_WHEN( "filling it in through the panel" )
            {
                ui.key->setText( "VIN" );
                ui.linePattern->setText( "VIN:\\s+(\\S+)" );

                THEN( "the rule is stored" )
                {
                    const auto entries = editor.entries();
                    REQUIRE( entries.size() == 1 );
                    REQUIRE( entries[ 0 ].key == "VIN" );
                    REQUIRE( entries[ 0 ].linePattern == "VIN:\\s+(\\S+)" );
                    REQUIRE( ui.canAccept() );
                }
            }
        }
    }
}

SCENARIO( "FooterEditor updates the list while a rule is edited in the panel", "[footereditor]" )
{
    GIVEN( "three rules and the second one selected" )
    {
        FooterEditor editor( threeRules() );
        EditorUi ui( editor );
        ui.select( 1 );

        WHEN( "editing its key" )
        {
            ui.key->setText( "Build" );

            THEN( "the list row shows the new key at once" )
            {
                REQUIRE( ui.listKey( 1 ) == "Build" );
                REQUIRE( editor.entries()[ 1 ].key == "Build" );
            }
        }

        WHEN( "clearing its key and line pattern" )
        {
            ui.key->setText( "" );
            ui.linePattern->setText( "" );

            THEN( "the list says it has no key, and saves it without one" )
            {
                REQUIRE( ui.listKey( 1 ) == "(no key)" );
                REQUIRE( ui.listPattern( 1 ).isEmpty() );
                REQUIRE( editor.entries()[ 1 ].key.isEmpty() );
            }
        }

        WHEN( "editing its line pattern" )
        {
            ui.linePattern->setText( "Build #(\\d+)" );

            THEN( "the list row shows the new pattern" )
            {
                REQUIRE( ui.listPattern( 1 ) == "Build #(\\d+)" );
                REQUIRE( editor.entries()[ 1 ].linePattern == "Build #(\\d+)" );
            }
        }

        WHEN( "editing its value pattern" )
        {
            ui.valuePattern->setText( "#(\\d+)" );

            THEN( "the rule has it" )
            {
                REQUIRE( editor.entries()[ 1 ].valuePattern == "#(\\d+)" );
            }
        }

        WHEN( "disabling it in the panel" )
        {
            ui.enabled->setChecked( false );

            THEN( "the list row is unchecked" )
            {
                REQUIRE_FALSE( ui.listChecked( 1 ) );
                REQUIRE_FALSE( editor.entries()[ 1 ].enabled );
            }
        }

        WHEN( "disabling it in the list" )
        {
            ui.setListChecked( 1, false );

            THEN( "the panel shows it disabled, and keeps its other fields" )
            {
                REQUIRE_FALSE( ui.enabled->isChecked() );
                REQUIRE( ui.key->text() == "B" );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "B-raw" } );
                REQUIRE_FALSE( editor.entries()[ 1 ].enabled );
            }
        }

        WHEN( "disabling another rule in the list" )
        {
            ui.setListChecked( 2, false );

            THEN( "the panel still shows the selected rule" )
            {
                REQUIRE( ui.enabled->isChecked() );
                REQUIRE( ui.key->text() == "B" );
                REQUIRE_FALSE( editor.entries()[ 2 ].enabled );
            }
        }

        WHEN( "editing, adding and removing mappings" )
        {
            ui.mappings->item( 0, 1 )->setText( "B-edited" );
            ui.addMapping->click();
            ui.mappings->item( 1, 0 )->setText( "x" );
            ui.mappings->item( 1, 1 )->setText( "y" );
            ui.addMapping->click();
            ui.mappings->setCurrentCell( 0, 0 );
            ui.removeMapping->click();

            THEN( "the rule has the mappings as shown" )
            {
                const auto mappings = editor.entries()[ 1 ].mappings;
                REQUIRE( mappings.size() == 2 );
                REQUIRE( mappings[ 0 ].pattern == "x" );
                REQUIRE( mappings[ 0 ].displayValue == "y" );
                REQUIRE( mappings[ 1 ].pattern.isEmpty() );
                REQUIRE( mappings[ 1 ].displayValue.isEmpty() );
            }
        }

        WHEN( "editing every field" )
        {
            ui.key->setText( "K" );
            ui.linePattern->setText( "L" );
            ui.valuePattern->setText( "V" );
            ui.enabled->setChecked( false );
            ui.mappings->item( 0, 0 )->setText( "M" );

            THEN( "the other rules are unchanged" )
            {
                const auto entries = editor.entries();
                REQUIRE( sameEntry( entries[ 0 ], entryWithMapping( "A" ) ) );
                REQUIRE( sameEntry( entries[ 2 ], entryWithMapping( "C" ) ) );
            }
        }
    }
}

SCENARIO( "FooterEditor keeps mappings with their rule when rules are removed", "[footereditor]" )
{
    GIVEN( "three rules with one mapping each and the first rule selected" )
    {
        FooterEditor editor( threeRules() );
        EditorUi ui( editor );
        ui.select( 0 );
        REQUIRE( ui.shownMappingPatterns() == QStringList{ "A-raw" } );

        WHEN( "removing the first rule" )
        {
            ui.removeRule->click();

            THEN( "the remaining rules keep their own mappings" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 2 );
                REQUIRE( entries[ 0 ].key == "B" );
                REQUIRE( entries[ 0 ].mappings.size() == 1 );
                REQUIRE( entries[ 0 ].mappings[ 0 ].pattern == "B-raw" );
                REQUIRE( entries[ 1 ].key == "C" );
                REQUIRE( entries[ 1 ].mappings.size() == 1 );
                REQUIRE( entries[ 1 ].mappings[ 0 ].pattern == "C-raw" );
            }

            THEN( "the panel shows the newly selected rule" )
            {
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.panel->isEnabled() );
                REQUIRE( ui.key->text() == "B" );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "B-raw" } );
            }

            AND_WHEN( "selecting the other rule and back" )
            {
                ui.select( 1 );
                ui.select( 0 );

                THEN( "nothing was written over another rule" )
                {
                    const auto entries = editor.entries();
                    REQUIRE( entries[ 0 ].mappings[ 0 ].pattern == "B-raw" );
                    REQUIRE( entries[ 1 ].mappings[ 0 ].pattern == "C-raw" );
                }
            }
        }

        WHEN( "removing the last rule" )
        {
            ui.select( 2 );
            ui.removeRule->click();

            THEN( "the rule above becomes selected and shown" )
            {
                REQUIRE( ui.currentRow() == 1 );
                REQUIRE( ui.key->text() == "B" );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "B-raw" } );
            }
        }

        WHEN( "removing the middle rule" )
        {
            ui.select( 1 );
            ui.removeRule->click();

            THEN( "the next rule becomes selected and shown" )
            {
                REQUIRE( ui.currentRow() == 1 );
                REQUIRE( ui.key->text() == "C" );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "C-raw" } );
            }
        }
    }

    GIVEN( "two rules and the first one selected" )
    {
        FooterEditor editor( { entryWithMapping( "A" ), entryWithMapping( "B" ) } );
        EditorUi ui( editor );
        ui.select( 0 );

        WHEN( "removing it and editing the mapping of the remaining rule" )
        {
            ui.removeRule->click();
            ui.mappings->item( 0, 1 )->setText( "edited" );

            THEN( "the edit is kept" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 1 );
                REQUIRE( entries[ 0 ].key == "B" );
                REQUIRE( entries[ 0 ].mappings.size() == 1 );
                REQUIRE( entries[ 0 ].mappings[ 0 ].displayValue == "edited" );
            }
        }
    }

    GIVEN( "a single selected rule" )
    {
        FooterEditor editor( { entryWithMapping( "A" ) } );
        EditorUi ui( editor );
        ui.select( 0 );

        WHEN( "removing it" )
        {
            ui.removeRule->click();

            THEN( "the panel is empty and disabled" )
            {
                REQUIRE( editor.entries().isEmpty() );
                REQUIRE_FALSE( ui.panel->isEnabled() );
                REQUIRE( ui.key->text().isEmpty() );
                REQUIRE( ui.mappings->rowCount() == 0 );
                REQUIRE_FALSE( ui.removeRule->isEnabled() );
            }

            AND_WHEN( "editing the empty panel anyway" )
            {
                ui.key->setText( "ghost" );

                THEN( "no rule appears" )
                {
                    REQUIRE( editor.entries().isEmpty() );
                }
            }
        }
    }
}

SCENARIO( "FooterEditor keeps mappings with their rule when rules are moved", "[footereditor]" )
{
    GIVEN( "three rules and an edited mapping on the second one" )
    {
        FooterEditor editor( threeRules() );
        EditorUi ui( editor );
        ui.select( 1 );
        ui.mappings->item( 0, 1 )->setText( "B-edited" );

        WHEN( "moving it up" )
        {
            ui.moveUp->click();

            THEN( "the rule and its edited mapping move together" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries[ 0 ].key == "B" );
                REQUIRE( entries[ 0 ].mappings[ 0 ].displayValue == "B-edited" );
                REQUIRE( entries[ 1 ].key == "A" );
                REQUIRE( entries[ 1 ].mappings[ 0 ].displayValue == "A-shown" );
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.listKey( 0 ) == "B" );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "B-raw" } );
                REQUIRE_FALSE( ui.moveUp->isEnabled() );
            }

            AND_WHEN( "editing it in the panel after the move" )
            {
                ui.key->setText( "B2" );

                THEN( "the moved rule is edited, not the one now in its old place" )
                {
                    const auto entries = editor.entries();
                    REQUIRE( entries[ 0 ].key == "B2" );
                    REQUIRE( entries[ 1 ].key == "A" );
                }
            }
        }

        WHEN( "moving it down" )
        {
            ui.moveDown->click();

            THEN( "the rule and its edited mapping move together" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries[ 1 ].key == "C" );
                REQUIRE( entries[ 1 ].mappings[ 0 ].displayValue == "C-shown" );
                REQUIRE( entries[ 2 ].key == "B" );
                REQUIRE( entries[ 2 ].mappings[ 0 ].displayValue == "B-edited" );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( ui.key->text() == "B" );
                REQUIRE_FALSE( ui.moveDown->isEnabled() );
            }
        }
    }
}

SCENARIO( "FooterEditor keeps the detail panel live after an import", "[footereditor]" )
{
    GIVEN( "a selected rule" )
    {
        FooterEditor editor( { entryWithMapping( "A" ) } );
        EditorUi ui( editor );
        ui.select( 0 );

        WHEN( "rules are appended" )
        {
            editor.appendEntries( { entryWithMapping( "B" ) } );

            THEN( "the panel still shows and edits the selected rule" )
            {
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.panel->isEnabled() );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "A-raw" } );
                REQUIRE( ui.listKey( 1 ) == "B" );
                REQUIRE( ui.moveDown->isEnabled() );

                ui.mappings->item( 0, 1 )->setText( "A-edited" );
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 2 );
                REQUIRE( entries[ 0 ].mappings[ 0 ].displayValue == "A-edited" );
                REQUIRE( entries[ 1 ].mappings[ 0 ].displayValue == "B-shown" );
            }
        }
    }

    GIVEN( "no rules" )
    {
        FooterEditor editor( {} );
        EditorUi ui( editor );

        WHEN( "rules are appended" )
        {
            editor.appendEntries( { entryWithMapping( "A" ), entryWithMapping( "B" ) } );

            THEN( "the first one is selected and shown" )
            {
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.panel->isEnabled() );
                REQUIRE( ui.key->text() == "A" );
            }
        }
    }
}

SCENARIO( "FooterEditor validates rules while they are edited", "[footereditor]" )
{
    GIVEN( "an editor with two valid rules and the first one selected" )
    {
        FooterEditor editor( { entryWithMapping( "A" ), entryWithMapping( "B" ) } );
        EditorUi ui( editor );
        ui.select( 0 );
        REQUIRE( ui.canAccept() );
        REQUIRE( ui.problems->isHidden() );
        REQUIRE( EditorUi::unmarked( ui.linePattern, ui.linePatternProblem ) );

        WHEN( "its line pattern becomes invalid" )
        {
            ui.linePattern->setText( "[unclosed" );

            THEN( "the field and the list row show why, and the rules cannot be saved" )
            {
                REQUIRE( EditorUi::marked( ui.linePattern, ui.linePatternProblem ) );
                REQUIRE( ui.linePatternProblem->text().contains( "invalid line pattern" ) );
                REQUIRE( EditorUi::unmarked( ui.key, ui.keyProblem ) );
                REQUIRE( EditorUi::unmarked( ui.valuePattern, ui.valuePatternProblem ) );
                REQUIRE( ui.listMarked( 0 ) );
                REQUIRE_FALSE( ui.listMarked( 1 ) );
                REQUIRE( ui.problems->text().startsWith( "Rule 1: invalid line pattern" ) );
                REQUIRE_FALSE( ui.canAccept() );
            }

            AND_WHEN( "the other rule is selected" )
            {
                ui.select( 1 );

                THEN( "the panel shows that rule unmarked, and the rules still cannot be saved" )
                {
                    REQUIRE( EditorUi::unmarked( ui.linePattern, ui.linePatternProblem ) );
                    REQUIRE_FALSE( ui.canAccept() );

                    AND_WHEN( "the invalid rule is selected again" )
                    {
                        ui.select( 0 );

                        THEN( "its field is marked again" )
                        {
                            REQUIRE( EditorUi::marked( ui.linePattern, ui.linePatternProblem ) );
                        }
                    }
                }
            }

            AND_WHEN( "it is fixed again" )
            {
                ui.linePattern->setText( "[closed]" );

                THEN( "the rules can be saved" )
                {
                    REQUIRE( EditorUi::unmarked( ui.linePattern, ui.linePatternProblem ) );
                    REQUIRE_FALSE( ui.listMarked( 0 ) );
                    REQUIRE( ui.problems->isHidden() );
                    REQUIRE( ui.canAccept() );
                }
            }
        }

        WHEN( "a value pattern becomes invalid" )
        {
            ui.select( 1 );
            ui.valuePattern->setText( "(" );

            THEN( "the field is marked and the rules cannot be saved" )
            {
                REQUIRE( EditorUi::marked( ui.valuePattern, ui.valuePatternProblem ) );
                REQUIRE( ui.listMarked( 1 ) );
                REQUIRE_FALSE( ui.canAccept() );
            }
        }

        WHEN( "the second rule takes the key of the first" )
        {
            ui.select( 1 );
            ui.key->setText( "A" );

            THEN( "the rules are alternatives for the key and can be saved" )
            {
                REQUIRE( EditorUi::unmarked( ui.key, ui.keyProblem ) );
                REQUIRE_FALSE( ui.listMarked( 0 ) );
                REQUIRE_FALSE( ui.listMarked( 1 ) );
                REQUIRE( ui.canAccept() );
            }
        }

        WHEN( "an enabled rule with a line pattern loses its key" )
        {
            ui.select( 1 );
            ui.key->setText( " " );

            THEN( "the key is marked and the rules cannot be saved" )
            {
                REQUIRE( EditorUi::marked( ui.key, ui.keyProblem ) );
                REQUIRE( ui.keyProblem->text().contains( "needs a key" ) );
                REQUIRE( ui.listMarked( 1 ) );
                REQUIRE_FALSE( ui.canAccept() );
            }

            AND_WHEN( "the rule is disabled in the panel" )
            {
                ui.enabled->setChecked( false );

                THEN( "the rules can be saved" )
                {
                    REQUIRE( EditorUi::unmarked( ui.key, ui.keyProblem ) );
                    REQUIRE( ui.canAccept() );
                }
            }

            AND_WHEN( "the rule is disabled in the list" )
            {
                ui.setListChecked( 1, false );

                THEN( "the panel's mark goes away, and the rules can be saved" )
                {
                    REQUIRE( EditorUi::unmarked( ui.key, ui.keyProblem ) );
                    REQUIRE_FALSE( ui.listMarked( 1 ) );
                    REQUIRE( ui.canAccept() );
                }
            }
        }

        WHEN( "a rule has neither key nor line pattern yet" )
        {
            ui.select( 1 );
            ui.key->setText( "" );
            ui.linePattern->setText( "" );

            THEN( "the rules can still be saved" )
            {
                REQUIRE( ui.canAccept() );
            }
        }

        WHEN( "the invalid rule is removed" )
        {
            ui.linePattern->setText( "(" );
            REQUIRE_FALSE( ui.canAccept() );
            ui.removeRule->click();

            THEN( "the rules can be saved, and the remaining rule is not marked" )
            {
                REQUIRE( ui.canAccept() );
                REQUIRE( EditorUi::unmarked( ui.linePattern, ui.linePatternProblem ) );
                REQUIRE_FALSE( ui.listMarked( 0 ) );
            }
        }
    }

    GIVEN( "rules loaded with an invalid pattern" )
    {
        FooterEditor editor( { entryWithMapping( "A" ), { "Bad", "(", "", true, {} } } );
        EditorUi ui( editor );

        THEN( "they cannot be saved until fixed, and the rule is marked in the list" )
        {
            REQUIRE_FALSE( ui.canAccept() );
            REQUIRE( ui.listMarked( 1 ) );
            REQUIRE( ui.problems->text().startsWith( "Rule 2:" ) );
        }

        WHEN( "the rule is selected" )
        {
            ui.select( 1 );

            THEN( "its field is marked" )
            {
                REQUIRE( EditorUi::marked( ui.linePattern, ui.linePatternProblem ) );
            }
        }

        WHEN( "the rule is moved up" )
        {
            ui.select( 1 );
            ui.moveUp->click();

            THEN( "its mark and the rule number in the problems follow it" )
            {
                REQUIRE( ui.listMarked( 0 ) );
                REQUIRE_FALSE( ui.listMarked( 1 ) );
                REQUIRE( ui.problems->text().startsWith( "Rule 1:" ) );
                REQUIRE( EditorUi::marked( ui.linePattern, ui.linePatternProblem ) );
            }
        }
    }
}

SCENARIO( "FooterEditor compiles each pattern only once", "[footereditor]" )
{
    // 400 distinct patterns: validating all rules on every change would
    // compile them hundreds of thousands of times.
    QList<FooterEntry> entries;
    for ( int i = 0; i < 200; ++i ) {
        const auto key = QString( "Key%1" ).arg( i );
        entries.append(
            { key,
              QString( "^(?:\\d{4}-\\d{2}-\\d{2}|\\w+){1,3}\\s+%1:\\s+(\\S+)" ).arg( key ),
              QString( "(?<=%1:)\\s*([A-Z0-9]{3,}|[a-z]+-\\d+)" ).arg( key ),
              true,
              { ValueMapping{ "0", "zero" } } } );
    }

    GIVEN( "an editor with 200 rules" )
    {
        FooterEditor editor( entries );
        EditorUi ui( editor );

        THEN( "each pattern has been compiled once" )
        {
            REQUIRE( editor.patternCompilations() == 400 );
        }

        WHEN( "every rule is selected in turn" )
        {
            for ( int row = 0; row < 200; ++row ) {
                ui.select( row );
            }

            THEN( "no pattern is compiled again" )
            {
                REQUIRE( editor.patternCompilations() == 400 );
            }
        }

        WHEN( "a rule is moved up many times" )
        {
            ui.select( 199 );
            for ( int i = 0; i < 20; ++i ) {
                ui.moveUp->click();
            }

            THEN( "no pattern is compiled again and the rules are unchanged but for the order" )
            {
                REQUIRE( editor.patternCompilations() == 400 );
                REQUIRE( ui.listKey( 179 ) == "Key199" );
                REQUIRE( ui.key->text() == "Key199" );
                REQUIRE( editor.entries().size() == 200 );
                REQUIRE( ui.canAccept() );
            }

            AND_WHEN( "a pattern of the moved rule becomes invalid" )
            {
                ui.valuePattern->setText( "(" );

                THEN( "only that pattern is compiled, and only that rule is marked" )
                {
                    REQUIRE( editor.patternCompilations() == 401 );
                    REQUIRE( ui.model->problems( 179 ).valuePattern.contains( "invalid" ) );
                    REQUIRE( ui.model->problems( 178 ).isEmpty() );
                    REQUIRE( ui.model->problems( 180 ).isEmpty() );
                    REQUIRE( EditorUi::marked( ui.valuePattern, ui.valuePatternProblem ) );
                    REQUIRE_FALSE( ui.canAccept() );

                    AND_WHEN( "the rule is moved again" )
                    {
                        ui.moveUp->click();

                        THEN( "the mark moves with it, without compiling again" )
                        {
                            REQUIRE( editor.patternCompilations() == 401 );
                            REQUIRE_FALSE( ui.model->problems( 178 ).isEmpty() );
                            REQUIRE( ui.model->problems( 179 ).isEmpty() );
                            REQUIRE( EditorUi::marked( ui.valuePattern, ui.valuePatternProblem ) );
                            REQUIRE_FALSE( ui.canAccept() );
                        }
                    }
                }
            }
        }

        WHEN( "a key is typed into a rule one character at a time" )
        {
            ui.select( 100 );
            ui.key->setText( "" );
            for ( const auto& prefix : { "N", "Ne", "New" } ) {
                ui.key->setText( prefix );
            }

            THEN( "no pattern is compiled again" )
            {
                REQUIRE( editor.patternCompilations() == 400 );
                REQUIRE( ui.listKey( 100 ) == "New" );
            }
        }
    }
}
