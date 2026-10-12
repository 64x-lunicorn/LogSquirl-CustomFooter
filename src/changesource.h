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

#include <QString>

#include <functional>

namespace custom_footer {

/**
 * Says when the followed file changed, for a FileFollower.
 *
 * A change is reported once per read: after reporting one, a source need
 * not report more until rearm(), which its owner calls whenever it reads
 * the file.
 *
 * The production adapter is FileChangeSource, on the file system; tests
 * report changes themselves (tests/followerfakes.h).
 */
class ChangeSource {
public:
    virtual ~ChangeSource() = default;

    /// Called on the owner's thread when the file changed; set by the owner.
    std::function<void()> changed;

    /// Follow another file, or none for an empty path. Nothing is watched
    /// until rearm().
    virtual void setFile( const QString& path ) = 0;

    /// The file is read now: report changes made from now on.
    virtual void rearm() = 0;

    /// Whether the file is missing, and its appearance is waited for.
    virtual bool waitingForFile() const = 0;
};

} // namespace custom_footer
