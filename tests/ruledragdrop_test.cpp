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
 * @file ruledragdrop_test.cpp
 * @brief BDD tests for reordering rules in the FooterEditor by drag & drop
 *        and by keyboard.
 *
 * A drag is simulated as the view performs it: the model's mimeData() for
 * the dragged row, then dropMimeData() at the row it is dropped before.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footereditor.h"
#include "rulelistmodel.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>

#include <algorithm>
#include <memory>
#include <utility>

using namespace custom_footer;

namespace {

struct DragUi {
    template <typename Widget>
    static Widget* find( FooterEditor& editor, const char* name )
    {
        auto* widget = editor.findChild<Widget*>( name );
        REQUIRE( widget );
        return widget;
    }

    explicit DragUi( FooterEditor& editor )
        : list( find<QTableView>( editor, "ruleList" ) )
        , model( qobject_cast<RuleListModel*>( list->model() ) )
        , key( find<QLineEdit>( editor, "keyEdit" ) )
        , mappings( find<QTableWidget>( editor, "mappingTable" ) )
        , moveUp( find<QToolButton>( editor, "moveUpButton" ) )
        , moveDown( find<QToolButton>( editor, "moveDownButton" ) )
        , problems( find<QLabel>( editor, "problemLabel" ) )
    {
        REQUIRE( model );
    }

    void select( int row ) const
    {
        list->setCurrentIndex( model->index( row, RuleListModel::KeyColumn ) );
    }

    int currentRow() const
    {
        return list->currentIndex().isValid() ? list->currentIndex().row() : -1;
    }

    QString listKey( int row ) const
    {
        return model->index( row, RuleListModel::KeyColumn ).data().toString();
    }

    /// Drag the rule in @p from and drop it before the rule in @p before
    /// (the row count drops it at the end), as the view does.
    bool drag( int from, int before, Qt::DropAction action = Qt::MoveAction ) const
    {
        const std::unique_ptr<QMimeData> data(
            model->mimeData( { model->index( from, RuleListModel::KeyColumn ),
                               model->index( from, RuleListModel::LinePatternColumn ) } ) );
        REQUIRE( data );
        if ( !model->canDropMimeData( data.get(), action, before, 0, QModelIndex() ) ) {
            return false;
        }
        return model->dropMimeData( data.get(), action, before, 0, QModelIndex() );
    }

    /// Drag the rule in @p from and drop it onto the rule in @p onto.
    bool dragOnto( int from, int onto ) const
    {
        const std::unique_ptr<QMimeData> data(
            model->mimeData( { model->index( from, RuleListModel::KeyColumn ) } ) );
        const auto target = model->index( onto, RuleListModel::LinePatternColumn );
        return model->canDropMimeData( data.get(), Qt::MoveAction, -1, -1, target )
               && model->dropMimeData( data.get(), Qt::MoveAction, -1, -1, target );
    }

    void pressMove( int key ) const
    {
        QKeyEvent event( QEvent::KeyPress, key, Qt::ControlModifier | Qt::ShiftModifier );
        QApplication::sendEvent( list, &event );
    }

