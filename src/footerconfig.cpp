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

#include "footerconfig.h"

#include <QSettings>

namespace costume_footer {

QString FooterConfig::configPath( const QString& configDir )
{
    return configDir + "/costume_footer.ini";
}

QList<FooterEntry> FooterConfig::loadEntries( const QString& configDir )
{
    QSettings settings( configPath( configDir ), QSettings::IniFormat );
    QList<FooterEntry> entries;

    const int count = settings.beginReadArray( "entries" );
    for ( int i = 0; i < count; ++i ) {
        settings.setArrayIndex( i );
        FooterEntry entry;
        entry.key = settings.value( "key" ).toString();
        entry.regexPattern = settings.value( "regex" ).toString();
        entry.enabled = settings.value( "enabled", true ).toBool();
        entries.append( entry );
    }
    settings.endArray();

    return entries;
}

void FooterConfig::saveEntries( const QString& configDir,
                                const QList<FooterEntry>& entries )
{
    QSettings settings( configPath( configDir ), QSettings::IniFormat );

    settings.beginWriteArray( "entries", entries.size() );
    for ( int i = 0; i < entries.size(); ++i ) {
        settings.setArrayIndex( i );
        settings.setValue( "key", entries[ i ].key );
        settings.setValue( "regex", entries[ i ].regexPattern );
        settings.setValue( "enabled", entries[ i ].enabled );
    }
    settings.endArray();
}

DisplayMode FooterConfig::loadDisplayMode( const QString& configDir )
{
    QSettings settings( configPath( configDir ), QSettings::IniFormat );
    return static_cast<DisplayMode>(
        settings.value( "display/mode", static_cast<int>( DisplayMode::Footer ) )
            .toInt() );
}

void FooterConfig::saveDisplayMode( const QString& configDir, DisplayMode mode )
{
    QSettings settings( configPath( configDir ), QSettings::IniFormat );
    settings.setValue( "display/mode", static_cast<int>( mode ) );
}

int FooterConfig::loadMaxLines( const QString& configDir )
{
    QSettings settings( configPath( configDir ), QSettings::IniFormat );
    return settings.value( "scan/maxLines", 100000 ).toInt();
}

void FooterConfig::saveMaxLines( const QString& configDir, int maxLines )
{
    QSettings settings( configPath( configDir ), QSettings::IniFormat );
    settings.setValue( "scan/maxLines", maxLines );
}

} // namespace costume_footer
