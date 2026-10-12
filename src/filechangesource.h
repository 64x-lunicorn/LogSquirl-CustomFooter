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

#include "changesource.h"

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

namespace custom_footer {

/**
 * The file system's notifications about the active log file, as a
 * ChangeSource.
 *
 * Once a change was reported, the file is no longer watched until rearm(),
 * which the owner calls when it reads the file: a file that was renamed
 * away and replaced is then watched at its path. While the file is missing,
 * e.g. after it was rotated away, its directory is watched instead, and a
 * change is reported once the file is there again. Notifications queued for
 * a file followed before are ignored.
 *
 * Some systems report reading the file as a change of it, e.g. of its
 * access time, so an owner reacting to every notification would read the
 * file again and again. A notification is therefore reported only if the
 * file's size, modification or birth time, or its existence differ from
 * when the owner last read it (rearm()) or the last change was reported.
 */
class FileChangeSource : public QObject, public ChangeSource {
    Q_OBJECT

public:
    explicit FileChangeSource( QObject* parent = nullptr );

    void setFile( const QString& path ) override;
    void rearm() override;
    bool waitingForFile() const override
    {
        return !watchedDir_.isEmpty();
    }

    const QString& file() const
    {
        return file_;
    }

    /// What is watched, for tests.
    QStringList watchedPaths() const
    {
        return watcher_.files() + watcher_.directories();
    }

    /// Whether the file system is asked for notifications at all. Tests of
    /// which notifications are reported turn it off and deliver them
    /// themselves: some systems report reads of the file too, e.g. its
    /// access time, which would report changes behind the test's back.
    void setSystemNotifications( bool enabled );

private Q_SLOTS:
    void fileChanged( const QString& path );
    void directoryChanged( const QString& path );

private:
    /// What tells a change of the file's content or identity.
    struct Stamp {
        bool exists = false;
        qint64 size = -1;
        QDateTime modified;
        QDateTime birth;

        bool operator==( const Stamp& other ) const
        {
            return exists == other.exists && size == other.size && modified == other.modified
                   && birth == other.birth;
        }
    };
    static Stamp stampOf( const QString& filePath );

    void report();
    void unwatch();

    QString file_;
    /// The file's directory, watched while the file is missing.
    QString watchedDir_;
    QFileSystemWatcher watcher_;
    bool systemNotifications_ = true;
    /// The file as last read by the owner, or as last reported changed.
    Stamp known_;
};

} // namespace custom_footer
