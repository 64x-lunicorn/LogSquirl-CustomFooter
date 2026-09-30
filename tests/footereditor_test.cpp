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
 * @brief BDD tests for the FooterEditor rule and mapping tables.
 */

#include <catch2/catch.hpp>

#include "footereditor.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>

using namespace custom_footer;

namespace {

/// Test access to the editor's widgets, found by object name.
struct EditorUi {
    explicit EditorUi( FooterEditor& editor )
        : rules( editor.findChild<QTableWidget*>( "rulesTable" ) )
        , mappings( editor.findChild<QTableWidget*>( "mappingTable" ) )
        , mappingGroup( editor.findChild<QGroupBox*>( "mappingGroup" ) )
        , removeRule( editor.findChild<QToolButton*>( "removeRuleButton" ) )
        , moveUp( editor.findChild<QToolButton*>( "moveUpButton" ) )
        , buttons( editor.findChild<QDialogButtonBox*>() )
    {
        REQUIRE( rules );
        REQUIRE( mappings );
        REQUIRE( mappingGroup );
        REQUIRE( removeRule );
        REQUIRE( moveUp );
        REQUIRE( buttons );
    }

    QStringList shownMappingPatterns() const
    {
        QStringList patterns;
        for ( int i = 0; i < mappings->rowCount(); ++i ) {
            patterns.append( mappings->item( i, 0 )->text() );
        }
        return patterns;
    }

    void setEnabled( int row, bool enabled ) const
    {
        auto* checkbox = rules->cellWidget( row, 0 )->findChild<QCheckBox*>();
        REQUIRE( checkbox );
        checkbox->setChecked( enabled );
    }

    bool canAccept() const
    {
        return buttons->button( QDialogButtonBox::Ok )->isEnabled()
               && buttons->button( QDialogButtonBox::Apply )->isEnabled();
    }

    QTableWidget* rules;
    QTableWidget* mappings;
    QGroupBox* mappingGroup;
    QToolButton* removeRule;
    QToolButton* moveUp;
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

} // namespace

SCENARIO( "FooterEditor keeps mappings with their rule when rules are removed", "[footereditor]" )
{
    GIVEN( "three rules with one mapping each and the first rule selected" )
    {
        FooterEditor editor( threeRules() );
        EditorUi ui( editor );
        ui.rules->setCurrentCell( 0, 1 );
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
                REQUIRE( ui.rules->currentRow() == 0 );
                REQUIRE( ui.mappingGroup->isEnabled() );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "B-raw" } );
            }

            AND_WHEN( "selecting the other rule and back" )
            {
                ui.rules->setCurrentCell( 1, 1 );
                ui.rules->setCurrentCell( 0, 1 );

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
            ui.rules->setCurrentCell( 2, 1 );
            ui.removeRule->click();

            THEN( "the rule above becomes selected and shown" )
            {
                REQUIRE( ui.rules->currentRow() == 1 );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "B-raw" } );
            }
        }
    }

    GIVEN( "two rules and the first one selected" )
    {
        FooterEditor editor( { entryWithMapping( "A" ), entryWithMapping( "B" ) } );
        EditorUi ui( editor );
        ui.rules->setCurrentCell( 0, 1 );

        WHEN( "removing it and editing the mapping of the remaining rule" )
        {
            ui.removeRule->click();
            ui.mappings->item( 0, 1 )->setText( "edited" );

            THEN( "the edit is kept" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 1 );
                REQUIRE( entries[ 0 ].mappings[ 0 ].displayValue == "edited" );
                REQUIRE( ui.rules->item( 0, 4 )->text() == "1" );
            }
        }
    }

    GIVEN( "a single selected rule" )
    {
        FooterEditor editor( { entryWithMapping( "A" ) } );
        EditorUi ui( editor );
        ui.rules->setCurrentCell( 0, 1 );

        WHEN( "removing it" )
        {
            ui.removeRule->click();

            THEN( "the mapping panel is empty and disabled" )
            {
                REQUIRE( editor.entries().isEmpty() );
                REQUIRE_FALSE( ui.mappingGroup->isEnabled() );
                REQUIRE( ui.mappings->rowCount() == 0 );
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
        ui.rules->setCurrentCell( 1, 1 );
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
                REQUIRE( ui.rules->currentRow() == 0 );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "B-raw" } );
            }
        }
    }
}

SCENARIO( "FooterEditor keeps the mapping panel live after an import", "[footereditor]" )
{
    GIVEN( "a selected rule" )
    {
        FooterEditor editor( { entryWithMapping( "A" ) } );
        EditorUi ui( editor );
        ui.rules->setCurrentCell( 0, 1 );

        WHEN( "rules are appended" )
        {
            editor.appendEntries( { entryWithMapping( "B" ) } );

            THEN( "the panel still shows and edits the selected rule" )
            {
                REQUIRE( ui.rules->currentRow() == 0 );
                REQUIRE( ui.mappingGroup->isEnabled() );
                REQUIRE( ui.shownMappingPatterns() == QStringList{ "A-raw" } );

                ui.mappings->item( 0, 1 )->setText( "A-edited" );
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 2 );
                REQUIRE( entries[ 0 ].mappings[ 0 ].displayValue == "A-edited" );
                REQUIRE( entries[ 1 ].mappings[ 0 ].displayValue == "B-shown" );
            }
        }
    }
}

