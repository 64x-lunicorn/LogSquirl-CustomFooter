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

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QList>
#include <QTableWidget>
#include <QToolButton>

namespace custom_footer {

/**
 * Modal dialog for editing footer extraction rules.
 *
 * Layout:
 *   - QTableWidget with five columns: Enabled, Key, Line Pattern, Value Pattern, Mappings
 *   - Toolbar row with [+] [-] [↑] [↓] [Import] [Export] buttons
 *   - Inline mapping editor panel below the table
 *   - QDialogButtonBox with OK / Cancel / Apply
 *
 * Rules are validated as they are edited: an invalid pattern, or an enabled
 * rule with a line pattern but no key, is marked in its cell and blocks OK
 * and Apply. Rules may share a key: they are alternatives for its value.
 */
class FooterEditor : public QDialog {
    Q_OBJECT

public:
    explicit FooterEditor( const QList<FooterEntry>& entries, QWidget* parent = nullptr );

    /// Return the edited list of entries.
    QList<FooterEntry> entries() const;

    /// Append entries after the existing ones, as an import does.
    void appendEntries( const QList<FooterEntry>& entries );

    /// How many patterns validation has compiled so far. Unchanged patterns
    /// are not compiled again; tests check that with this.
    int patternCompilations() const
    {
        return patternCompilations_;
    }

Q_SIGNALS:
    /// Emitted when the user clicks Apply.
    void applied();

private Q_SLOTS:
    void addEntry();
    void removeEntry();
    void moveEntryUp();
    void moveEntryDown();
    void updateButtons();
    void importRules();
    void exportRules();
    void showMappingsOfCurrentRule();
    void addMapping();
    void removeMapping();
    void storeMappingsOfCurrentRule();
    void validate();

private:
    void populateTable( const QList<FooterEntry>& entries );
    void setRow( int row, const FooterEntry& entry );
    void moveEntry( int from, int to );

    /// The mappings of a rule live in its Mappings cell, so they stay with
    /// the rule's row whichever way the table changes.
    QList<ValueMapping> mappingsOf( int row ) const;
    void setMappings( int row, const QList<ValueMapping>& mappings );

    /// Why a pattern does not compile, remembered from the last validate(),
    /// so that validating compiles only the patterns edited since.
    QString patternError( const QString& pattern, QHash<QString, QString>& errors );
    QHash<QString, QString> patternErrors_;
    int patternCompilations_ = 0;

    QTableWidget* table_ = nullptr;
    QToolButton* addButton_ = nullptr;
    QToolButton* removeButton_ = nullptr;
    QToolButton* upButton_ = nullptr;
    QToolButton* downButton_ = nullptr;
    QToolButton* importButton_ = nullptr;
    QToolButton* exportButton_ = nullptr;
    QLabel* problemLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;

    // Mapping editor panel, showing the mappings of the current rule.
    QGroupBox* mappingGroup_ = nullptr;
    QTableWidget* mappingTable_ = nullptr;
    QToolButton* addMappingButton_ = nullptr;
    QToolButton* removeMappingButton_ = nullptr;
};

} // namespace custom_footer
