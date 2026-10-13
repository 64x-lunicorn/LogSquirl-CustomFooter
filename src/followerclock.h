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

#include <QElapsedTimer>
#include <QTimer>
#include <QtGlobal>

#include <functional>

namespace custom_footer {

/**
 * The time as a FileFollower sees it: what time it is, and a wake-up at a
 * later time. One wake-up is pending at most.
 *
 * The production adapter is QtFollowerClock; tests use a clock they move
 * themselves (tests/followerfakes.h), so no test waits for real time.
 */
class FollowerClock {
public:
    virtual ~FollowerClock() = default;

    /// Monotonic milliseconds, from any start.
    virtual qint64 now() const = 0;

    /// Call @p fn on the owner's thread once now() reached @p at, instead
    /// of a wake-up asked for earlier. Never calls it from within wakeAt().
    virtual void wakeAt( qint64 at, std::function<void()> fn ) = 0;

    /// Drop the pending wake-up, if any.
    virtual void cancelWake() = 0;
};

/// A FollowerClock of a QElapsedTimer and a single-shot QTimer, which wakes
/// up on the thread that created it.
class QtFollowerClock : public FollowerClock {
public:
    QtFollowerClock();

    qint64 now() const override;
    void wakeAt( qint64 at, std::function<void()> fn ) override;
    void cancelWake() override;

private:
    QElapsedTimer elapsed_;
    QTimer timer_;
    std::function<void()> wake_;
};

} // namespace custom_footer
