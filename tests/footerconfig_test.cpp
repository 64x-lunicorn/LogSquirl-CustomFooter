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

/**
 * @file footerconfig_test.cpp
 * @brief BDD tests for FooterConfig load/save round-trip.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footerentry.h"

#include <QTemporaryDir>

using namespace costume_footer;

SCENARIO( "FooterConfig persists entries to INI file", "[footerconfig]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a list of footer entries" )
    {
        QList<FooterEntry> entries;
        entries.append( { "VIN", "VIN:\\s+(\\S+)", true } );
        entries.append( { "ECU", "ECU=([A-Z0-9]+)", false } );

        WHEN( "saving and loading entries" )
        {
            FooterConfig::saveEntries( tmpDir.path(), entries );
            const auto loaded = FooterConfig::loadEntries( tmpDir.path() );

            THEN( "the loaded entries match the saved ones" )
            {
                REQUIRE( loaded.size() == 2 );

                REQUIRE( loaded[ 0 ].key == "VIN" );
                REQUIRE( loaded[ 0 ].regexPattern == "VIN:\\s+(\\S+)" );
                REQUIRE( loaded[ 0 ].enabled == true );

                REQUIRE( loaded[ 1 ].key == "ECU" );
                REQUIRE( loaded[ 1 ].regexPattern == "ECU=([A-Z0-9]+)" );
                REQUIRE( loaded[ 1 ].enabled == false );
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

SCENARIO( "FooterConfig persists display mode", "[footerconfig]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );

    GIVEN( "a display mode setting" )
    {
        WHEN( "saving and loading DisplayMode::Both" )
        {
            FooterConfig::saveDisplayMode( tmpDir.path(), DisplayMode::Both );
            const auto loaded = FooterConfig::loadDisplayMode( tmpDir.path() );

            THEN( "the loaded mode matches" )
            {
                REQUIRE( loaded == DisplayMode::Both );
            }
        }

        WHEN( "loading from empty config" )
        {
            const auto loaded = FooterConfig::loadDisplayMode( tmpDir.path() );

            THEN( "default is Footer" )
            {
                REQUIRE( loaded == DisplayMode::Footer );
            }
        }
    }
}
