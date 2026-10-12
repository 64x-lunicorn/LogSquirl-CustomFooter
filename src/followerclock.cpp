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

#include "followerclock.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace custom_footer {

QtFollowerClock::QtFollowerClock()
{
    elapsed_.start();
    timer_.setSingleShot( true );
    QObject::connect( &timer_, &QTimer::timeout, [ this ] {
        // The wake-up may ask for the next one.
        const auto wake = std::exchange( wake_, nullptr );
        if ( wake ) {
            wake();
        }
    } );
}

qint64 QtFollowerClock::now() const
{
    return elapsed_.elapsed();
}

void QtFollowerClock::wakeAt( qint64 at, std::function<void()> fn )
{
    wake_ = std::move( fn );
    const auto in = std::clamp<qint64>( at - now(), 0, std::numeric_limits<int>::max() );
    timer_.start( static_cast<int>( in ) );
}

void QtFollowerClock::cancelWake()
{
    timer_.stop();
    wake_ = nullptr;
}

} // namespace custom_footer
