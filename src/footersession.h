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

#include "footervalue.h"
#include "hostfunctions.h"

#include <QMetaObject>
#include <QPointer>
#include <QString>

#include <memory>

class QMessageBox;
class QWidget;

namespace custom_footer {

class FooterController;
class FooterDisplayWidget;
class FooterEditor;

/**
 * Everything one loaded footer plugin needs: the rules and the line limit
 * in the config dir, the active file, the controller that fills the footer,
 * and the rule editor from when it opens until it is deleted.
 *
 * The rules are read and written only by the session and its controller;
 * the editor gets them from the session and hands them back through its
 * signals, and the C entry points never touch them. The footer and the editor
 * always see the same active file, which is pushed in with setActiveFile()
 * and never asked of the host. All members run on the UI thread only.
 *
 * Where the host can go to a Log Line, the footer's values offer it, and the
 * session asks the host to go to the line a value was found in; when the
 * host cannot, e.g. as the line is no longer in the open file, a short notice
 * says so at the value. On an older host the values do not offer it.
 */
class FooterSession {
public:
    /// Loads the rules and the line limit from @p configDir and starts the
    /// controller on @p footer. Without a config dir the defaults are used
    /// and nothing is saved. @p footer is not owned and must outlive the
    /// session. The footer's values offer going to their Log Line where
    /// @p host can.
    FooterSession( const QString& configDir, FooterDisplayWidget* footer, HostFunctions host = {} );

    /// Shuts down, in this order: deletes the editor at once, open or closed
    /// but not yet deleted, with its dialogs, its preview (stopped and
    /// waited for), its timers and its pending events; then stops the
    /// controller, which cancels a running scan and waits for it, so that
    /// the footer is no longer updated; then disconnects from the footer,
    /// whose values no longer offer going to their Log Line.
    ///
    /// When it returns, no plugin code runs: no thread, timer, queued event
    /// or callback reaches the session or anything it made. The host may
    /// unload the library.
    ~FooterSession();

    FooterSession( const FooterSession& ) = delete;
    FooterSession& operator=( const FooterSession& ) = delete;

    /// The one active file, for the footer and the editor, also one opened
    /// later. Empty: none. The same path again scans it again for changes.
    void setActiveFile( const QString& filePath );

    /**
     * Open the rule editor over @p over, or over the application-modal
     * window that blocks it, where it is usable; or move the open one there
     * and raise it. Returns immediately: the editor never runs a nested
     * event loop.
     *
     * The editor starts with the rules and the line limit of the config dir
     * and the active file. OK and Apply save its rules and show them in the
     * footer; a failed save is logged as a warning, and the rules are loaded
     * again all the same. A closed editor is deleted later. Its Test…
     * opens the host's Regex Lab, where the host offers it; a failure to
     * open it is logged.
     *
     * The returned editor is valid until it is deleted after closing, or
     * the session is destroyed.
     */
    FooterEditor* openEditor( QWidget* over );

    /**
     * Start a new rule from the Log Line selected in the host, as
     * "Rule from selected line…" does: open the editor as openEditor()
     * does, or bring the open one forward with its edits, and add a new
     * simple rule there with that line shown, for the user to click or mark
     * the value in. Of several selected lines the first is taken. Returns
     * the editor, valid as openEditor()'s.
     *
     * Without a selected Log Line (or an open Log File), no rule is added:
     * a message over the open editor, or else over @p over, says what to
     * do, and nullptr is returned. The message is opened without blocking,
     * shown once however often this is called, and deleted with the
     * session. Does nothing and returns nullptr when the host does not offer
     * the selected Log Lines (has( HostFeature::SelectedLogLines )).
     *
     * The editor offers the same as its [From selected line…] button,
     * where the host offers it.
     */
    FooterEditor* ruleFromSelectedLine( QWidget* over );

    /// Whether the running host offers @p feature: its member of the
    /// HostFunctions the session was made with is set.
    bool has( HostFeature feature ) const noexcept;

private:
    /// Save the editor's rules and show them in the footer. Logs instead
    /// of throwing, as it runs in a handler of the editor's signals.
    void saveFrom( const FooterEditor& editor ) noexcept;

    /// Ask the host to go to the Log Line of @p value, and tell the user at
    /// the value when it cannot. Logs instead of throwing, as it runs in a
    /// handler of the footer's signal.
    void goToLine( const FooterValue& value ) noexcept;

    /// The Log Lines selected in the host, read now; nothing, after the
    /// message why over @p messageOver, without a selected Log Line.
    std::optional<SelectedLogLines> selectedLinesOrSayWhy( QWidget* messageOver );

    /// Say, over @p over, how to make a rule from a selected line: to open
    /// a Log File first without an active file; over the editor, to keep
    /// the rules with OK or Apply and close it first, as it blocks the
    /// window that takes the selection.
    void showNoSelectedLine( QWidget* over );

    const QString configDir_;
    const HostFunctions host_;
    QString activeFile_;
    QPointer<FooterDisplayWidget> footer_;
    /// The footer's requests to go to a Log Line, to goToLine().
    QMetaObject::Connection goToLineConnection_;
    std::unique_ptr<FooterController> controller_;
    /// The editor from when it is opened until it is deleted.
    QPointer<FooterEditor> editor_;
    /// The message that no Log Line is selected, while it is open.
    QPointer<QMessageBox> noSelectedLine_;
};

} // namespace custom_footer
