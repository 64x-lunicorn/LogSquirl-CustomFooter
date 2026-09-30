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
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class QDialogButtonBox;
class QLabel;
class QToolButton;

namespace custom_footer {

class RuleDetailPanel;
class RuleListModel;
class RuleListView;
class RuleTemplateDialog;
class RulePreviewer;

/**
 * Modal dialog for editing footer extraction rules.
 *
 * Layout:
 *   - Left: the rule list (RuleListModel in a RuleListView), one row per rule
 *     with its enabled check box, key and line pattern, and below it the
 *     [+] [From template…] [-] [↑] [↓] [Import] [Export] buttons. Rules are reordered with
 *     ↑/↓, Ctrl+Shift+Up/Down in the list, or by dragging them
 *   - Right: the RuleDetailPanel for the selected rule, with all its fields
 *     and value mappings
 *   - Below: the problems of all rules, and OK / Cancel / Apply
 *
 * The rules live in the model, one row each with its mappings and problems,
 * so they stay together whichever way the list changes. Edits in the panel
 * are stored into the selected rule as they are made.
 *
 * Rules are validated as they are edited: an invalid pattern, or an enabled
 * rule with a line pattern but no key, is marked at its field and in the
 * list and blocks OK and Apply. Rules may share a key: they are
 * alternatives for its value.
 *
 * The panel previews the selected rule against the active file, which the
 * plugin passes in with setActiveFile(): shortly after an edit stops, a
 * RulePreviewer scans the file on a worker thread. Closing or destroying
 * the dialog cancels a running preview and waits for it.
 */
class FooterEditor : public QDialog {
    Q_OBJECT

public:
    explicit FooterEditor( const QList<FooterEntry>& entries, QWidget* parent = nullptr );
    ~FooterEditor() override;

    /// The log file the selected rule is previewed against; empty for none.
    /// The plugin sets it, and again whenever the active file changes.
    void setActiveFile( const QString& filePath );
    QString activeFile() const;

    /// The footer's line limit, which the preview scans with too.
    void setMaxLines( int maxLines );

    /// Cancel a scheduled or running preview and wait for the worker, e.g.
    /// before the plugin is unloaded.
    void stopPreview();

    /// Stops the preview before closing.
    void done( int result ) override;

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

    /// How many times a rule has been validated so far. An edit validates
    /// only the edited rule; tests check that with this.
    int ruleValidations() const
    {
        return ruleValidations_;
    }

Q_SIGNALS:
    /// Emitted when the user clicks Apply.
    void applied();

private Q_SLOTS:
    void addEntry();
    /// Open the template dialog; a chosen template is added as a new rule.
    void addFromTemplate();
    void addTemplateRule();
    void removeEntry();
    void moveEntryUp();
    void moveEntryDown();
    void updateButtons();
    void importRules();
    void exportRules();
    void showCurrentRule();
    void storePanelInCurrentRule();
    void ruleChanged( int row );

private:
    int currentRow() const;
    void selectRow( int row );
    /// Append @p entry as a new rule, select it and focus its key.
    void appendAndSelect( const FooterEntry& entry );
    void moveEntry( int from, int to );

    /// Validate one rule and store its problems in the model.
    void validateRow( int row );
    /// Validate the rules in rows @p first to @p last, e.g. new ones.
    void validateRows( int first, int last );
    /// List the problems of all rules, with their current row numbers, from
    /// the problems stored in the model. Validates nothing.
    void listProblems();
    /// Preview the selected rule once edits pause, or show that none is.
    void schedulePreview();
    /// Show the listed problems below the list, at the panel's fields, and
    /// allow OK and Apply only without any.
    void showProblems();

    /// Why a pattern does not compile, remembered for the dialog's lifetime,
    /// so that validating compiles only patterns not seen before.
    QString patternError( const QString& pattern );
    QHash<QString, QString> patternErrors_;
    /// The problem lines of each invalid rule, by row. Updated for the rule
    /// being edited, and listed again when rows are inserted, removed or
    /// moved, so an edit costs as much with one rule as with a thousand.
    QMap<int, QStringList> problemLines_;
    int patternCompilations_ = 0;
    int ruleValidations_ = 0;

    RuleListModel* model_ = nullptr;
    RuleListView* list_ = nullptr;
    RuleDetailPanel* panel_ = nullptr;
    RulePreviewer* previewer_ = nullptr;

    QToolButton* addButton_ = nullptr;
    QToolButton* templateButton_ = nullptr;
    /// Made on first use and kept, so each opening starts from reset().
    RuleTemplateDialog* templateDialog_ = nullptr;
    QToolButton* removeButton_ = nullptr;
    QToolButton* upButton_ = nullptr;
    QToolButton* downButton_ = nullptr;
    QToolButton* importButton_ = nullptr;
    QToolButton* exportButton_ = nullptr;
    QLabel* problemLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};

} // namespace custom_footer
