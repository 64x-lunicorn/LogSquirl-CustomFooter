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

#include "footerconfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace custom_footer {

QString FooterConfig::configPath( const QString& configDir )
{
    return configDir + "/custom_footer.ini";
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
        // Read linePattern; fall back to "regex" for backward compatibility
        entry.linePattern = settings.value( "linePattern",
                                            settings.value( "regex" ) ).toString();
        entry.valuePattern = settings.value( "valuePattern" ).toString();
        entry.enabled = settings.value( "enabled", true ).toBool();

        // Read mappings sub-array
        const int mapCount = settings.beginReadArray( "mappings" );
        for ( int m = 0; m < mapCount; ++m ) {
            settings.setArrayIndex( m );
            ValueMapping mapping;
            mapping.pattern = settings.value( "pattern" ).toString();
            mapping.displayValue = settings.value( "display" ).toString();
            entry.mappings.append( mapping );
        }
        settings.endArray();

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
        settings.setValue( "linePattern", entries[ i ].linePattern );
        settings.setValue( "valuePattern", entries[ i ].valuePattern );
        settings.setValue( "enabled", entries[ i ].enabled );

        // Write mappings sub-array
        settings.beginWriteArray( "mappings", entries[ i ].mappings.size() );
        for ( int m = 0; m < entries[ i ].mappings.size(); ++m ) {
            settings.setArrayIndex( m );
            settings.setValue( "pattern", entries[ i ].mappings[ m ].pattern );
            settings.setValue( "display", entries[ i ].mappings[ m ].displayValue );
        }
        settings.endArray();
    }
    settings.endArray();
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

bool FooterConfig::exportToJson( const QString& filePath,
                                 const QList<FooterEntry>& entries )
{
    QJsonArray jsonEntries;
    for ( const auto& entry : entries ) {
        QJsonObject obj;
        obj[ "key" ] = entry.key;
        obj[ "linePattern" ] = entry.linePattern;
        obj[ "valuePattern" ] = entry.valuePattern;
        obj[ "enabled" ] = entry.enabled;

        QJsonArray jsonMappings;
        for ( const auto& mapping : entry.mappings ) {
            QJsonObject mapObj;
            mapObj[ "pattern" ] = mapping.pattern;
            mapObj[ "display" ] = mapping.displayValue;
            jsonMappings.append( mapObj );
        }
        obj[ "mappings" ] = jsonMappings;

        jsonEntries.append( obj );
    }

    QJsonObject root;
    root[ "version" ] = 1;
    root[ "entries" ] = jsonEntries;

    QFile file( filePath );
    if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
        return false;
    }
    file.write( QJsonDocument( root ).toJson( QJsonDocument::Indented ) );
    return true;
}

QList<FooterEntry> FooterConfig::importFromJson( const QString& filePath,
                                                 QString* errorOut )
{
    QList<FooterEntry> entries;

    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        if ( errorOut ) {
            *errorOut = QStringLiteral( "Cannot open file: %1" ).arg( filePath );
        }
        return entries;
    }

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson( file.readAll(), &parseError );
    if ( doc.isNull() ) {
        if ( errorOut ) {
            *errorOut = QStringLiteral( "JSON parse error: %1" )
                            .arg( parseError.errorString() );
        }
        return entries;
    }

    const auto root = doc.object();
    const int version = root[ "version" ].toInt( 0 );
    if ( version < 1 ) {
        if ( errorOut ) {
            *errorOut = QStringLiteral( "Unknown or missing version field" );
        }
        return entries;
    }

    const auto jsonEntries = root[ "entries" ].toArray();
    for ( const auto& val : jsonEntries ) {
        const auto obj = val.toObject();
        FooterEntry entry;
        entry.key = obj[ "key" ].toString();
        entry.linePattern = obj[ "linePattern" ].toString();
        entry.valuePattern = obj[ "valuePattern" ].toString();
        entry.enabled = obj[ "enabled" ].toBool( true );

        const auto jsonMappings = obj[ "mappings" ].toArray();
        for ( const auto& mapVal : jsonMappings ) {
            const auto mapObj = mapVal.toObject();
            ValueMapping mapping;
            mapping.pattern = mapObj[ "pattern" ].toString();
            mapping.displayValue = mapObj[ "display" ].toString();
            entry.mappings.append( mapping );
        }

        entries.append( entry );
    }

    return entries;
}

} // namespace custom_footer
