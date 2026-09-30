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

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace custom_footer {

/**
 * Watches the active log file for its owner, the footer or the editor's
 * preview, and says when it should be read again.
 *
 * Changes are collected for a pause before changed() is emitted, without
 * postponing it for as long as a busy log keeps changing. Once a change was
 * seen, the file is no longer watched until the owner calls watch() again
 * when it reads the file: a file that was renamed away and replaced is then
 * watched at its path. While the file is missing, e.g. after it was rotated
 * away, its directory is watched instead, and changed() is emitted once
 * the file is there again.
 */
class ActiveFileWatcher : public QObject {
    Q_OBJECT

public:
    explicit ActiveFileWatcher( int delayMs, QObject* parent = nullptr );

    /// Watch another file, or none; nothing of the previous one is kept, and
    /// nothing is watched until watch().
    void setFile( const QString& filePath );
    const QString& file() const
    {
        return file_;
    }

    /// Watch the file again, or its directory while it is missing.
    void watch();

    /// Emit changed() after the pause, as for a change of the file.
    void schedule();

    /// Emit changed() after @p delayMs instead, replacing a pending change.
    void scheduleIn( int delayMs );

    /// Drop a change not yet reported.
    void cancelPending();

    /// Whether a change is waiting for the pause to end.
    bool isPending() const
    {
        return timer_.isActive();
    }

    /// Whether the file is missing and its directory is watched for it.
    bool isWaitingForFile() const
    {
        return !watchedDir_.isEmpty();
    }

    /// What is watched, for tests.
    QStringList watchedPaths() const
    {
        return watcher_.files() + watcher_.directories();
    }

    void setDelay( int delayMs );

Q_SIGNALS:
    /// The file changed, or reappeared, and the pause has passed.
    void changed();

private Q_SLOTS:
    void fileChanged( const QString& path );
    void directoryChanged( const QString& path );

private:
    void unwatch();

    QString file_;
    /// The file's directory, watched while the file is missing.
    QString watchedDir_;
    QFileSystemWatcher watcher_;
    QTimer timer_;
    int delayMs_;
};

} // namespace custom_footer
