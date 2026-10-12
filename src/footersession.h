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

#include "hostfunctions.h"

#include <QPointer>
#include <QString>

#include <memory>

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
 */
class FooterSession {
public:
    /// Loads the rules and the line limit from @p configDir and starts the
    /// controller on @p footer. Without a config dir the defaults are used
    /// and nothing is saved. @p footer is not owned and must outlive the
    /// session.
    FooterSession( const QString& configDir, FooterDisplayWidget* footer, HostFunctions host = {} );

    /// Shuts down, in this order: deletes the editor at once, open or closed
    /// but not yet deleted, with its dialogs, its preview (stopped and
    /// waited for), its timers and its pending events; then stops the
    /// controller, which cancels a running scan and waits for it, so that
    /// the footer is no longer updated.
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

    /// Whether the running host offers @p feature: its member of the
    /// HostFunctions the session was made with is set.
    bool has( HostFeature feature ) const noexcept;

private:
    /// Save the editor's rules and show them in the footer. Logs instead
    /// of throwing, as it runs in a handler of the editor's signals.
    void saveFrom( const FooterEditor& editor ) noexcept;

    const QString configDir_;
    const HostFunctions host_;
    QString activeFile_;
    std::unique_ptr<FooterController> controller_;
    /// The editor from when it is opened until it is deleted.
    QPointer<FooterEditor> editor_;
};

} // namespace custom_footer
