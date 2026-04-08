/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "footerentry.h"

#include <QList>
#include <QString>

namespace costume_footer {

/// Load and save footer entries and display mode to an INI config file.
class FooterConfig {
  public:
    /// Load all entries and display mode from the config directory.
    static QList<FooterEntry> loadEntries( const QString& configDir );

    /// Save all entries and display mode to the config directory.
    static void saveEntries( const QString& configDir,
                             const QList<FooterEntry>& entries );

    /// Load the display mode setting.
    static DisplayMode loadDisplayMode( const QString& configDir );

    /// Save the display mode setting.
    static void saveDisplayMode( const QString& configDir, DisplayMode mode );

    /// Load the max-lines limit for scanning.
    static int loadMaxLines( const QString& configDir );

    /// Save the max-lines limit for scanning.
    static void saveMaxLines( const QString& configDir, int maxLines );

  private:
    static QString configPath( const QString& configDir );
};

} // namespace costume_footer
