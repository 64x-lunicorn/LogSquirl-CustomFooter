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
#include "hostfunctions.h"
#include "ruledetailpanel.h"
#include "rulepreviewer.h"

#include <QDialog>
#include <QList>
#include <QPersistentModelIndex>
#include <QString>

#include <optional>

class QDialogButtonBox;
class QLabel;
class QToolButton;

namespace custom_footer {

class RuleDetailPanel;
class RuleListModel;
class RuleListView;
class RuleTemplateDialog;
class SelectedLinePicker;

/**
 * Modal dialog for editing footer extraction rules.
 *
 * Layout:
 *   - Left: the rule list (RuleListModel in a RuleListView), one row per rule
 *     with its enabled check box, key and line pattern, and below it the
 *     [+] [From template…] [From selected line…] [-] [↑] [↓] [Import] [Export]
 *     buttons. Rules are reordered with ↑/↓, Ctrl+Shift+Up/Down in the list,
 *     or by dragging them. [From selected line…] is shown only where
 *     offerRuleFromLine() offers it
 *   - Right: the RuleDetailPanel for the selected rule, with all its fields
 *     and value mappings; above it, for a rule made from a Log Line, that
 *     line, to click or mark the value in
 *   - Below: the problems of all rules, and OK / Cancel / Apply
 *
 * The rules live in the model, one row each with its mappings and problems,
 * so they stay together whichever way the list changes. Edits in the panel
 * are stored into the selected rule as they are made.
 *
 * Rules are checked as they are edited, by the RuleCheck that holds the
 * model's rows: the editor only shows what it finds. A problem, such as an
 * invalid pattern or an enabled rule with a line pattern but no key, is
 * marked at its field and in the list, listed below it, and blocks OK and
 * Apply. Rules may share a key: they are alternatives for its value.
 *
 * The panel previews the selected rule against the active file, which the
 * plugin passes in with setActiveFile(): shortly after an edit stops, a
 * RulePreviewer scans the file on a worker thread, and again when the file
 * changes. Closing or destroying the dialog cancels a running preview and
 * waits for it; nothing restarts it afterwards, not even an edit committed
 * while the dialog closes.
 *
 * Test… next to a pattern opens it in LogSquirl's Regex Lab, through the
 * function the plugin passes in with setRegexLab(). One Lab is open for
 * the editor at a time, and Test… is disabled meanwhile. Apply in the Lab
 * writes the pattern into the rule it was opened for, as if typed there,
 * also when another rule is selected or the rule has moved; Cancel, an
 * Apply of the pattern as it was opened, a rule removed meanwhile, or a
 * closed editor drop the answer. A rule has no match-case flag: the Lab
 * gets "Match case" on, or off for a pattern that starts with (?i) (after
 * any start-of-pattern items such as (*CRLF)), which the Lab gets without
 * it; an applied pattern gets (?i) there when "Match case" came back off.
 *
 * A rule made from a Log Line (addRuleFromLine()) starts empty, in simple
 * mode, with the line shown above the panel. Each click or mark in the line
 * makes the rule again (ruleFromLine()): the text before the value, where
 * the value ends, and the suggested key, unless the user changed the key.
 * The line stays with its rule when rules move, and goes with it; it is
 * shown only while that rule is selected. One line at a time: a rule made
 * from another line takes its place. The line is editor state, never saved.
 * While the Regex Lab is open, marks are not taken, as its answer may be
 * for this rule.
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

    /// How Test… opens the Regex Lab, with this editor as the context. Empty,
    /// as by default: the host has none, and Test… is disabled, saying that
    /// it needs LogSquirl 26.11 or later.
    void setRegexLab( OpenRegexLab openRegexLab );

    /// Stops the preview for good before closing.
    void done( int result ) override;

    /// Return the edited list of entries.
    QList<FooterEntry> entries() const;

    /// Append entries after the existing ones, as an import does.
    void appendEntries( const QList<FooterEntry>& entries );

    /// Show the [From selected line…] button, which emits
    /// ruleFromLineRequested(), or hide it. Hidden by default.
    void offerRuleFromLine( bool offered );

    /// Append a new, empty rule in simple mode, select it, and show @p line
    /// above the panel to click or mark the value in. @p selectedLines is
    /// how many Log Lines were selected, of which @p line is the first.
    void addRuleFromLine( const QString& line, int selectedLines = 1 );

Q_SIGNALS:
    /// Emitted when the user clicks Apply.
    void applied();

    /// Emitted when the user clicks [From selected line…]: the owner reads
    /// the selected Log Line and calls addRuleFromLine(), or says why not.
    void ruleFromLineRequested();

private Q_SLOTS:
    void addEntry();
    /// Open the template dialog; each checked template is added as a new
    /// rule.
    void addFromTemplate();
    void addTemplateRules();
    void removeEntry();
    void moveEntryUp();
    void moveEntryDown();
    void updateButtons();
    void importRules();
    void importFrom( const QString& filePath );
    void exportRules();
    void exportTo( const QString& filePath );
    void showCurrentRule();
    void storePanelInCurrentRule();
    void ruleChanged( int row );
    /// Open the selected rule's pattern in @p field in the Regex Lab.
    void testPattern( PatternField field );
    /// Make the rule from its line again, for the value marked in it.
    void lineMarked( qsizetype start, qsizetype length );

private:
    int currentRow() const;
    void selectRow( int row );
    /// Append @p entries as new rules, select the first and focus its key.
    void appendAndSelect( const QList<FooterEntry>& entries );
    void moveEntry( int from, int to );
    /// Whether @p row is the rule made from the line the picker shows.
    bool isLineRule( int row ) const;
    /// Tell at the picker when the preview of the rule made from its line
    /// finds its first match in another Log Line.
    void showLineRulePreview( const FooterScanner::Preview& preview );

    /// Show a warning without blocking: opened with open().
    void showError( const QString& title, const QString& text, const QString& objectName );

    /// The Regex Lab's answer for the pattern in @p field of @p rule, the
    /// rule it was opened for, wherever it is now; @p opened is what the Lab
    /// was opened with.
    void regexLabAnswered( const QPersistentModelIndex& rule, PatternField field,
                           const RegexLabPattern& opened,
                           const std::optional<RegexLabPattern>& applied );
    /// Note whether a Regex Lab opened by Test… awaits its answer, and offer
    /// Test… accordingly.
    void setRegexLabOpen( bool open );
    /// Offer Test… only when the Regex Lab can be opened.
    void updatePatternTest();

    /// Preview the selected rule, or show that none is.
    void schedulePreview( RulePreviewer::Start start = RulePreviewer::Start::AfterPause );
    /// Show the check's listing below the list and the selected rule's
    /// problems at the panel's fields, and allow OK and Apply only when the
    /// rules can be saved.
    void showProblems();

    RuleListModel* model_ = nullptr;
    RuleListView* list_ = nullptr;
    RuleDetailPanel* panel_ = nullptr;
    RulePreviewer* previewer_ = nullptr;
    /// The dialog is closing: no more previews, no more Regex Lab answers.
    bool closing_ = false;
    OpenRegexLab openRegexLab_;
    /// A Regex Lab opened by Test… has not answered yet.
    bool regexLabOpen_ = false;

    QToolButton* addButton_ = nullptr;
    QToolButton* templateButton_ = nullptr;
    /// Made on first use and kept, so each opening starts from reset().
    RuleTemplateDialog* templateDialog_ = nullptr;
    QToolButton* ruleFromLineButton_ = nullptr;
    /// The Log Line of the rule made from one, shown above the panel.
    SelectedLinePicker* linePicker_ = nullptr;
    /// That rule's row; invalid once it is removed or no rule has a line.
    QPersistentModelIndex lineRule_;
    /// The key last suggested from the line: replaced by the next one
    /// unless the user changed it.
    QString lineKey_;
    QToolButton* removeButton_ = nullptr;
    QToolButton* upButton_ = nullptr;
    QToolButton* downButton_ = nullptr;
    QToolButton* importButton_ = nullptr;
    QToolButton* exportButton_ = nullptr;
    QLabel* problemLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};

} // namespace custom_footer
