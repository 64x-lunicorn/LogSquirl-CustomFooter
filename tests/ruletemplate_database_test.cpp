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
 * @file ruletemplate_database_test.cpp
 * @brief The Database rule templates: the server version each one finds in
 * the log of PostgreSQL, MySQL (and MariaDB) and Redis.
 */

#include <catch2/catch.hpp>

#include "footerscanner.h"
#include "ruletemplate.h"
#include "simplerule.h"

#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <optional>

using namespace custom_footer;

namespace {

const RuleTemplate& templateNamed( const QString& name )
{
    const auto& templates = ruleTemplates();
    const auto it = std::find_if( templates.begin(), templates.end(),
                                  [ & ]( const RuleTemplate& t ) { return t.name == name; } );
    REQUIRE( it != templates.end() );
    return *it;
}

/// The value the template named @p templateName extracts from @p line
/// through the scanner, as the footer would show it; nothing if it does
/// not match.
std::optional<QString> extract( const QString& templateName, const QString& line )
{
    const auto entry = templateNamed( templateName ).entry();
    REQUIRE( FooterScanner::patternError( entry.linePattern ).isEmpty() );
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto path = dir.path() + "/sample.log";
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    file.write( "first line\n" );
    file.write( line.toUtf8() );
    file.write( "\nlast line\n" );
    file.close();

    const auto values = FooterScanner::scan( path, { entry } );
    if ( !values.contains( entry.key ) ) {
        return std::nullopt;
    }
    return values.value( entry.key );
}

/// Lines a template finds, with the value it takes from each.
struct Sample {
    const char* line;
    const char* value;
};

void requireFinds( const QString& templateName, std::initializer_list<Sample> samples )
{
    for ( const auto& sample : samples ) {
        INFO( sample.line );
        REQUIRE( extract( templateName, sample.line ) == QString( sample.value ) );
    }
}

void requireFindsNothing( const QString& templateName, std::initializer_list<const char*> lines )
{
    for ( const auto* line : lines ) {
        INFO( line );
        REQUIRE( extract( templateName, line ) == std::nullopt );
    }
}

/// The names of the templates of @p group, in list order.
QStringList namesIn( const QString& group )
{
    QStringList names;
    for ( const auto& t : ruleTemplates() ) {
        if ( t.group == group ) {
            names.append( t.name );
        }
    }
    return names;
}

} // namespace

SCENARIO( "The Database group holds its templates", "[ruletemplate]" )
{
    THEN( "PostgreSQL, MySQL and Redis version, in order" )
    {
        REQUIRE( namesIn( "Database" )
                 == QStringList{ "PostgreSQL version", "MySQL version", "Redis version" } );
    }

    THEN( "each is keyed by its name" )
    {
        for ( const auto& name : namesIn( "Database" ) ) {
            INFO( name.toStdString() );
            REQUIRE( templateNamed( name ).key == name );
        }
    }

    THEN( "all are simple rules: the version follows a fixed text" )
    {
        REQUIRE( templateNamed( "PostgreSQL version" ).simple
                 == SimpleRule{ "starting PostgreSQL ", ValueEnd::Whitespace, {} } );
        REQUIRE( templateNamed( "MySQL version" ).simple
                 == SimpleRule{ "Version: '", ValueEnd::Character, "'" } );
        REQUIRE( templateNamed( "Redis version" ).simple
                 == SimpleRule{ "Redis version=", ValueEnd::Character, "," } );
        for ( const auto& name : namesIn( "Database" ) ) {
            INFO( name.toStdString() );
            const auto& t = templateNamed( name );
            REQUIRE( t.isSimple() );
            REQUIRE( simpleRuleOf( t.entry() ) == t.simple );
        }
    }
}

