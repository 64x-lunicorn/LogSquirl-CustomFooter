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
#include "simplerule.h"

#include <QGroupBox>
#include <QHash>

#include <optional>

class QCheckBox;
class QComboBox;
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
 * A rule is edited in simple or advanced mode. In simple mode the user
 * gives the text before the value and where the value ends, and the
 * patterns are generated from them (simplerule.h) and shown read-only. In
 * advanced mode the patterns are edited directly. A rule is shown in simple
 * mode exactly when its patterns have the simple form; switching to
 * advanced keeps the patterns, and switching back is offered only while
 * they still have the simple form. The mode is not saved: it follows from
 * the patterns whenever a rule is shown.
 *
 * The panel only edits a copy: every change emits edited(), and the owner
 * stores entry() into its rule. showEntry() does not emit edited().
 *
 * The layout is a column of sections, so that more of them, such as a
 * live preview, can be added below the form.
 */
class RuleDetailPanel : public QGroupBox {
    Q_OBJECT

public:
    explicit RuleDetailPanel( QWidget* parent = nullptr );

    /// Show @p entry for editing and enable the panel.
    /// An @p unfinished simple rule is shown in its simple fields instead
    /// of what the entry's patterns say.
    void showEntry( const FooterEntry& entry,
                    const std::optional<SimpleRule>& unfinished = std::nullopt );

    /// Show no rule: the panel is emptied and disabled.
    void showNoEntry();

    /// Show that the rule was enabled or disabled elsewhere, e.g. in the
    /// list, keeping the rest of the panel, its mode included, as it is.
    void showEnabled( bool enabled );

    /// The rule as edited in the panel.
    FooterEntry entry() const;

    /// The simple rule being written, if its patterns cannot be generated
    /// yet: its end character is missing or unusable. entry() then has
    /// empty patterns.
    std::optional<SimpleRule> unfinishedSimpleRule() const;

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
    void advancedToggled( bool advanced );
    void simpleFieldEdited();
    void valueEndChosen();
    void patternEdited();
    void addMapping();
    void removeMapping();
    void updateMappingButtons();

private:
    /// A line edit in the form, with the label below it that says why its
    /// content keeps the rules from being saved.
    QLineEdit* addField( QFormLayout* form, const QString& label, const QString& objectName,
                         QLabel** problemLabel );
    /// Handle Return and Escape in @p field, as in every field of the form.
    void watchField( QLineEdit* field );
    /// A hidden label for why a field keeps the rules from being saved.
    QLabel* newProblemLabel( const QString& objectName );
    void setMappings( const QList<ValueMapping>& mappings );

    /// Show @p rule in the simple fields, without emitting edited().
    void setSimpleFields( const SimpleRule& rule );
    /// The simple rule as given in the simple fields.
    SimpleRule simpleRule() const;
    /// Show the fields of the mode, and whether the mode can be switched.
    void setAdvanced( bool advanced );
    /// Offer switching to simple mode only while the patterns allow it.
    void updateAdvancedCheck();

    QCheckBox* enabledCheck_ = nullptr;
    QLineEdit* keyEdit_ = nullptr;
    QLineEdit* linePatternEdit_ = nullptr;
    QLineEdit* valuePatternEdit_ = nullptr;
    QLabel* keyProblem_ = nullptr;
    QLabel* linePatternProblem_ = nullptr;
    QLabel* valuePatternProblem_ = nullptr;

    QCheckBox* advancedCheck_ = nullptr;
    QLineEdit* textBeforeEdit_ = nullptr;
    QComboBox* valueEndCombo_ = nullptr;
    QLineEdit* endCharacterEdit_ = nullptr;
    QLabel* endCharacterProblem_ = nullptr;
    /// What Escape reverts the value end and the Advanced switch to, as
    /// revertText_ does for the line edits.
    int revertValueEnd_ = 0;
    bool revertAdvanced_ = false;
    /// The rows of the simple fields, hidden in advanced mode.
    QList<QWidget*> simpleRows_;
    bool advanced_ = false;
    /// What Escape reverts each field to: its text when the rule was shown,
    /// the field got the focus, or Return was pressed in it.
    QHash<QLineEdit*, QString> revertText_;

    QTableWidget* mappingTable_ = nullptr;
    QToolButton* addMappingButton_ = nullptr;
    QToolButton* removeMappingButton_ = nullptr;
};

} // namespace custom_footer