    QTableView* list;
    RuleListModel* model;
    QLineEdit* key;
    QTableWidget* mappings;
    QToolButton* moveUp;
    QToolButton* moveDown;
    QLabel* problems;
};

/// Rules that differ in every field, one of them invalid, so that a mix-up
/// of any field between rules shows.
QList<FooterEntry> distinctRules()
{
    return {
        { "A", "a=(\\S+)", "", true, { ValueMapping{ "a1", "A one" } } },
        { "B", "b=(\\S+)", "b:(\\d+)", false, {} },
        { "C", "(", "", true, { ValueMapping{ "c1", "C one" }, ValueMapping{ "c2", "C two" } } },
        { "D", "d=(\\S+)", "", false, { ValueMapping{ "d1", "D one" } } },
        { "E",
          "e=(\\S+)",
          "e:(\\w+)",
          true,
          { ValueMapping{ "e1", "E one" }, ValueMapping{ "e2", "E two" },
            ValueMapping{ "e3", "E three" } } },
    };
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

/// Where each rule should be after moving the one in @p from to @p to.
void expectMove( QList<FooterEntry>& expected, int from, int to )
{
    expected.move( from, to );
}

/// Every rule is whole and in the expected place, and only the invalid
/// rule "C" is marked, in its current row.
void requireRules( const DragUi& ui, const FooterEditor& editor,
                   const QList<FooterEntry>& expected )
{
    const auto entries = editor.entries();
    REQUIRE( entries.size() == expected.size() );
    for ( int row = 0; row < expected.size(); ++row ) {
        INFO( "row " << row );
        REQUIRE( sameEntry( entries[ row ], expected[ row ] ) );
        REQUIRE( ui.listKey( row ) == expected[ row ].key );
        REQUIRE( ui.model->problems( row ).isEmpty() == ( expected[ row ].key != "C" ) );
        REQUIRE( ui.model->index( row, 0 ).data( Qt::CheckStateRole ).toInt()
                 == ( expected[ row ].enabled ? Qt::Checked : Qt::Unchecked ) );
    }
    const int invalidRow = static_cast<int>(
        std::find_if( expected.begin(), expected.end(),
                      []( const FooterEntry& entry ) { return entry.key == "C"; } )
        - expected.begin() );
    REQUIRE( ui.problems->text().startsWith( QString( "Rule %1:" ).arg( invalidRow + 1 ) ) );
}

} // namespace

SCENARIO( "FooterEditor lets rules be dragged within the rule list", "[footereditor][dragdrop]" )
{
    GIVEN( "an editor with rules" )
    {
        FooterEditor editor( distinctRules() );
        DragUi ui( editor );

        THEN( "the list moves rules by drag & drop, and only within itself" )
        {
            REQUIRE( ui.list->dragEnabled() );
            REQUIRE( ui.list->dragDropMode() == QAbstractItemView::InternalMove );
            REQUIRE( ui.list->defaultDropAction() == Qt::MoveAction );
            REQUIRE( ui.list->showDropIndicator() );
            REQUIRE_FALSE( ui.list->dragDropOverwriteMode() );
            REQUIRE( ui.model->supportedDropActions() == Qt::MoveAction );
            REQUIRE( ui.model->supportedDragActions() == Qt::MoveAction );
        }

        THEN( "rules can be dragged, and dropped between rules but not onto one" )
        {
            for ( int column = 0; column < RuleListModel::ColumnCount; ++column ) {
                const auto flags = ui.model->flags( ui.model->index( 1, column ) );
                REQUIRE( flags.testFlag( Qt::ItemIsDragEnabled ) );
                REQUIRE_FALSE( flags.testFlag( Qt::ItemIsDropEnabled ) );
            }
            REQUIRE( ui.model->flags( QModelIndex() ).testFlag( Qt::ItemIsDropEnabled ) );
        }
    }
}

SCENARIO( "FooterEditor reorders rules by drag and drop", "[footereditor][dragdrop]" )
{
    GIVEN( "rules that differ in every field, the second one selected" )
    {
        FooterEditor editor( distinctRules() );
        DragUi ui( editor );
        auto expected = distinctRules();
        ui.select( 1 );
        requireRules( ui, editor, expected );

        WHEN( "dragging it down, below the fourth rule" )
        {
            REQUIRE( ui.drag( 1, 4 ) );
            expectMove( expected, 1, 3 );

            THEN( "it moves there with all its data, and stays the selected rule" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 3 );
                REQUIRE( ui.key->text() == "B" );
            }

            AND_WHEN( "editing it in the panel" )
            {
                ui.key->setText( "B2" );

                THEN( "the dragged rule is edited, not the one now in its old place" )
                {
                    const auto entries = editor.entries();
                    REQUIRE( entries[ 3 ].key == "B2" );
                    REQUIRE( entries[ 1 ].key == "C" );
                }
            }
        }

        WHEN( "dragging the fourth rule up, above the second" )
        {
            ui.select( 3 );
            REQUIRE( ui.drag( 3, 1 ) );
            expectMove( expected, 3, 1 );

            THEN( "it moves there with all its data, and stays the selected rule" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 1 );
                REQUIRE( ui.key->text() == "D" );
                REQUIRE( ui.mappings->item( 0, 0 )->text() == "d1" );
            }
        }

