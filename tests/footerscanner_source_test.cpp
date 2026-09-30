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
 * @file footerscanner_source_test.cpp
 * @brief BDD tests for where a shown value came from: raw value and rule.
 */

#include <catch2/catch.hpp>

#include "footerentry.h"
#include "footerscanner.h"
#include "footervalue.h"

#include <QFile>
#include <QTemporaryDir>

using namespace custom_footer;

namespace {

void writeBytes( const QString& path, const QByteArray& content, bool append = false )
{
    QFile file( path );
    REQUIRE( file.open( append ? QIODevice::Append : QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

} // namespace

SCENARIO( "FooterScanner tells where each shown value came from", "[footerscanner][source]" )
{
    QTemporaryDir tmpDir;
    REQUIRE( tmpDir.isValid() );
    const auto path = tmpDir.path() + "/source.log";

    QList<FooterEntry> entries;
    entries.append( { "Disabled", "VIN:\\s+(\\S+)", "", false, {} } );
    entries.append( { "Mode", "mode=(\\S+)", "", true, { { "0x04", "Production" } } } );
    entries.append( { "VIN", "VIN:\\s+(\\S+)", "", true, {} } );
    entries.append( { "Mode", "legacy-mode (\\S+)", "", true, { { "4", "Production" } } } );
    const FooterScanner scanner( entries );

    GIVEN( "a file with a mapped and an unmapped value" )
    {
        writeBytes( path, "VIN: ABC123\nmode=0x04\n" );

        WHEN( "it is scanned" )
        {
            const auto values = scanner.footerValues( scanner.scanFrom( path, {} ).values );

            THEN( "each value carries its key, shown value, raw value and rule, in rule order" )
            {
                REQUIRE( values.size() == 2 );
                REQUIRE( values[ 0 ] == FooterValue{ "Mode", "Production", "0x04", 1 } );
                REQUIRE( values[ 0 ].isMapped() );
                REQUIRE( values[ 1 ] == FooterValue{ "VIN", "ABC123", "ABC123", 2 } );
                REQUIRE_FALSE( values[ 1 ].isMapped() );
            }
        }
    }

    GIVEN( "a file where a later alternative of a key supplies the value" )
    {
        writeBytes( path, "legacy-mode 4\nmode=0x04\n" );

        WHEN( "it is scanned" )
        {
            const auto values = scanner.footerValues( scanner.scanFrom( path, {} ).values );

            THEN( "the value names the alternative that supplied it" )
            {
                REQUIRE( values.size() == 1 );
                REQUIRE( values[ 0 ] == FooterValue{ "Mode", "Production", "4", 3 } );
            }
        }
    }

    GIVEN( "a scanned file that grows" )
    {
        writeBytes( path, "mode=0x04\n" );
        const auto first = scanner.scanFrom( path, {} );
        writeBytes( path, "VIN: LATER\n", true );

        WHEN( "the scan is continued" )
        {
            const auto next = scanner.scanFrom( path, first.progress );
            REQUIRE( next.resumed );
            const auto values = scanner.footerValues( next.values );

            THEN( "values from before and after keep their sources" )
            {
                REQUIRE( values.size() == 2 );
                REQUIRE( values[ 0 ] == FooterValue{ "Mode", "Production", "0x04", 1 } );
                REQUIRE( values[ 1 ] == FooterValue{ "VIN", "LATER", "LATER", 2 } );
            }
        }
    }

    GIVEN( "a file whose last line has no line break yet" )
    {
        writeBytes( path, "VIN: DONE\nmode=0x04" );

        WHEN( "it is scanned" )
        {
            const auto values = scanner.footerValues( scanner.scanFrom( path, {} ).values );

            THEN( "the value of that line has its source too" )
            {
                REQUIRE( values.size() == 2 );
                REQUIRE( values[ 0 ] == FooterValue{ "Mode", "Production", "0x04", 1 } );
                REQUIRE( values[ 1 ] == FooterValue{ "VIN", "DONE", "DONE", 2 } );
            }
        }
    }
}
