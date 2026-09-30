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
#include "rulelistmodel.h"

#include <QGroupBox>
#include <QHash>

class QCheckBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QTableWidget;
class QToolButton;

namespace custom_footer {

/**
 * The form for one rule: enabled, key, line pattern, value pattern and the
 * value mappings, each field with the reason it keeps the rules from being
 * saved.
 *
 * The panel only edits a copy: every change emits edited(), and the owner
 * stores entry() into its rule. showEntry() does not emit edited().
 *
 * The layout is a column of sections, so that more of them (a live
 * preview, a simple mode for the patterns) can be added below the form.
 */
class RuleDetailPanel : public QGroupBox {
    Q_OBJECT

public:
    explicit RuleDetailPanel( QWidget* parent = nullptr );

    /// Show @p entry for editing and enable the panel.
    void showEntry( const FooterEntry& entry );

    /// Show no rule: the panel is emptied and disabled.
    void showNoEntry();

    /// The rule as edited in the panel.
    FooterEntry entry() const;

    /// Mark the fields of the shown rule that have problems.
    void setProblems( const RuleProblems& problems );

    /// Put the focus into the key field, e.g. for a new rule.
    void focusKey();

    /// Store a mapping cell still being edited, as if its editor had lost
    /// the focus. Buttons without focus, such as tool buttons, and OK on
    /// macOS, do not end the edit themselves.
    void commitPendingEdit();

protected:
    /// Return and Enter in a field only confirm it, and Escape reverts it,
    /// instead of accepting or rejecting the whole dialog.
    bool eventFilter( QObject* watched, QEvent* event ) override;

Q_SIGNALS:
    /// The user changed the rule in the panel.
    void edited();

private Q_SLOTS:
    void addMapping();
    void removeMapping();
    void updateMappingButtons();

private:
    /// A line edit in the form, with the label below it that says why its
    /// content keeps the rules from being saved.
    QLineEdit* addField( QFormLayout* form, const QString& label, const QString& objectName,
                         QLabel** problemLabel );
    void setMappings( const QList<ValueMapping>& mappings );

    QCheckBox* enabledCheck_ = nullptr;
    QLineEdit* keyEdit_ = nullptr;
    QLineEdit* linePatternEdit_ = nullptr;
    QLineEdit* valuePatternEdit_ = nullptr;
    QLabel* keyProblem_ = nullptr;
    QLabel* linePatternProblem_ = nullptr;
    QLabel* valuePatternProblem_ = nullptr;
    /// What Escape reverts each field to: its text when the rule was shown,
    /// the field got the focus, or Return was pressed in it.
    QHash<QLineEdit*, QString> revertText_;

    QTableWidget* mappingTable_ = nullptr;
    QToolButton* addMappingButton_ = nullptr;
    QToolButton* removeMappingButton_ = nullptr;
};

} // namespace custom_footer
