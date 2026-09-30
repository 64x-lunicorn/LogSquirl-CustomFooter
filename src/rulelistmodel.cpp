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

#include "rulelistmodel.h"

#include <QBrush>
#include <QDataStream>
#include <QFont>
#include <QIODevice>
#include <QMimeData>
#include <QPalette>
#include <QRandomGenerator>
#include <QStringList>

#include <algorithm>

namespace custom_footer {

namespace {

/// A pattern on one line, for the list: the view elides what does not fit.
QString oneLine( QString text )
{
    text.replace( QLatin1Char( '\n' ), QChar( 0x21B5 ) ); // ↵
    text.remove( QLatin1Char( '\r' ) );
    return text;
}

/// The rows of a drag within the rule list: the drag token of the model
/// they come from, so that no other list takes them, and the row numbers.
const QString RuleRowsMimeType
    = QStringLiteral( "application/x-logsquirl-custom-footer-rule-rows" );

} // namespace

RuleListModel::RuleListModel( QObject* parent )
    : QAbstractTableModel( parent )
    , dragToken_( QRandomGenerator::global()->generate64() )
{
}

QList<FooterEntry> RuleListModel::entries() const
{
    QList<FooterEntry> result;
    result.reserve( rules_.size() );
    for ( const auto& rule : rules_ ) {
        result.append( rule.entry );
    }
    return result;
}

const FooterEntry& RuleListModel::entry( int row ) const
{
    return rules_.at( row ).entry;
}

void RuleListModel::setEntries( const QList<FooterEntry>& entries )
{
    beginResetModel();
    rules_.clear();
    for ( const auto& entry : entries ) {
        rules_.append( { entry, {} } );
    }
    endResetModel();
}

void RuleListModel::appendEntries( const QList<FooterEntry>& entries )
{
    if ( entries.isEmpty() ) {
        return;
    }
    const int first = static_cast<int>( rules_.size() );
    beginInsertRows( QModelIndex(), first, first + static_cast<int>( entries.size() ) - 1 );
    for ( const auto& entry : entries ) {
        rules_.append( { entry, {} } );
    }
    endInsertRows();
}

void RuleListModel::setEntry( int row, const FooterEntry& entry )
{
    if ( row < 0 || row >= rules_.size() ) {
        return;
    }
    rules_[ row ].entry = entry;
    emitRowChanged( row );
}

bool RuleListModel::moveRule( int from, int to )
{
    // moveRows() takes the row the rule is moved before.
    return moveRows( QModelIndex(), from, 1, QModelIndex(), to > from ? to + 1 : to );
}

std::optional<SimpleRule> RuleListModel::unfinishedSimpleRule( int row ) const
{
    return row >= 0 && row < rules_.size() ? rules_.at( row ).unfinishedSimpleRule : std::nullopt;
}

void RuleListModel::setUnfinishedSimpleRule( int row, const std::optional<SimpleRule>& rule )
{
    if ( row >= 0 && row < rules_.size() ) {
        rules_[ row ].unfinishedSimpleRule = rule;
    }
}

RuleProblems RuleListModel::problems( int row ) const
{
    return row >= 0 && row < rules_.size() ? rules_.at( row ).problems : RuleProblems();
}

void RuleListModel::setProblems( int row, const RuleProblems& problems )
{
    if ( row < 0 || row >= rules_.size() || rules_.at( row ).problems == problems ) {
        return;
    }
    rules_[ row ].problems = problems;
    emitRowChanged( row );
}

int RuleListModel::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : static_cast<int>( rules_.size() );
}

int RuleListModel::columnCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant RuleListModel::data( const QModelIndex& index, int role ) const
{
    if ( !index.isValid() || index.row() >= rules_.size() ) {
        return {};
    }
    const auto& rule = rules_.at( index.row() );
    const bool keyColumn = index.column() == KeyColumn;

    switch ( role ) {
    case Qt::DisplayRole:
        if ( keyColumn ) {
            return rule.entry.key.isEmpty() ? tr( "(no key)" ) : rule.entry.key;
        }
        return oneLine( rule.entry.linePattern );
    case Qt::EditRole:
        return keyColumn ? rule.entry.key : rule.entry.linePattern;
    case Qt::CheckStateRole:
        if ( keyColumn ) {
            return rule.entry.enabled ? Qt::Checked : Qt::Unchecked;
        }
        return {};
    case Qt::ForegroundRole:
        if ( keyColumn && rule.entry.key.isEmpty() ) {
            return QPalette().brush( QPalette::Disabled, QPalette::Text );
        }
        return {};
    case Qt::FontRole:
        if ( keyColumn && rule.entry.key.isEmpty() ) {
            QFont font;
            font.setItalic( true );
            return font;
        }
        return {};
    case Qt::BackgroundRole:
        return rule.problems.isEmpty() ? QVariant()
                                       : QVariant( QBrush( problemBackgroundColor() ) );
    case Qt::ToolTipRole: {
        QStringList lines;
        if ( !keyColumn && !rule.entry.linePattern.isEmpty() ) {
            lines.append( rule.entry.linePattern );
        }
        for ( const auto& problem : { rule.problems.key, rule.problems.endCharacter,
                                      rule.problems.linePattern, rule.problems.valuePattern } ) {
            if ( !problem.isEmpty() ) {
                lines.append( problem );
            }
        }
        return lines.isEmpty() ? QVariant() : QVariant( lines.join( QLatin1Char( '\n' ) ) );
    }
    default:
        return {};
    }
}

QVariant RuleListModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation != Qt::Horizontal || role != Qt::DisplayRole ) {
        return QAbstractTableModel::headerData( section, orientation, role );
    }
    switch ( section ) {
    case KeyColumn:
        return tr( "Key" );
    case LinePatternColumn:
        return tr( "Line Pattern" );
    default:
        return {};
    }
}

