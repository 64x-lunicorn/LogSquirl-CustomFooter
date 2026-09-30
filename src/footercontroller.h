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
 * run on a worker thread; a scan's values are shown only if no newer scan was
 * requested meanwhile. The active file is watched, so the values follow a
 * growing log.
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

    /// Show the values of this file; an empty path clears the footer.
    void setActiveFile( const QString& filePath );

    /// Load the rules again, e.g. after they were saved, and rescan.
    void reloadConfig();

private:
    using Values = QList<QPair<QString, QString>>;

    void loadConfig();
    void rescan();
    void watchActiveFile();
    void show( const Values& values );

    QPointer<FooterDisplayWidget> widget_;
    const QString configDir_;
    QString activeFile_;

    std::shared_ptr<const FooterScanner> scanner_;
    int maxLines_ = FooterScanner::kDefaultMaxLines;

    QFileSystemWatcher fileWatcher_;
    QTimer rescanTimer_;

    /// Counts scan requests; a finished scan is shown only if it is the last.
    quint64 generation_ = 0;
    std::shared_ptr<std::atomic_bool> cancelRunning_;

    /// One worker thread: a new scan waits for the cancelled one to stop.
    QThreadPool pool_;
};

} // namespace custom_footer
