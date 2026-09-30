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

#include "footerdisplaywidget.h"
#include "footerscanner.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <memory>

namespace custom_footer {

/**
 * Keeps a FooterDisplayWidget up to date with the active log file.
 *
 * The rules are loaded and compiled once, and again on reloadConfig(). Scans
 * run on a worker thread. Another active file, or reloaded rules, cancel the
 * running scan, whose values are then never shown. A change of the active
 * file does not: the running scan finishes and shows its values, and one
 * more scan then covers every change made meanwhile, so that the values of a
 * log that changes faster than it is scanned still show up. The active file
 * is watched, so the values follow a
 * growing log: once scanned, a file that only grew is scanned on from where
 * the last scan stopped, and not at all once every key has a value. A file
 * that was truncated or replaced is scanned from its start. While the active
 * file is missing, e.g. after it was rotated away, its directory is watched
 * so that the file is scanned and watched again once it is recreated.
 *
 * Destroying the controller cancels a running scan and waits for it: no code
 * of the plugin runs on the worker thread afterwards.
 */
class FooterController : public QObject {
    Q_OBJECT

public:
    /// How long file changes are collected before the file is scanned again.
    static constexpr int kRescanDelayMs = 500;

    FooterController( FooterDisplayWidget* widget, const QString& configDir,
                      QObject* parent = nullptr );
    ~FooterController() override;

    const QString& configDir() const
    {
        return configDir_;
    }

    /// Show the values of this file; an empty path clears the footer. For
    /// the file already active, it is scanned again for changes.
    void setActiveFile( const QString& filePath );

    /// Load the rules again, e.g. after they were saved, and rescan.
    void reloadConfig();

    /// Whether the active file is missing and its directory is watched for
    /// it to be created again.
    bool isWaitingForActiveFile() const
    {
        return !watchedDir_.isEmpty();
    }

private:
    using Values = QList<FooterValue>;

    void loadConfig();
    /// Scan the active file for changes, after a running scan of it.
    void requestScan();
    /// Cancel a running scan and scan the active file afresh.
    void restartScan();
    void startScan();
    void watchActiveFile();
    void unwatch();
    void show( const Values& values );

    QPointer<FooterDisplayWidget> widget_;
    const QString configDir_;
    QString activeFile_;

    std::shared_ptr<const FooterScanner> scanner_;
    int maxLines_ = FooterScanner::kDefaultMaxLines;

    /// How far the active file has been scanned with the current rules.
    FooterScanner::Progress progress_;

    QFileSystemWatcher fileWatcher_;
    /// The active file's directory, watched while the file is missing.
    QString watchedDir_;
    QTimer rescanTimer_;

    /// Counts started scans; a finished scan is shown only if it is the last.
    quint64 generation_ = 0;
    /// A scan of the active file runs, and is not cancelled.
    bool scanning_ = false;
    /// The active file is to be scanned again once the running scan finished.
    bool rescanPending_ = false;
    std::shared_ptr<std::atomic_bool> cancelRunning_;

    /// One worker thread: a new scan waits for the cancelled one to stop.
    QThreadPool pool_;
};

} // namespace custom_footer
