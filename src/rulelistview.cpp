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

#include <QCursor>
#include <QDrag>
#include <QKeyEvent>
#include <QMimeData>
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
    // Ctrl is Cmd on macOS, as for every Qt shortcut.
    if ( event->modifiers() == ( Qt::ControlModifier | Qt::ShiftModifier ) ) {
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

void RuleListView::startDrag( Qt::DropActions /*supportedActions*/ )
{
    if ( !model() || !selectionModel() ) {
        return;
    }
    const auto rows = selectionModel()->selectedRows();
    if ( rows.isEmpty() ) {
        return;
    }
    auto* data = model()->mimeData( rows );
    if ( !data ) {
        return;
    }

    auto* drag = new QDrag( this );
    drag->setMimeData( data );
    // The dragged row as it looks in the list.
    QRect rowRect;
    for ( const auto& row : rows ) {
        rowRect
            |= visualRect( row ) | visualRect( row.siblingAtColumn( model()->columnCount() - 1 ) );
    }
    rowRect &= viewport()->rect();
    if ( !rowRect.isEmpty() ) {
        drag->setPixmap( viewport()->grab( rowRect ) );
        drag->setHotSpot( viewport()->mapFromGlobal( QCursor::pos() ) - rowRect.topLeft() );
    }

    // The drop has already moved the rules in the model. Unlike
    // QAbstractItemView::startDrag(), nothing is removed here: on macOS an
    // internal move can be reported as a copy, or a move could remove the
    // rule that now sits where the dragged one was.
    drag->exec( Qt::MoveAction, Qt::MoveAction );
}

} // namespace custom_footer