SCENARIO( "The PostgreSQL version template finds the version in the start line", "[ruletemplate]" )
{
    THEN( "the word after starting PostgreSQL, also of a distribution build or a beta" )
    {
        requireFinds(
            "PostgreSQL version",
            { // Debian's init script, postgresql-common 238, restarting: the
              // cluster's major version, still the server's.
              { "Restarting PostgreSQL 13 database server: main.", "13" },
              // Docker image postgres:16.1 (Debian bookworm).
              { "2024-01-15 10:30:00.123 UTC [1] LOG:  starting PostgreSQL 16.1 (Debian "
                "16.1-1.pgdg120+1) on x86_64-pc-linux-gnu, compiled by gcc (Debian 12.2.0-14) "
                "12.2.0, 64-bit",
                "16.1" },
              // Docker image postgres:15.5-alpine.
              { "2023-11-10 08:00:00.000 UTC [1] LOG:  starting PostgreSQL 15.5 on "
                "x86_64-pc-linux-musl, compiled by gcc (Alpine 12.2.1_git20220924-r10) 12.2.1 "
                "20220924, 64-bit",
                "15.5" },
              // PostgreSQL 12.2 from Ubuntu 20.04, log_line_prefix '%m [%p] '.
              { "2020-05-01 12:00:00.000 UTC [12345] LOG:  starting PostgreSQL 12.2 (Ubuntu "
                "12.2-4) on x86_64-pc-linux-gnu, compiled by gcc (Ubuntu 9.3.0-8ubuntu1) 9.3.0, "
                "64-bit",
                "12.2" },
              // PostgreSQL 17beta1 built from source on macOS 14.
              { "2024-05-23 09:00:00.000 CEST [4242] LOG:  starting PostgreSQL 17beta1 on "
                "aarch64-apple-darwin23.4.0, compiled by Apple clang version 15.0.0 "
                "(clang-1500.3.9.4), 64-bit",
                "17beta1" } } );
    }

    THEN( "of several starts in the file, such as the Docker image's init server, the first" )
    {
        // Docker image postgres:16.1 on its first start: a temporary server
        // for the init scripts, then the real one.
        requireFinds( "PostgreSQL version",
                      { { "2024-01-15 10:29:58.000 UTC [48] LOG:  starting PostgreSQL 16.1 "
                          "(Debian 16.1-1.pgdg120+1) on x86_64-pc-linux-gnu, compiled by gcc "
                          "(Debian 12.2.0-14) 12.2.0, 64-bit\n"
                          "PostgreSQL init process complete; ready for start up.\n"
                          "2024-01-15 10:30:00.123 UTC [1] LOG:  starting PostgreSQL 16.2 "
                          "(Debian 16.2-1.pgdg120+2) on x86_64-pc-linux-gnu, compiled by gcc "
                          "(Debian 12.2.0-14) 12.2.0, 64-bit",
                          "16.1" } } );
    }

    THEN( "not other start lines, a version() result, another spelling, or no value" )
    {
        requireFindsNothing(
            "PostgreSQL version",
            { // Docker image postgres:16.1.
              "2024-01-15 10:30:00.200 UTC [1] LOG:  database system is ready to accept "
              "connections",
              "PostgreSQL init process complete; ready for start up.",
              "PostgreSQL Database directory appears to contain a database; Skipping "
              "initialization",
              // Debian's init script, postgresql-common 238.
              "Starting PostgreSQL 13 database server: main.",
              // psql 16.1, SELECT version();
              " PostgreSQL 16.1 (Debian 16.1-1.pgdg120+1) on x86_64-pc-linux-gnu, compiled by "
              "gcc (Debian 12.2.0-14) 12.2.0, 64-bit",
              "starting postgresql 16.1", "LOG:  starting PostgreSQL" } );
    }
}

