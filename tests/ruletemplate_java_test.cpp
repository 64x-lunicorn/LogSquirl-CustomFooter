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
 * @file ruletemplate_java_test.cpp
 * @brief The Java and Spring Boot template group: what each template finds.
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

const QString javaGroup = QStringLiteral( "Java and Spring Boot" );

/// The template named @p name in the Java and Spring Boot group.
const RuleTemplate& javaTemplate( const QString& name )
{
    const auto& templates = ruleTemplates();
    const auto it
        = std::find_if( templates.begin(), templates.end(), [ & ]( const RuleTemplate& t ) {
              return t.group == javaGroup && t.name == name;
          } );
    REQUIRE( it != templates.end() );
    return *it;
}

/// The value @p entry extracts from @p line through the scanner, as the
/// footer would show it; nothing if it does not match.
std::optional<QString> extract( const FooterEntry& entry, const QString& line )
{
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
    const auto entry = javaTemplate( templateName ).entry();
    for ( const auto& sample : samples ) {
        INFO( sample.line );
        REQUIRE( extract( entry, sample.line ) == QString( sample.value ) );
    }
}

void requireFindsNothing( const QString& templateName, std::initializer_list<const char*> lines )
{
    const auto entry = javaTemplate( templateName ).entry();
    for ( const auto* line : lines ) {
        INFO( line );
        REQUIRE( extract( entry, line ) == std::nullopt );
    }
}

// Spring Boot 4.1.1 reference documentation, "Log Format" (Spring Boot 3
// words its start line the same way: no host).
const char* const boot4Start
    = "2026-08-17T10:51:28.508Z  INFO 11390 --- [myapp] [           main] "
      "o.s.b.d.f.logexample.MyApplication       : Starting MyApplication using Java 25.0.4 "
      "with PID 11390 (/opt/apps/myapp.jar started by myuser in /opt/apps/)";
// Spring Boot 3.3, as StartupInfoLogger writes it for a jar with an
// Implementation-Version.
const char* const boot3Start
    = "2024-06-03T09:12:44.871+02:00  INFO 4242 --- [demo] [           main] "
      "com.example.demo.DemoApplication         : Starting DemoApplication v0.0.1-SNAPSHOT "
      "using Java 17.0.8 with PID 4242 (/opt/demo/demo.jar started by app in /opt/demo)";
// Spring Boot 3.0, a native image (AOT-processed).
const char* const boot3AotStart
    = "2023-01-12T15:20:01.003Z  INFO 1 --- [           main] "
      "com.example.demo.DemoApplication         : Starting AOT-processed DemoApplication "
      "v1.4.2 using Java 17.0.5 with PID 1 (/workspace/demo started by cnb in /workspace)";
// Spring Boot 2.7.18 reference documentation, "SpringApplication".
const char* const boot27Start
    = "2023-11-23 07:23:23.238  INFO 28579 --- [           main] "
      "o.s.b.d.f.logexample.MyApplication       : Starting MyApplication using Java 1.8.0_392 "
      "on myhost with PID 28579 (/opt/apps/myapp.jar started by myuser in /opt/apps/)";
// Spring Boot 2.7, with an Implementation-Version.
const char* const boot27VersionStart
    = "2022-11-24 16:02:11.417  INFO 7311 --- [           main] "
      "com.example.shop.ShopApplication         : Starting ShopApplication v2.3.0 using Java "
      "11.0.17 on shop-01.example.com with PID 7311 (/srv/shop/shop.jar started by shop in "
      "/srv/shop)";
// Spring Boot 2.2 reference documentation, "Log Format" (before 2.4 there is
// no Java version).
const char* const boot22Start
    = "2019-03-05 10:57:51.112  INFO 45469 --- [           main] "
      "o.s.b.s.app.SampleApplication            : Starting SampleApplication v0.1.0 on "
      "mycomputer with PID 45469 (/apps/myapp.jar started by pwebb)";

// Other lines of the same Spring Boot 4.1.1 log, which start or name the
// application but are not its start line.
const char* const tomcatService
    = "2026-08-17T10:51:33.232Z  INFO 11390 --- [myapp] [           main] "
      "o.apache.catalina.core.StandardService   : Starting service [Tomcat]";