        WHEN( "dragging the last rule to the top" )
        {
            ui.select( 4 );
            REQUIRE( ui.drag( 4, 0 ) );
            expectMove( expected, 4, 0 );

            THEN( "it becomes the first rule" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.key->text() == "E" );
                REQUIRE_FALSE( ui.moveUp->isEnabled() );
                REQUIRE( ui.moveDown->isEnabled() );
            }
        }

        WHEN( "dragging the first rule to the end" )
        {
            ui.select( 0 );
            REQUIRE( ui.drag( 0, 5 ) );
            expectMove( expected, 0, 4 );

            THEN( "it becomes the last rule" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 4 );
                REQUIRE( ui.key->text() == "A" );
                REQUIRE( ui.moveUp->isEnabled() );
                REQUIRE_FALSE( ui.moveDown->isEnabled() );
            }
        }

        WHEN( "dropping a rule onto another rule" )
        {
            REQUIRE( ui.dragOnto( 1, 3 ) );
            expectMove( expected, 1, 3 );

            THEN( "it takes that rule's place" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 3 );
            }
        }

        WHEN( "dropping a rule where it already is" )
        {
            const bool droppedAbove = ui.drag( 1, 1 );
            const bool droppedBelow = ui.drag( 1, 2 );

            THEN( "nothing changes" )
            {
                REQUIRE_FALSE( droppedAbove );
                REQUIRE_FALSE( droppedBelow );
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 1 );
            }
        }

        WHEN( "the platform reports the drop as a copy" )
        {
            // On macOS an internal move can arrive as a copy; a rule is
            // never duplicated.
            REQUIRE( ui.drag( 1, 4, Qt::CopyAction ) );
            expectMove( expected, 1, 3 );

            THEN( "the rule is moved, not copied" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 3 );
            }
        }

        WHEN( "dragging rules around many times, in every direction" )
        {
            const QList<std::pair<int, int>> drags
                = { { 0, 5 }, { 4, 0 }, { 2, 4 }, { 3, 1 }, { 1, 5 }, { 4, 2 },
                    { 0, 3 }, { 2, 0 }, { 3, 5 }, { 4, 1 }, { 1, 0 }, { 0, 2 } };
            for ( const auto& [ from, before ] : drags ) {
                ui.select( from );
                REQUIRE( ui.drag( from, before ) );
                const int to = before > from ? before - 1 : before;
                expectMove( expected, from, to );
                INFO( "after dragging " << from << " before " << before );
                REQUIRE( ui.currentRow() == to );
                REQUIRE( ui.key->text() == expected[ to ].key );
                requireRules( ui, editor, expected );
            }

            THEN( "every rule still has its own data and marks" )
            {
                requireRules( ui, editor, expected );
            }
        }

        WHEN( "a mapping is still being typed when the rule is dragged" )
        {
            ui.select( 0 );
            ui.mappings->setCurrentCell( 0, 1 );
            ui.mappings->editItem( ui.mappings->item( 0, 1 ) );
            auto* cell = ui.mappings->viewport()->findChild<QLineEdit*>();
            REQUIRE( cell );
            cell->setText( "typed" );

            REQUIRE( ui.drag( 0, 5 ) );

            THEN( "the typed text moves with its rule" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries[ 4 ].key == "A" );
                REQUIRE( entries[ 4 ].mappings[ 0 ].displayValue == "typed" );
                REQUIRE( entries[ 0 ].key == "B" );
                REQUIRE( entries[ 0 ].mappings.isEmpty() );
            }
        }
    }
}