SCENARIO( "The MySQL version template finds the version in the ready line", "[ruletemplate]" )
{
    THEN( "the quoted version of MySQL 8, MySQL 5.7 and MariaDB" )
    {
        requireFinds(
            "MySQL version",
            { // Docker image mysql:8.0.36.
              { "2024-01-15T10:30:05.123456Z 0 [System] [MY-010931] [Server] /usr/sbin/mysqld: "
                "ready for connections. Version: '8.0.36'  socket: '/var/run/mysqld/mysqld.sock'  "
                "port: 3306  MySQL Community Server - GPL.",
                "8.0.36" },
              // Docker image mysql:8.4.0.
              { "2024-07-01T08:00:02.654321Z 0 [System] [MY-010931] [Server] /usr/sbin/mysqld: "
                "ready for connections. Version: '8.4.0'  socket: '/var/run/mysqld/mysqld.sock'  "
                "port: 3306  MySQL Community Server - GPL.",
                "8.4.0" },
              // Docker image mysql:5.7.44: the version on a line of its own.
              { "2024-01-15T10:30:05.123456Z 0 [Note] mysqld: ready for connections.\n"
                "Version: '5.7.44'  socket: '/var/run/mysqld/mysqld.sock'  port: 3306  MySQL "
                "Community Server (GPL)",
                "5.7.44" },
              // Docker image mariadb:11.2.2.
              { "2024-01-15 10:30:05 0 [Note] mariadbd: ready for connections.\n"
                "Version: '11.2.2-MariaDB-1:11.2.2+maria~ubu2204'  socket: "
                "'/run/mysqld/mysqld.sock'  port: 3306  mariadb.org binary distribution",
                "11.2.2-MariaDB-1:11.2.2+maria~ubu2204" } } );
    }

    THEN( "of the Docker image's init server and the real one, the first" )
    {
        // Docker image mysql:8.0.36 on its first start: a temporary server on
        // port 0 for the init scripts, then the real one.
        requireFinds(
            "MySQL version",
            { { "2024-01-15T10:29:59.000000Z 0 [System] [MY-010931] [Server] /usr/sbin/mysqld: "
                "ready for connections. Version: '8.0.36'  socket: "
                "'/var/run/mysqld/mysqld.sock'  port: 0  MySQL Community Server - GPL.\n"
                "2024-01-15T10:30:05.123456Z 0 [System] [MY-010931] [Server] /usr/sbin/mysqld: "
                "ready for connections. Version: '8.0.37'  socket: "
                "'/var/run/mysqld/mysqld.sock'  port: 3306  MySQL Community Server - GPL.",
                "8.0.36" } } );
    }

    THEN( "not the start or shutdown line, the X Plugin, the client, or an unquoted version" )
    {
        requireFindsNothing(
            "MySQL version",
            { // Docker image mysql:8.0.36.
              "2024-01-15T10:30:04.000000Z 0 [System] [MY-010116] [Server] /usr/sbin/mysqld "
              "(mysqld 8.0.36) starting as process 1",
              "2024-01-15T10:30:05.000000Z 0 [System] [MY-011323] [Server] X Plugin ready for "
              "connections. Bind-address: '::' port: 33060, socket: /var/run/mysqld/mysqlx.sock",
              "2024-01-15T11:00:00.000000Z 0 [System] [MY-010910] [Server] /usr/sbin/mysqld: "
              "Shutdown complete (mysqld 8.0.36)  MySQL Community Server - GPL.",
              // mysql client 8.0.36 banner.
              "Server version: 8.0.36 MySQL Community Server - GPL", "Version: 8.0.36",
              "version: '8.0.36'", "Version: ''  socket: '/var/run/mysqld/mysqld.sock'" } );
    }
}

SCENARIO( "The Redis version template finds the version in the start line", "[ruletemplate]" )
{
    THEN( "the version after Redis version= up to the comma" )
    {
        requireFinds(
            "Redis version",
            { // Docker image redis:7.2.4.
              { "1:C 15 Jan 2024 10:30:00.123 * oO0OoO0OoO0Oo Redis is starting oO0OoO0OoO0Oo\n"
                "1:C 15 Jan 2024 10:30:00.123 * Redis version=7.2.4, bits=64, commit=00000000, "
                "modified=0, pid=1, just started",
                "7.2.4" },
              // Docker image redis:6.2.14.
              { "1:C 15 Jan 2024 10:30:00.123 # Redis version=6.2.14, bits=64, commit=00000000, "
                "modified=0, pid=1, just started",
                "6.2.14" },
              // Docker image redis:5.0.14.
              { "1:C 15 Jan 2024 10:30:00.123 # Redis version=5.0.14, bits=64, commit=00000000, "
                "modified=0, pid=1, just started",
                "5.0.14" },
              { "Redis version=7.2.4", "7.2.4" } } );
    }

    THEN( "not Valkey, the INFO field, the logo, or no value" )
    {
        requireFindsNothing(
            "Redis version",
            { // Docker image valkey/valkey:8.0.1.
              "1:C 15 Oct 2024 10:30:00.123 * Valkey version=8.0.1, bits=64, commit=00000000, "
              "modified=0, pid=1, just started",
              // redis-cli 7.2.4, INFO server.
              "redis_version:7.2.4",
              // Docker image redis:7.4.0, the logo in a terminal.
              "                Redis Community Edition 7.4.0 (00000000/0) 64 bit",
              "1:C 15 Jan 2024 10:30:00.123 * oO0OoO0OoO0Oo Redis is starting oO0OoO0OoO0Oo",
              "redis version=7.2.4", "Redis version=, bits=64", "Redis version=" } );
    }
}