const char* const tomcatEngine
    = "2026-08-17T10:51:33.239Z  INFO 11390 --- [myapp] [           main] "
      "o.apache.catalina.core.StandardEngine    : Starting Servlet engine: [Apache Tomcat/11.0.24]";
const char* const started
    = "2026-08-17T10:51:35.286Z  INFO 11390 --- [myapp] [           main] "
      "o.s.b.d.f.logexample.MyApplication       : Started MyApplication in 8.572 seconds "
      "(process running for 10.155)";

} // namespace

SCENARIO( "The Java and Spring Boot group holds its templates in order", "[ruletemplate]" )
{
    GIVEN( "the templates of the group" )
    {
        QStringList names;
        for ( const auto& t : ruleTemplates() ) {
            if ( t.group == javaGroup ) {
                names.append( t.name );
            }
        }

        THEN( "they are the Spring Boot start line's values, the profile and the JRE version" )
        {
            REQUIRE( names
                     == QStringList{ "Application", "Application version", "Java version", "Host",
                                     "Active profile", "JRE version" } );
        }

        THEN( "each has the key its name says" )
        {
            for ( const auto& name : names ) {
                INFO( name.toStdString() );
                const auto& t = javaTemplate( name );
                REQUIRE( t.key == name );
                REQUIRE( t.entry().key == name );
            }
        }
    }
}

SCENARIO( "Java and Spring Boot templates are simple where a fixed text comes before the value",
          "[ruletemplate]" )
{
    THEN( "Java version is the word after using Java, a simple rule" )
    {
        const auto& t = javaTemplate( "Java version" );
        REQUIRE( t.isSimple() );
        REQUIRE( t.simple.textBefore == "using Java" );
        REQUIRE( t.simple.valueEnd == ValueEnd::Whitespace );
        const auto entry = t.entry();
        REQUIRE( simpleRuleOf( entry ).has_value() );
    }

    THEN( "the others need the shape of their line and are advanced" )
    {
        for ( const auto* name :
              { "Application", "Application version", "Host", "Active profile", "JRE version" } ) {
            INFO( name );
            const auto& t = javaTemplate( name );
            REQUIRE( !t.isSimple() );
            REQUIRE( !simpleRuleOf( t.entry() ).has_value() );
        }
    }
}

SCENARIO( "The application template finds the application of the Spring Boot start line",
          "[ruletemplate]" )
{
    THEN( "in the wording of Spring Boot 2 and 3, with or without a version" )
    {
        requireFinds( "Application", { { boot4Start, "MyApplication" },
                                       { boot3Start, "DemoApplication" },
                                       { boot3AotStart, "DemoApplication" },
                                       { boot27Start, "MyApplication" },
                                       { boot27VersionStart, "ShopApplication" },
                                       { boot22Start, "SampleApplication" } } );
    }

    THEN( "not in other lines that start something or name the application" )
    {
        requireFindsNothing( "Application",
                             { tomcatService, tomcatEngine, started, "Starting MyApplication",
                               "Starting MyApplication using Java 17.0.8",
                               "Restarting MyApplication using Java 17.0.8 with PID 1" } );
    }
}

SCENARIO( "The application version template finds the version of the Spring Boot start line",
          "[ruletemplate]" )
{
    THEN( "the version after v, without the v" )
    {
        requireFinds( "Application version", { { boot3Start, "0.0.1-SNAPSHOT" },
                                               { boot3AotStart, "1.4.2" },
                                               { boot27VersionStart, "2.3.0" },
                                               { boot22Start, "0.1.0" } } );
    }

    THEN( "not when the application has no version, nor in other lines" )
    {
        requireFindsNothing( "Application version",
                             { boot4Start, boot27Start, tomcatService, tomcatEngine, started,
                               "Starting MyApplication v1.0" } );
    }
}

SCENARIO( "The Java version template finds the version after using Java", "[ruletemplate]" )
{
    THEN( "in the start line of Spring Boot 2.4 and later" )
    {
        requireFinds( "Java version", { { boot4Start, "25.0.4" },
                                        { boot3Start, "17.0.8" },
                                        { boot3AotStart, "17.0.5" },
                                        { boot27Start, "1.8.0_392" },
                                        { boot27VersionStart, "11.0.17" } } );
    }

    THEN( "not before Spring Boot 2.4, which has no Java version, nor in other lines" )
    {
        requireFindsNothing( "Java version",
                             { boot22Start, tomcatEngine, started, "using java 17.0.8" } );
    }
}

