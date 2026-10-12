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

#include "filechangesource.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace custom_footer {

/**
 * Watches the active log file for the footer and says when it should be
 * scanned again: a FileChangeSource with a pause after its changes.
 *
 * Changes are collected for a pause before changed() is emitted, without
 * postponing it for as long as a busy log keeps changing. What counts as a
 * change, and what is watched until the owner reads the file again
 * (watch()), is the FileChangeSource's.
 *
 * The editor's preview follows the file with a FileFollower instead; the
 * footer moves onto it too (#35), and this class goes then.
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
        return source_.file();
    }

    /// Watch the file again, or its directory while it is missing.
    void watch();

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
        return source_.waitingForFile();
    }

Q_SIGNALS:
    /// The file changed, or reappeared, and the pause has passed.
    void changed();

private:
    FileChangeSource source_;
    QTimer timer_;
    int delayMs_;
};

} // namespace custom_footer