Qt::ItemFlags RuleListModel::flags( const QModelIndex& index ) const
{
    if ( !index.isValid() ) {
        // Rules are dropped between rules, never onto one.
        return Qt::ItemIsDropEnabled;
    }
    // Edited in the detail panel; only the enabled check box is in the list.
    Qt::ItemFlags itemFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled
                              | Qt::ItemNeverHasChildren;
    if ( index.column() == KeyColumn ) {
        itemFlags |= Qt::ItemIsUserCheckable;
    }
    return itemFlags;
}

bool RuleListModel::setData( const QModelIndex& index, const QVariant& value, int role )
{
    if ( !index.isValid() || index.row() >= rules_.size() || index.column() != KeyColumn
         || role != Qt::CheckStateRole ) {
        return false;
    }
    const bool enabled = value.toInt() == Qt::Checked;
    auto& entry = rules_[ index.row() ].entry;
    if ( entry.enabled != enabled ) {
        entry.enabled = enabled;
        emitRowChanged( index.row() );
        Q_EMIT ruleChanged( index.row() );
    }
    return true;
}

bool RuleListModel::removeRows( int row, int count, const QModelIndex& parent )
{
    if ( parent.isValid() || row < 0 || count <= 0 || row + count > rules_.size() ) {
        return false;
    }
    beginRemoveRows( QModelIndex(), row, row + count - 1 );
    rules_.remove( row, count );
    endRemoveRows();
    return true;
}

bool RuleListModel::moveRows( const QModelIndex& sourceParent, int sourceRow, int count,
                              const QModelIndex& destinationParent, int destinationChild )
{
    const int size = static_cast<int>( rules_.size() );
    if ( sourceParent.isValid() || destinationParent.isValid() || count <= 0 || sourceRow < 0
         || sourceRow + count > size || destinationChild < 0 || destinationChild > size
         || ( destinationChild >= sourceRow && destinationChild <= sourceRow + count ) ) {
        return false;
    }
    if ( !beginMoveRows( QModelIndex(), sourceRow, sourceRow + count - 1, QModelIndex(),
                         destinationChild ) ) {
        return false;
    }
    const auto moved = rules_.mid( sourceRow, count );
    rules_.remove( sourceRow, count );
    const int insertAt = destinationChild > sourceRow ? destinationChild - count : destinationChild;
    for ( int i = 0; i < count; ++i ) {
        rules_.insert( insertAt + i, moved.at( i ) );
    }
    endMoveRows();
    return true;
}

Qt::DropActions RuleListModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions RuleListModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList RuleListModel::mimeTypes() const
{
    return { RuleRowsMimeType };
}

QMimeData* RuleListModel::mimeData( const QModelIndexList& indexes ) const
{
    // One index per dragged cell; each row once, in order.
    QList<int> rows;
    for ( const auto& index : indexes ) {
        if ( index.isValid() && index.model() == this && !rows.contains( index.row() ) ) {
            rows.append( index.row() );
        }
    }
    if ( rows.isEmpty() ) {
        return nullptr;
    }
    std::sort( rows.begin(), rows.end() );

    QByteArray encoded;
    QDataStream stream( &encoded, QIODevice::WriteOnly );
    stream << dragToken_ << static_cast<qint32>( rows.size() );
    for ( const int row : std::as_const( rows ) ) {
        stream << static_cast<qint32>( row );
    }
    auto* data = new QMimeData;
    data->setData( RuleRowsMimeType, encoded );
    return data;
}

std::optional<RuleListModel::DraggedRows> RuleListModel::draggedRows( const QMimeData* data ) const
{
    if ( !data || !data->hasFormat( RuleRowsMimeType ) ) {
        return std::nullopt;
    }
    QDataStream stream( data->data( RuleRowsMimeType ) );
    quint64 token = 0;
    qint32 size = 0;
    stream >> token >> size;
    if ( stream.status() != QDataStream::Ok || token != dragToken_ || size <= 0 ) {
        return std::nullopt;
    }
    DraggedRows rows;
    rows.count = size;
    for ( qint32 i = 0; i < size; ++i ) {
        qint32 row = -1;
        stream >> row;
        // The list selects one rule; a drag of several must be one block.
        if ( stream.status() != QDataStream::Ok || row < 0 || row >= rules_.size()
             || ( i > 0 && row != rows.first + i ) ) {
            return std::nullopt;
        }
        if ( i == 0 ) {
            rows.first = row;
        }
    }
    return rows;
}

bool RuleListModel::canDropMimeData( const QMimeData* data, Qt::DropAction action, int, int,
                                     const QModelIndex& parent ) const
{
    // Rules are only moved, and only between rules: none has children.
    return action == Qt::MoveAction && !parent.isValid() && draggedRows( data ).has_value();
}

bool RuleListModel::dropMimeData( const QMimeData* data, Qt::DropAction action, int row, int,
                                  const QModelIndex& parent )
{
    if ( action != Qt::MoveAction || parent.isValid() ) {
        return false;
    }
    const auto dragged = draggedRows( data );
    if ( !dragged ) {
        return false;
    }

    // Without a row, e.g. below the last rule, the rules go to the end.
    const int size = static_cast<int>( rules_.size() );
    const int before = row < 0 ? size : std::min( row, size );
    if ( before >= dragged->first && before <= dragged->first + dragged->count ) {
        return false; // Dropped where it already is.
    }

    Q_EMIT aboutToDropRules();
    return moveRows( QModelIndex(), dragged->first, dragged->count, QModelIndex(), before );
}

void RuleListModel::emitRowChanged( int row )
{
    Q_EMIT dataChanged( index( row, 0 ), index( row, ColumnCount - 1 ) );
}

} // namespace custom_footer
