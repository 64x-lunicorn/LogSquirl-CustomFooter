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

#include "rulelistview.h"

#include <QDrag>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>

namespace custom_footer {

RuleListView::RuleListView( QWidget* parent )
    : QTableView( parent )
{
    setDragEnabled( true );
    setAcceptDrops( true );
    setDragDropMode( QAbstractItemView::InternalMove );
    setDefaultDropAction( Qt::MoveAction );
    setDropIndicatorShown( true );
    // Drop between rules, never onto one.
    setDragDropOverwriteMode( false );
}

void RuleListView::keyPressEvent( QKeyEvent* event )
{
    // Ctrl is Cmd on macOS, as for every Qt shortcut. macOS adds the keypad
    // modifier to every arrow key, as the numeric keypad's arrows do
    // elsewhere, so it is ignored.
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if ( modifiers == ( Qt::ControlModifier | Qt::ShiftModifier ) ) {
        if ( event->key() == Qt::Key_Up ) {
            Q_EMIT moveUpRequested();
            event->accept();
            return;
        }
        if ( event->key() == Qt::Key_Down ) {
            Q_EMIT moveDownRequested();
            event->accept();
            return;
        }
    }
    QTableView::keyPressEvent( event );
}

void RuleListView::mousePressEvent( QMouseEvent* event )
{
    pressPosition_ = event->position().toPoint();
    QTableView::mousePressEvent( event );
}

void RuleListView::startDrag( Qt::DropActions supportedActions )
{
    // Rules are only moved.
    if ( !model() || !( supportedActions & Qt::MoveAction ) ) {
        return;
    }
    QModelIndexList indexes;
    for ( const auto& index : selectedIndexes() ) {
        if ( model()->flags( index ) & Qt::ItemIsDragEnabled ) {
            indexes.append( index );
        }
    }
    if ( indexes.isEmpty() ) {
        return;
    }
    auto* data = model()->mimeData( indexes );
    if ( !data ) {
        return;
    }

    auto* drag = new QDrag( this );
    drag->setMimeData( data );
    // The dragged cells as they look in the list, held where they were
    // pressed.
    QRect rect;
    for ( const auto& index : std::as_const( indexes ) ) {
        rect |= visualRect( index );
    }
    rect &= viewport()->rect();
    if ( !rect.isEmpty() ) {
        drag->setPixmap( viewport()->grab( rect ) );
        drag->setHotSpot( pressPosition_ - rect.topLeft() );
    }

    // The drop has already moved the rules in the model
    // (RuleListModel::dropMimeData()). Unlike QAbstractItemView::startDrag(),
    // nothing is removed after a drag that ends in a move: the selected row
    // is then the moved rule itself.
    drag->exec( Qt::MoveAction, Qt::MoveAction );

    // What the base class resets after a drag: no drop indicator, no
    // scrolling, no drag state.
    stopAutoScroll();
    setState( NoState );
    viewport()->update();
}

} // namespace custom_footer