SCENARIO( "FooterEditor validates rules while they are edited", "[footereditor]" )
{
    GIVEN( "an editor with two valid rules" )
    {
        FooterEditor editor( { entryWithMapping( "A" ), entryWithMapping( "B" ) } );
        EditorUi ui( editor );
        REQUIRE( ui.canAccept() );

        WHEN( "a line pattern becomes invalid" )
        {
            ui.rules->item( 0, 2 )->setText( "[unclosed" );

            THEN( "the cell explains the error and the rules cannot be saved" )
            {
                REQUIRE_FALSE( ui.rules->item( 0, 2 )->toolTip().isEmpty() );
                REQUIRE_FALSE( ui.canAccept() );
            }

            AND_WHEN( "it is fixed again" )
            {
                ui.rules->item( 0, 2 )->setText( "[closed]" );

                THEN( "the rules can be saved" )
                {
                    REQUIRE( ui.rules->item( 0, 2 )->toolTip().isEmpty() );
                    REQUIRE( ui.canAccept() );
                }
            }
        }

        WHEN( "a value pattern becomes invalid" )
        {
            ui.rules->item( 1, 3 )->setText( "(" );

            THEN( "the rules cannot be saved" )
            {
                REQUIRE_FALSE( ui.rules->item( 1, 3 )->toolTip().isEmpty() );
                REQUIRE_FALSE( ui.canAccept() );
            }
        }

        WHEN( "the second rule takes the key of the first" )
        {
            ui.rules->item( 1, 1 )->setText( "A" );

            THEN( "the rules are alternatives for the key and can be saved" )
            {
                REQUIRE( ui.rules->item( 1, 1 )->toolTip().isEmpty() );
                REQUIRE( ui.rules->item( 0, 1 )->toolTip().isEmpty() );
                REQUIRE( ui.canAccept() );
            }
        }

        WHEN( "an enabled rule with a line pattern loses its key" )
        {
            ui.rules->item( 1, 1 )->setText( " " );

            THEN( "the key is marked and the rules cannot be saved" )
            {
                REQUIRE_FALSE( ui.rules->item( 1, 1 )->toolTip().isEmpty() );
                REQUIRE_FALSE( ui.canAccept() );
            }

            AND_WHEN( "the rule is disabled" )
            {
                ui.setEnabled( 1, false );

                THEN( "the rules can be saved" )
                {
                    REQUIRE( ui.rules->item( 1, 1 )->toolTip().isEmpty() );
                    REQUIRE( ui.canAccept() );
                }
            }
        }

        WHEN( "a rule has neither key nor line pattern yet" )
        {
            ui.rules->item( 1, 1 )->setText( "" );
            ui.rules->item( 1, 2 )->setText( "" );

            THEN( "the rules can still be saved" )
            {
                REQUIRE( ui.canAccept() );
            }
        }
    }

    GIVEN( "rules loaded with an invalid pattern" )
    {
        FooterEditor editor( { { "Bad", "(", "", true, {} } } );
        EditorUi ui( editor );

        THEN( "they cannot be saved until fixed" )
        {
            REQUIRE_FALSE( ui.canAccept() );
        }
    }
}

SCENARIO( "FooterEditor stays responsive with many rules", "[footereditor]" )
{
    // Patterns that take a while to compile, all of them distinct.
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
        QElapsedTimer timer;
        timer.start();
        FooterEditor editor( entries );
        EditorUi ui( editor );

        WHEN( "a rule is moved up many times" )
        {
            ui.rules->setCurrentCell( 199, 1 );
            for ( int i = 0; i < 20; ++i ) {
                ui.moveUp->click();
            }

            THEN( "the editor keeps up and the rules are unchanged but for the order" )
            {
                REQUIRE( timer.elapsed() < 3000 );
                REQUIRE( ui.rules->item( 179, 1 )->text() == "Key199" );
                REQUIRE( editor.entries().size() == 200 );
                REQUIRE( ui.canAccept() );
            }

            AND_WHEN( "a pattern of the moved rule becomes invalid" )
            {
                ui.rules->item( 179, 3 )->setText( "(" );

                THEN( "only that rule is marked" )
                {
                    REQUIRE_FALSE( ui.rules->item( 179, 3 )->toolTip().isEmpty() );
                    REQUIRE( ui.rules->item( 178, 3 )->toolTip().isEmpty() );
                    REQUIRE( ui.rules->item( 180, 3 )->toolTip().isEmpty() );
                    REQUIRE_FALSE( ui.canAccept() );

                    AND_WHEN( "the rule is moved again" )
                    {
                        ui.moveUp->click();

                        THEN( "the mark moves with it" )
                        {
                            REQUIRE_FALSE( ui.rules->item( 178, 3 )->toolTip().isEmpty() );
                            REQUIRE( ui.rules->item( 179, 3 )->toolTip().isEmpty() );
                            REQUIRE_FALSE( ui.canAccept() );
                        }
                    }
                }
            }
        }
    }
}