SCENARIO( "FooterEditor only accepts rules dragged from its own list", "[footereditor][dragdrop]" )
{
    GIVEN( "two editors" )
    {
        FooterEditor editor( distinctRules() );
        FooterEditor other( distinctRules() );
        DragUi ui( editor );
        DragUi otherUi( other );

        WHEN( "a rule of the other editor is dropped into this one" )
        {
            const std::unique_ptr<QMimeData> data(
                otherUi.model->mimeData( { otherUi.model->index( 0, 0 ) } ) );
            const bool canDrop
                = ui.model->canDropMimeData( data.get(), Qt::MoveAction, 2, 0, QModelIndex() );
            const bool dropped
                = ui.model->dropMimeData( data.get(), Qt::MoveAction, 2, 0, QModelIndex() );

            THEN( "neither editor changes" )
            {
                REQUIRE_FALSE( canDrop );
                REQUIRE_FALSE( dropped );
                requireRules( ui, editor, distinctRules() );
                requireRules( otherUi, other, distinctRules() );
            }
        }

        WHEN( "something else is dropped" )
        {
            QMimeData text;
            text.setText( "A" );

            THEN( "it is refused" )
            {
                REQUIRE_FALSE(
                    ui.model->canDropMimeData( &text, Qt::MoveAction, 2, 0, QModelIndex() ) );
                REQUIRE_FALSE(
                    ui.model->dropMimeData( &text, Qt::MoveAction, 2, 0, QModelIndex() ) );
                requireRules( ui, editor, distinctRules() );
            }
        }
    }
}

SCENARIO( "FooterEditor saves the rules in the order of the list", "[footereditor][dragdrop]" )
{
    GIVEN( "rules reordered by drags, ↑/↓ and the keyboard" )
    {
        FooterEditor editor( distinctRules() );
        DragUi ui( editor );
        REQUIRE( ui.drag( 4, 0 ) );
        REQUIRE( ui.drag( 1, 4 ) );
        ui.select( 2 );
        ui.moveUp->click();
        ui.pressMove( Qt::Key_Down );
        ui.pressMove( Qt::Key_Down );

        // Fix the invalid rule, or Apply stays disabled.
        for ( int row = 0; row < ui.model->rowCount(); ++row ) {
            if ( ui.listKey( row ) == "C" ) {
                ui.select( row );
            }
        }
        DragUi::find<QLineEdit>( editor, "linePatternEdit" )->setText( "c=(\\S+)" );
        REQUIRE(
            editor.findChild<QDialogButtonBox*>()->button( QDialogButtonBox::Apply )->isEnabled() );

        WHEN( "they are saved and loaded again" )
        {
            QList<FooterEntry> applied;
            QObject::connect( &editor, &FooterEditor::applied,
                              [ & ] { applied = editor.entries(); } );
            editor.findChild<QDialogButtonBox*>()->button( QDialogButtonBox::Apply )->click();
            QTemporaryDir dir;
            REQUIRE( dir.isValid() );
            REQUIRE( FooterConfig::saveEntries( dir.path(), applied ) );
            const auto loaded = FooterConfig::loadEntries( dir.path() );

            THEN( "their order is the order of the list" )
            {
                REQUIRE( loaded.size() == ui.model->rowCount() );
                for ( int row = 0; row < ui.model->rowCount(); ++row ) {
                    INFO( "row " << row );
                    REQUIRE( loaded[ row ].key == ui.listKey( row ) );
                    REQUIRE( sameEntry( loaded[ row ], ui.model->entry( row ) ) );
                }
            }
        }
    }
}

