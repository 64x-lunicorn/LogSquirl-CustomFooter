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
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QStringList>

namespace custom_footer {

namespace {

/// Background of rules that keep the rules from being saved.
const QColor kProblemColor( 220, 50, 50, 70 );

/// A pattern on one line, for the list: the view elides what does not fit.
QString oneLine( QString text )
{
    text.replace( QLatin1Char( '\n' ), QChar( 0x21B5 ) ); // ↵
    text.remove( QLatin1Char( '\r' ) );
    return text;
}

} // namespace

RuleListModel::RuleListModel( QObject* parent )
    : QAbstractTableModel( parent )
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
    Q_EMIT ruleChanged( row );
}

bool RuleListModel::moveRule( int from, int to )
{
    // moveRows() takes the row the rule is moved before.
    return moveRows( QModelIndex(), from, 1, QModelIndex(), to > from ? to + 1 : to );
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
        return rule.problems.isEmpty() ? QVariant() : QVariant( QBrush( kProblemColor ) );
    case Qt::ToolTipRole: {
        QStringList lines;
        if ( !keyColumn && !rule.entry.linePattern.isEmpty() ) {
            lines.append( rule.entry.linePattern );
        }
        for ( const auto& problem :
              { rule.problems.key, rule.problems.linePattern, rule.problems.valuePattern } ) {
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
        return Qt::NoItemFlags;
    }
    // Edited in the detail panel; only the enabled check box is in the list.
    Qt::ItemFlags itemFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
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

void RuleListModel::emitRowChanged( int row )
{
    Q_EMIT dataChanged( index( row, 0 ), index( row, ColumnCount - 1 ) );
}

} // namespace custom_footer
