/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "footerentry.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QList>
#include <QTableWidget>
#include <QToolButton>

namespace costume_footer {

/**
 * Modal dialog for editing footer extraction rules.
 *
 * Layout modelled after PredefinedFilterSetEdit:
 *   - QTableWidget with three columns: Enabled (checkbox), Key, Regex
 *   - Toolbar row with [+] [-] [↑] [↓] buttons
 *   - ComboBox for DisplayMode selection
 *   - QDialogButtonBox with OK / Cancel / Apply
 */
class FooterEditor : public QDialog {
    Q_OBJECT

  public:
    explicit FooterEditor( const QList<FooterEntry>& entries,
                           DisplayMode mode, QWidget* parent = nullptr );

    /// Return the edited list of entries.
    QList<FooterEntry> entries() const;

    /// Return the selected display mode.
    DisplayMode displayMode() const;

  Q_SIGNALS:
    /// Emitted when the user clicks Apply.
    void applied();

  private Q_SLOTS:
    void addEntry();
    void removeEntry();
    void moveEntryUp();
    void moveEntryDown();
    void updateButtons();

  private:
    void populateTable( const QList<FooterEntry>& entries );
    QList<FooterEntry> tableToEntries() const;

    QTableWidget* table_ = nullptr;
    QToolButton* addButton_ = nullptr;
    QToolButton* removeButton_ = nullptr;
    QToolButton* upButton_ = nullptr;
    QToolButton* downButton_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};

} // namespace costume_footer
