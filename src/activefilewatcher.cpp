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

#include "activefilewatcher.h"

namespace custom_footer {

ActiveFileWatcher::ActiveFileWatcher( int delayMs, QObject* parent )
    : QObject( parent )
    , delayMs_( delayMs )
{
    timer_.setSingleShot( true );
    connect( &timer_, &QTimer::timeout, this, &ActiveFileWatcher::changed );
    source_.changed = [ this ] {
        if ( !timer_.isActive() ) {
            timer_.start( delayMs_ );
        }
    };
}

void ActiveFileWatcher::setFile( const QString& filePath )
{
    timer_.stop();
    source_.setFile( filePath );
}

void ActiveFileWatcher::watch()
{
    source_.rearm();
}

void ActiveFileWatcher::cancelPending()
{
    timer_.stop();
}

} // namespace custom_footer