SCENARIO( "FooterEditor moves the selected rule with the keyboard", "[footereditor][dragdrop]" )
{
    GIVEN( "rules, the second one selected" )
    {
        FooterEditor editor( distinctRules() );
        DragUi ui( editor );
        auto expected = distinctRules();
        ui.select( 1 );

        THEN( "the ↑/↓ tooltips name the shortcuts" )
        {
            const auto up = QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_Up )
                                .toString( QKeySequence::NativeText );
            const auto down = QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_Down )
                                  .toString( QKeySequence::NativeText );
            REQUIRE( ui.moveUp->toolTip().contains( up ) );
            REQUIRE( ui.moveDown->toolTip().contains( down ) );
        }

        WHEN( "pressing Ctrl+Shift+Up in the list" )
        {
            ui.pressMove( Qt::Key_Up );
            expectMove( expected, 1, 0 );

            THEN( "the rule moves up with its data and stays selected" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.key->text() == "B" );
            }

            AND_WHEN( "pressing it again at the top" )
            {
                ui.pressMove( Qt::Key_Up );

                THEN( "nothing changes" )
                {
                    requireRules( ui, editor, expected );
                    REQUIRE( ui.currentRow() == 0 );
                }
            }
        }

        WHEN( "pressing Ctrl+Shift+Down in the list until the end" )
        {
            for ( int i = 0; i < 5; ++i ) {
                ui.pressMove( Qt::Key_Down );
            }
            expectMove( expected, 1, 4 );

            THEN( "the rule moves to the end with its data and stays selected" )
            {
                requireRules( ui, editor, expected );
                REQUIRE( ui.currentRow() == 4 );
                REQUIRE( ui.key->text() == "B" );
            }
        }

        WHEN( "a mapping is still being typed when the rule is moved by keyboard" )
        {
            ui.select( 0 );
            ui.mappings->setCurrentCell( 0, 1 );
            ui.mappings->editItem( ui.mappings->item( 0, 1 ) );
            auto* cell = ui.mappings->viewport()->findChild<QLineEdit*>();
            REQUIRE( cell );
            cell->setText( "typed" );
            ui.pressMove( Qt::Key_Down );

            THEN( "the typed text moves with its rule" )
            {
                REQUIRE( editor.entries()[ 1 ].key == "A" );
                REQUIRE( editor.entries()[ 1 ].mappings[ 0 ].displayValue == "typed" );
            }
        }
    }
}

SCENARIO( "FooterEditor only renumbers the problems after a drag", "[footereditor][dragdrop]" )
{
    GIVEN( "an editor with 200 rules" )
    {
        QList<FooterEntry> entries;
        for ( int i = 0; i < 200; ++i ) {
            entries.append( { QString( "R%1" ).arg( i ),
                              QString( "r%1=(\\S+)" ).arg( i ),
                              QString(),
                              true,
                              {} } );
        }
        entries[ 150 ].linePattern = "(";
        FooterEditor editor( entries );
        DragUi ui( editor );
        const int validations = editor.ruleValidations();
        const int compilations = editor.patternCompilations();
        REQUIRE( ui.problems->text().startsWith( "Rule 151:" ) );

        WHEN( "dragging rules around, the invalid one among them" )
        {
            REQUIRE( ui.drag( 150, 0 ) );
            REQUIRE( ui.drag( 10, 200 ) );
            REQUIRE( ui.drag( 199, 100 ) );
            ui.select( 50 );
            ui.pressMove( Qt::Key_Up );

            THEN( "no rule is validated or compiled again, and the marks are renumbered" )
            {
                REQUIRE( editor.ruleValidations() == validations );
                REQUIRE( editor.patternCompilations() == compilations );
                REQUIRE( ui.listKey( 0 ) == "R150" );
                REQUIRE_FALSE( ui.model->problems( 0 ).isEmpty() );
                REQUIRE( ui.problems->text().startsWith( "Rule 1:" ) );
            }
        }
    }
}