SCENARIO( "The host template finds the host of the Spring Boot 2 start line", "[ruletemplate]" )
{
    THEN( "the word between on and with PID" )
    {
        requireFinds( "Host", { { boot27Start, "myhost" },
                                { boot27VersionStart, "shop-01.example.com" },
                                { boot22Start, "mycomputer" } } );
    }

    THEN( "not in Spring Boot 3 and later, which no longer logs the host, nor in other lines" )
    {
        requireFindsNothing( "Host", { boot4Start, boot3Start, boot3AotStart, tomcatService,
                                       started, "Listening on 0.0.0.0 with PID 12" } );
    }
}

SCENARIO( "The active profile template finds the active profiles", "[ruletemplate]" )
{
    THEN( "one profile without its quotes, several as listed" )
    {
        requireFinds(
            "Active profile",
            {
                // Spring Boot 3.3 and 2.7 (quoted, with a count)
                { "2024-06-03T09:12:45.002+02:00  INFO 4242 --- [demo] [           main] "
                  "com.example.demo.DemoApplication         : The following 1 profile is active: "
                  "\"prod\"",
                  "prod" },
                { "2024-06-03T09:12:45.002+02:00  INFO 4242 --- [demo] [           main] "
                  "com.example.demo.DemoApplication         : The following 2 profiles are "
                  "active: \"dev\", \"local\"",
                  "\"dev\", \"local\"" },
                // Spring Boot 2.3 to 2.6 (unquoted, without a count)
                { "2020-09-14 11:02:37.456  INFO 9876 --- [           main] "
                  "com.example.shop.ShopApplication         : The following profiles are active: "
                  "dev,local",
                  "dev,local" },
            } );
    }

    THEN( "not the default profile Spring Boot falls back to, nor other lines" )
    {
        requireFindsNothing(
            "Active profile",
            { // Spring Boot 4.1.1 reference documentation
              "2026-08-17T10:51:28.527Z  INFO 11390 --- [myapp] [           main] "
              "o.s.b.d.f.logexample.MyApplication       : No active profile set, falling back to 1 "
              "default profile: \"default\"",
              // Spring Boot 2.3
              "No active profile set, falling back to default profiles: default",
              "The following 1 profile is active: ", boot4Start } );
    }
}

SCENARIO( "The JRE version template finds the version of a JVM crash log", "[ruletemplate]" )
{
    THEN( "the runtime version in (build …) of the JRE version line" )
    {
        requireFinds(
            "JRE version",
            {
                // hs_err of Temurin 11.0.12
                { "# JRE version: OpenJDK Runtime Environment Temurin-11.0.12+7 (11.0.12+7) "
                  "(build 11.0.12+7)",
                  "11.0.12+7" },
                // hs_err of Oracle Java 8u201
                { "# JRE version: Java(TM) SE Runtime Environment (8.0_201-b09) "
                  "(build 1.8.0_201-b09)",
                  "1.8.0_201-b09" },
                // hs_err of a fastdebug OpenJDK 17 build (JDK 17 vmError.cpp format)
                { "# JRE version: OpenJDK Runtime Environment (17.0.1) "
                  "(fastdebug build 17.0.1-internal+0-adhoc.user.jdk17u)",
                  "17.0.1-internal+0-adhoc.user.jdk17u" },
            } );
    }

    THEN( "not without a build, as when the JVM crashes while starting, nor in other lines" )
    {
        requireFindsNothing(
            "JRE version",
            { // hs_err of a JVM crashing before its properties are set (JDK 17)
              "# JRE version:  (17.0.1+12) (build )",
              // hs_err of Temurin 11.0.12
              "# Java VM: OpenJDK 64-Bit Server VM Temurin-11.0.12+7 (11.0.12+7, mixed mode, "
              "tiered, compressed oops, g1 gc, windows-amd64)",
              // java -version of Temurin 17.0.4
              "OpenJDK Runtime Environment Temurin-17.0.4+8 (build 17.0.4+8)", boot4Start } );
    }
}
