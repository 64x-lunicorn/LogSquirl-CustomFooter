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

#include "footerentry.h"

#include <QList>
#include <QString>

namespace custom_footer {

/// Load and save footer entries to an INI config file.
/// Provides JSON import/export for sharing rule sets.
class FooterConfig {
  public:
    /// Load all entries from the config directory.
    static QList<FooterEntry> loadEntries( const QString& configDir );

    /// Save all entries to the config directory.
    static void saveEntries( const QString& configDir,
                             const QList<FooterEntry>& entries );

    /// Load the max-lines limit for scanning.
    static int loadMaxLines( const QString& configDir );

    /// Save the max-lines limit for scanning.
    static void saveMaxLines( const QString& configDir, int maxLines );

    /// Export all entries to a JSON file.  Returns true on success.
    static bool exportToJson( const QString& filePath,
                              const QList<FooterEntry>& entries );

    /// Import entries from a JSON file.  Returns empty list on error.
    static QList<FooterEntry> importFromJson( const QString& filePath,
                                              QString* errorOut = nullptr );

  private:
    static QString configPath( const QString& configDir );
};

} // namespace custom_footer
