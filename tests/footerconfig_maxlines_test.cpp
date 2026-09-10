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
 * @file footerconfig_maxlines_test.cpp
 * @brief BDD tests for FooterConfig maxLines persistence.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"

#include <QTemporaryDir>

using namespace custom_footer;

SCENARIO( "FooterConfig persists maxLines setting", "[footerconfig][maxlines]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "no config exists yet" )
    {
        WHEN( "loading maxLines" )
        {
            const int maxLines = FooterConfig::loadMaxLines( tmpDir.path() );

            THEN( "the default value 100000 is returned" )
            {
                REQUIRE( maxLines == 100000 );
            }
        }
    }

    GIVEN( "a custom maxLines value is saved" )
    {
        FooterConfig::saveMaxLines( tmpDir.path(), 5000 );

        WHEN( "loading it back" )
        {
            const int loaded = FooterConfig::loadMaxLines( tmpDir.path() );

            THEN( "the saved value is returned" )
            {
                REQUIRE( loaded == 5000 );
            }
        }
    }

    GIVEN( "maxLines is saved as zero" )
    {
        FooterConfig::saveMaxLines( tmpDir.path(), 0 );

        WHEN( "loading it back" )
        {
            const int loaded = FooterConfig::loadMaxLines( tmpDir.path() );

            THEN( "zero is returned (unlimited scanning)" )
            {
                REQUIRE( loaded == 0 );
            }
        }
    }
}

SCENARIO( "FooterConfig handles overwriting entries", "[footerconfig]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "an initial set of entries is saved" )
    {
        QList<FooterEntry> initial;
        initial.append( { "A", "patternA", "", true, {} } );
        initial.append( { "B", "patternB", "", true, {} } );
        FooterConfig::saveEntries( tmpDir.path(), initial );

        WHEN( "a different set of entries overwrites" )
        {
            QList<FooterEntry> replacement;
            replacement.append( { "C", "patternC", "", false, {} } );
            FooterConfig::saveEntries( tmpDir.path(), replacement );
            const auto loaded = FooterConfig::loadEntries( tmpDir.path() );

            THEN( "only the new entries exist" )
            {
                REQUIRE( loaded.size() == 1 );
                REQUIRE( loaded[ 0 ].key == "C" );
                REQUIRE( loaded[ 0 ].linePattern == "patternC" );
                REQUIRE( loaded[ 0 ].enabled == false );
            }
        }
    }
}

SCENARIO( "FooterConfig JSON import with missing version field", "[footerconfig][json]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a JSON file without a version field" )
    {
        const QString jsonPath = tmpDir.path() + "/noversion.json";
        QFile f( jsonPath );
        REQUIRE( f.open( QIODevice::WriteOnly | QIODevice::Text ) );
        f.write( R"({"entries": [{"key": "X", "linePattern": "x"}]})" );
        f.close();

        WHEN( "importing" )
        {
            QString error;
            const auto imported = FooterConfig::importFromJson( jsonPath, &error );

            THEN( "an error about missing version is reported" )
            {
                REQUIRE( imported.isEmpty() );
                REQUIRE( error.contains( "version" ) );
            }
        }
    }
}

SCENARIO( "FooterConfig export to unwritable path fails", "[footerconfig][json]" )
{
    GIVEN( "an invalid path" )
    {
        QList<FooterEntry> entries;
        entries.append( { "X", "pattern", "", true, {} } );

        WHEN( "exporting to an impossible path" )
        {
            const bool ok = FooterConfig::exportToJson(
                "/nonexistent_dir_12345/rules.json", entries );

            THEN( "it returns false" )
            {
                REQUIRE_FALSE( ok );
            }
        }
    }
}
