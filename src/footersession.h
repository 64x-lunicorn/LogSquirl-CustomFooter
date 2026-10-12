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

#include <QString>

#include <memory>

namespace custom_footer {

class FooterController;
class FooterDisplayWidget;

/**
 * Everything one loaded footer plugin needs: the rules and the line limit
 * in the config dir, the active file, the controller that fills the footer,
 * and the rule editor from when it opens until it is deleted.
 *
 * The rules are read and written only here. The footer and the editor
 * always see the same active file, which is pushed in with setActiveFile()
 * and never asked of the host. All members run on the UI thread only.
 */
class FooterSession {
public:
    /// Loads the rules and the line limit from @p configDir and starts the
    /// controller on @p footer. Without a config dir the defaults are used,
    /// nothing is saved, and that is logged once. @p footer is not owned
    /// and must outlive the session.
    FooterSession( const QString& configDir, FooterDisplayWidget* footer, HostFunctions host = {} );

    /// Shuts down, in this order: stops the controller, which cancels a
    /// running scan and waits for it, so that the footer is no longer
    /// updated.
    ///
    /// When it returns, no plugin code runs: no thread, timer, queued event
    /// or callback reaches the session or anything it made. The host may
    /// unload the library.
    ~FooterSession();

    FooterSession( const FooterSession& ) = delete;
    FooterSession& operator=( const FooterSession& ) = delete;

    /// The one active file, for the footer. Empty: none. The same path
    /// again scans it again for changes.
    void setActiveFile( const QString& filePath );

private:
    const QString configDir_;
    const HostFunctions host_;
    QString activeFile_;
    std::unique_ptr<FooterController> controller_;
};

} // namespace custom_footer
