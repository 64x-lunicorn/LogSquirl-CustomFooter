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

#include "filefollower.h"
#include "footerdisplaywidget.h"
#include "footerscanner.h"

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <memory>

namespace custom_footer {

/**
 * Keeps a FooterDisplayWidget up to date with the active log file.
 *
 * The rules are loaded and compiled once, and again on reloadConfig(). The
 * active file is followed by a FileFollower, which scans it on a worker
 * thread. Another active file, or reloaded rules, cancel the running scan,
 * whose values are then never shown, and scan afresh at once. A change of
 * the active file on disk, or the same file made active again, does not
 * cancel: the running scan finishes and shows its values, and one more
 * scan covers every change made meanwhile, a FollowerParts::kGapMs pause
 * after it, so that the values of a log that changes faster than it is
 * scanned still show up and the file is never scanned back to back.
 *
 * Once scanned, a file that only grew is scanned on from where the last
 * scan stopped, and not at all once every key has a value or a limit was
 * reached; nor is a file left as it was (FooterScanner::fileUnchangedFor()).
 * A file that was truncated or replaced is scanned from its start. While
 * the active file is missing, e.g. after it was rotated away, it is waited
 * for, and scanned and followed again once it is recreated.
 *
 * Problems of the rules, such as an invalid pattern, are reported to the
 * warning function it is given, each time the rules are loaded.
 *
 * Destroying the controller cancels a running scan and waits for it: no code
 * of the plugin runs on the worker thread afterwards, and no wake-up of the
 * follower is pending.
 */
class FooterController : public QObject {
    Q_OBJECT

public:
    /// Receives a problem of the rules, in plain English. May be empty.
    using Warn = std::function<void( const QString& problem )>;

    /// @p parts are the clock and change source of the active file's
    /// follower; tests pass their own.
    FooterController( FooterDisplayWidget* widget, const QString& configDir, Warn warn = {},
                      QObject* parent = nullptr, FollowerParts parts = {} );
    ~FooterController() override;

    const QString& configDir() const
    {
        return configDir_;
    }

    /// Show the values of this file; an empty path clears the footer. For
    /// the file already active, it is scanned again for changes, as for a
    /// change on disk.
    void setActiveFile( const QString& filePath );

    /// Load the rules again, e.g. after they were saved, and rescan.
    void reloadConfig();

    /// Whether the active file is missing, and its appearance is waited for.
    bool isWaitingForActiveFile() const
    {
        return follower_.status().waitingForFile;
    }

    /// How many scans were started, and how many of them finished, shown
    /// or dropped as outdated; for tests.
    int scansStarted() const
    {
        return follower_.status().started;
    }
    int scansFinished() const
    {
        return follower_.status().finished;
    }

private:
    using Values = QList<FooterValue>;

    void loadConfig();
    /// Cancel a running scan and follow the active file afresh, with the
    /// current rules.
    void restartScan();
    void show( const Values& values );

    QPointer<FooterDisplayWidget> widget_;
    const QString configDir_;
    const Warn warn_;
    QString activeFile_;

    std::shared_ptr<const FooterScanner> scanner_;
    int maxLines_ = FooterScanner::kDefaultMaxLines;

    /// The scans of the active file, on a worker thread, followed when it
    /// changes. The last scan shown is the next one's previous: how far the
    /// file has been scanned with the current rules. Declared last, so that
    /// it stops before anything its sink touches goes.
    FileFollower<FooterScanner::Scan> follower_;
};

} // namespace custom_footer
