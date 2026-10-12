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
 * @file footersession_test.cpp
 * @brief BDD tests for the footer session: one loaded plugin's rules, active
 *        file, footer and rule editor, driven directly with a temp config dir.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footerdisplaywidget.h"
#include "footersession.h"
#include "hostfunctions.h"
#include "logsquirl_plugin_api.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>

#include <memory>

using namespace custom_footer;

namespace {

/// Process events until @p done, for at most a generous bound: a hung
/// worker fails the test instead of hanging it. Nothing depends on timing.
template <typename Predicate>
bool processUntil( Predicate done, int boundMs = 10000 )
{
    QElapsedTimer timer;
    timer.start();
    while ( !done() ) {
        if ( timer.elapsed() > boundMs ) {
            return false;
        }
        QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
        QThread::msleep( 1 );
    }
    return true;
}

void writeFile( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

FooterEntry rule( const QString& key )
{
    return { key, key + ":\\s+(\\S+)", "", true, {} };
}

/// The footer's values as "key=value", in the order shown.
QStringList shown( const FooterDisplayWidget& footer )
{
    QStringList values;
    for ( const auto& value : footer.values() ) {
        values.append( value.key + "=" + value.value );
    }
    return values;
}

/// What a session logged through its host functions.
struct Log {
    QStringList warnings;
    QStringList all;

    HostFunctions host()
    {
        HostFunctions functions;
        functions.log = [ this ]( int level, const QString& message ) {
            all.append( message );
            if ( level == LOGSQUIRL_LOG_WARNING ) {
                warnings.append( message );
            }
        };
        return functions;
    }
};

} // namespace

SCENARIO( "A footer session shows the values of the active file", "[footersession]" )
{
    QTemporaryDir configDir;
    QTemporaryDir logDir;
    REQUIRE( configDir.isValid() );
    REQUIRE( logDir.isValid() );
    REQUIRE( FooterConfig::saveEntries( configDir.path(), { rule( "VIN" ) } ) );
    const auto first = logDir.path() + "/first.log";
    const auto second = logDir.path() + "/second.log";
    writeFile( first, "VIN: FIRST\n" );
    writeFile( second, "VIN: SECOND\n" );

    FooterDisplayWidget footer;
    Log log;

    GIVEN( "a session on the config dir and the footer" )
    {
        FooterSession session( configDir.path(), &footer, log.host() );

        THEN( "nothing is shown without an active file, and nothing is wrong" )
        {
            REQUIRE( footer.values().isEmpty() );
            REQUIRE( log.warnings.isEmpty() );
        }

        WHEN( "it is given the active file" )
        {
            session.setActiveFile( first );

            THEN( "the file's values are shown, with the rules from the config dir" )
            {
                REQUIRE( processUntil(
                    [ & ] { return shown( footer ) == QStringList{ "VIN=FIRST" }; } ) );
            }

            AND_WHEN( "it is given another file" )
            {
                REQUIRE( processUntil( [ & ] { return !footer.values().isEmpty(); } ) );
                session.setActiveFile( second );

                THEN( "that file's values are shown" )
                {
                    REQUIRE( processUntil(
                        [ & ] { return shown( footer ) == QStringList{ "VIN=SECOND" }; } ) );
                }
            }

            AND_WHEN( "it is told there is no active file" )
            {
                REQUIRE( processUntil( [ & ] { return !footer.values().isEmpty(); } ) );
                session.setActiveFile( {} );

                THEN( "the footer is cleared" )
                {
                    REQUIRE( footer.values().isEmpty() );
                }
            }
        }
    }

    GIVEN( "a rule with an invalid pattern in the config dir" )
    {
        REQUIRE( FooterConfig::saveEntries( configDir.path(),
                                            { { "Broken", "broken=(", "", true, {} } } ) );

        WHEN( "a session is made" )
        {
            FooterSession session( configDir.path(), &footer, log.host() );

            THEN( "the problem is logged as a warning through the host functions" )
            {
                REQUIRE( log.warnings.size() == 1 );
                REQUIRE( log.warnings.at( 0 ).startsWith( "Custom Footer: " ) );
                REQUIRE( log.warnings.at( 0 ).contains( "Broken" ) );
            }
        }

        WHEN( "a session is made without a log function, as for a host that logs nothing" )
        {
            FooterSession session( configDir.path(), &footer );

            THEN( "it still runs" )
            {
                session.setActiveFile( first );
                REQUIRE( footer.values().isEmpty() );
            }
        }
    }

    GIVEN( "no config dir" )
    {
        WHEN( "a session is made" )
        {
            FooterSession session( {}, &footer, log.host() );

            THEN( "it warns once that rules are neither loaded nor saved" )
            {
                REQUIRE( log.warnings
                         == QStringList{ "Custom Footer: the host gave no config directory; rules "
                                         "are neither loaded nor saved" } );
            }
        }
    }
}
