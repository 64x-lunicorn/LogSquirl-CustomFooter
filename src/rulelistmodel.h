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

#pragma once

#include "footerentry.h"
#include "simplerule.h"

#include <QAbstractTableModel>
#include <QColor>
#include <QList>
#include <QString>

#include <optional>

#include <optional>

namespace custom_footer {

/// The colour marking what keeps the rules from being saved, in the rule
/// list and at the fields of the detail panel.
inline QColor problemColor()
{
    return QColor( 220, 50, 50 );
}

/// The same colour as a background that text stays readable on.
inline QColor problemBackgroundColor()
{
    auto color = problemColor();
    color.setAlpha( 70 );
    return color;
}

/// Why a rule keeps the rules from being saved, per field. Empty when the
/// field is fine.
struct RuleProblems {
    QString key;
    QString endCharacter; ///< Of a simple rule's value end.
    QString linePattern;
    QString valuePattern;

    bool isEmpty() const
    {
        return key.isEmpty() && endCharacter.isEmpty() && linePattern.isEmpty()
               && valuePattern.isEmpty();
    }

    bool operator==( const RuleProblems& other ) const
    {
        return key == other.key && endCharacter == other.endCharacter
               && linePattern == other.linePattern && valuePattern == other.valuePattern;
    }
    bool operator!=( const RuleProblems& other ) const
    {
        return !( *this == other );
    }
};

/**
 * The rules of the editor, in their order, each with its validation
 * problems.
 *
 * Every rule is one row holding the whole FooterEntry, mappings included,
 * so a rule's data and marks stay together however the rows are inserted,
 * removed or moved. The list shows the enabled state as the check box of
 * the key column, the key, and the line pattern; the rest is edited in the
 * detail panel through setEntry().
 *
 * Rules are reordered by dragging them within the list: a drop moves the
 * dragged rules with moveRows(), so nothing is left for the view to remove
 * (RuleListView never removes rows after a drag). Only moves of rules
 * dragged from this model are accepted; a rule is never copied.
 */
class RuleListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { KeyColumn, LinePatternColumn, ColumnCount };

    explicit RuleListModel( QObject* parent = nullptr );

    QList<FooterEntry> entries() const;
    const FooterEntry& entry( int row ) const;

    /// Replace all rules. Their problems are cleared.
    void setEntries( const QList<FooterEntry>& entries );

    /// Append rules after the existing ones.
    void appendEntries( const QList<FooterEntry>& entries );

    /// Replace the rule in @p row, e.g. with the one edited in the panel.
    void setEntry( int row, const FooterEntry& entry );

    /// Move the rule in @p from so that it ends up in @p to.
    bool moveRule( int from, int to );

    /// A simple rule being written in the detail panel whose patterns
    /// cannot be generated yet, as its end character is missing or
    /// unusable; its entry's patterns are empty meanwhile. Editor state:
    /// never part of entries(), but kept with the row when rules move.
    std::optional<SimpleRule> unfinishedSimpleRule( int row ) const;
    void setUnfinishedSimpleRule( int row, const std::optional<SimpleRule>& rule );

    RuleProblems problems( int row ) const;
    void setProblems( int row, const RuleProblems& problems );

    int rowCount( const QModelIndex& parent = QModelIndex() ) const override;
    int columnCount( const QModelIndex& parent = QModelIndex() ) const override;
    QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    QVariant headerData( int section, Qt::Orientation orientation,
                         int role = Qt::DisplayRole ) const override;
    Qt::ItemFlags flags( const QModelIndex& index ) const override;
    bool setData( const QModelIndex& index, const QVariant& value,
                  int role = Qt::EditRole ) override;
    bool removeRows( int row, int count, const QModelIndex& parent = QModelIndex() ) override;
    bool moveRows( const QModelIndex& sourceParent, int sourceRow, int count,
                   const QModelIndex& destinationParent, int destinationChild ) override;

    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData( const QModelIndexList& indexes ) const override;
    bool canDropMimeData( const QMimeData* data, Qt::DropAction action, int row, int column,
                          const QModelIndex& parent ) const override;
    /// Move the dragged rules before @p row, or to the end without a row.
    /// Only moves are accepted. Returns false when nothing moved.
    bool dropMimeData( const QMimeData* data, Qt::DropAction action, int row, int column,
                       const QModelIndex& parent ) override;

Q_SIGNALS:
    /// The rule in @p row was enabled or disabled in the list. Not emitted
    /// for setEntry(), whose caller knows what it changed.
    void ruleChanged( int row );

    /// Dragged rules are about to be moved by a drop. Emitted before the
    /// move, so that a pending edit can still be stored in its rule.
    void aboutToDropRules();

private:
    struct Rule {
        FooterEntry entry;
        RuleProblems problems;
        std::optional<SimpleRule> unfinishedSimpleRule = std::nullopt;
    };

    void emitRowChanged( int row );

    struct DraggedRows {
        int first = 0;
        int count = 0;
    };
    /// The dragged block of rows, or none if @p data is not a drag of
    /// contiguous rows from this model.
    std::optional<DraggedRows> draggedRows( const QMimeData* data ) const;

    /// Marks this model's drags, so that no other list takes them. Random,
    /// not an address, so the payload tells nothing about the process.
    quint64 dragToken_;

    QList<Rule> rules_;
};

} // namespace custom_footer
