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
 * @file footerconfig_test.cpp
 * @brief BDD tests for FooterConfig load/save round-trip.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"

#include <QFile>
#include <QTemporaryDir>

using namespace custom_footer;

SCENARIO( "FooterConfig persists entries to INI file", "[footerconfig]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a list of footer entries with value pattern and mappings" )
    {
        QList<FooterEntry> entries;

        QList<ValueMapping> mappings;
        mappings.append( ValueMapping{ "true", "Enabled" } );
        mappings.append( ValueMapping{ "false", "Disabled" } );

        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
        entries.append( { "Protection", "isComponentProtection",
                          ":\\s+(\\S+)$", false, mappings } );

        WHEN( "saving and loading entries" )
        {
            FooterConfig::saveEntries( tmpDir.path(), entries );
            const auto loaded = FooterConfig::loadEntries( tmpDir.path() );

            THEN( "the loaded entries match the saved ones" )
            {
                REQUIRE( loaded.size() == 2 );

                REQUIRE( loaded[ 0 ].key == "VIN" );
                REQUIRE( loaded[ 0 ].linePattern == "VIN:\\s+(\\S+)" );
                REQUIRE( loaded[ 0 ].valuePattern.isEmpty() );
                REQUIRE( loaded[ 0 ].enabled == true );
                REQUIRE( loaded[ 0 ].mappings.isEmpty() );

                REQUIRE( loaded[ 1 ].key == "Protection" );
                REQUIRE( loaded[ 1 ].linePattern == "isComponentProtection" );
                REQUIRE( loaded[ 1 ].valuePattern == ":\\s+(\\S+)$" );
                REQUIRE( loaded[ 1 ].enabled == false );
                REQUIRE( loaded[ 1 ].mappings.size() == 2 );
                REQUIRE( loaded[ 1 ].mappings[ 0 ].pattern == "true" );
                REQUIRE( loaded[ 1 ].mappings[ 0 ].displayValue == "Enabled" );
                REQUIRE( loaded[ 1 ].mappings[ 1 ].pattern == "false" );
                REQUIRE( loaded[ 1 ].mappings[ 1 ].displayValue == "Disabled" );
            }
        }
    }

    GIVEN( "an empty config directory" )
    {
        WHEN( "loading entries from scratch" )
        {
            const auto loaded = FooterConfig::loadEntries( tmpDir.path() );

            THEN( "an empty list is returned" )
            {
                REQUIRE( loaded.isEmpty() );
            }
        }
    }
}

SCENARIO( "FooterConfig JSON export and import round-trip", "[footerconfig]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a list of entries with mappings" )
    {
        QList<ValueMapping> mappings;
        mappings.append( ValueMapping{ "0", "Off" } );
        mappings.append( ValueMapping{ "1", "On" } );

        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
        entries.append( { "Feature", "feature_flag",
                          "=\\s*(\\d+)", true, mappings } );

        const QString jsonPath = tmpDir.path() + "/rules.json";

        WHEN( "exporting to JSON and importing back" )
        {
            const bool ok = FooterConfig::exportToJson( jsonPath, entries );
            REQUIRE( ok );

            QString error;
            const auto imported = FooterConfig::importFromJson( jsonPath, &error );

            THEN( "the imported entries match" )
            {
                REQUIRE( error.isEmpty() );
                REQUIRE( imported.size() == 2 );

                REQUIRE( imported[ 0 ].key == "VIN" );
                REQUIRE( imported[ 0 ].linePattern == "VIN:\\s+(\\S+)" );
                REQUIRE( imported[ 0 ].valuePattern.isEmpty() );
                REQUIRE( imported[ 0 ].enabled == true );
                REQUIRE( imported[ 0 ].mappings.isEmpty() );

                REQUIRE( imported[ 1 ].key == "Feature" );
                REQUIRE( imported[ 1 ].linePattern == "feature_flag" );
                REQUIRE( imported[ 1 ].valuePattern == "=\\s*(\\d+)" );
                REQUIRE( imported[ 1 ].enabled == true );
                REQUIRE( imported[ 1 ].mappings.size() == 2 );
                REQUIRE( imported[ 1 ].mappings[ 0 ].pattern == "0" );
                REQUIRE( imported[ 1 ].mappings[ 0 ].displayValue == "Off" );
                REQUIRE( imported[ 1 ].mappings[ 1 ].pattern == "1" );
                REQUIRE( imported[ 1 ].mappings[ 1 ].displayValue == "On" );
            }
        }
    }

    GIVEN( "a non-existent file" )
    {
        WHEN( "importing" )
        {
            QString error;
            const auto imported
                = FooterConfig::importFromJson( "/does/not/exist.json", &error );

            THEN( "an error is reported and result is empty" )
            {
                REQUIRE( imported.isEmpty() );
                REQUIRE_FALSE( error.isEmpty() );
            }
        }
    }

    GIVEN( "an invalid JSON file" )
    {
        const QString badPath = tmpDir.path() + "/bad.json";
        QFile f( badPath );
        REQUIRE( f.open( QIODevice::WriteOnly ) );
        f.write( "{ not valid json" );
        f.close();

        WHEN( "importing" )
        {
            QString error;
            const auto imported = FooterConfig::importFromJson( badPath, &error );

            THEN( "a parse error is reported" )
            {
                REQUIRE( imported.isEmpty() );
                REQUIRE( error.contains( "parse" ) );
            }
        }
    }
}


