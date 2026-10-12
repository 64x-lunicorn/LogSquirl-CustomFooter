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

/**
 * @file followerfakes.h
 * @brief The test adapters of the file follower's seams: a clock the test
 *        moves, and a change source the test fires.
 */

#pragma once

#include "changesource.h"
#include "followerclock.h"

#include <QString>

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>

namespace custom_footer {

/**
 * A FollowerClock whose time only the test moves: setNow() and advance()
 * fire a wake-up that is due, at its own time, and then any wake-up it
 * asked for that is due as well, all synchronously and in order.
 */
class ManualClock : public FollowerClock {
public:
    qint64 now() const override
    {
        return now_;
    }

    void wakeAt( qint64 at, std::function<void()> fn ) override
    {
        wake_ = Wake{ at, std::move( fn ) };
    }

    void cancelWake() override
    {
        wake_.reset();
    }

    void setNow( qint64 now )
    {
        while ( wake_ && wake_->at <= now ) {
            const auto wake = *std::exchange( wake_, std::nullopt );
            now_ = std::max( now_, wake.at );
            wake.fn();
        }
        now_ = std::max( now_, now );
    }

    void advance( qint64 ms )
    {
        setNow( now_ + ms );
    }

    /// When the pending wake-up is due, if one is pending.
    std::optional<qint64> wakeTime() const
    {
        return wake_ ? std::optional<qint64>( wake_->at ) : std::nullopt;
    }

private:
    struct Wake {
        qint64 at = 0;
        std::function<void()> fn;
    };
    qint64 now_ = 0;
    std::optional<Wake> wake_;
};

/// A ChangeSource whose changes the test reports, recording what it is told.
class FakeChangeSource : public ChangeSource {
public:
    void setFile( const QString& path ) override
    {
        file = path;
    }

    void rearm() override
    {
        ++rearms;
    }

    bool waitingForFile() const override
    {
        return waiting;
    }

    /// Report a change of the file, as the system would.
    void fire()
    {
        if ( changed ) {
            changed();
        }
    }

    QString file;
    int rearms = 0;
    bool waiting = false;
};

} // namespace custom_footer
