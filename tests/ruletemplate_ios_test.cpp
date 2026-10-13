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
 * @file ruletemplate_ios_test.cpp
 * @brief The iOS crash log template group: what each template finds in the
 * header of a crash report.
 */

#include <catch2/catch.hpp>

#include "footerscanner.h"
#include "ruletemplate.h"
#include "simplerule.h"

#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include <algorithm>
#include <optional>

using namespace custom_footer;

namespace {

const QString kGroup = QStringLiteral( "iOS crash log" );

const RuleTemplate& templateNamed( const QString& name )
{
    const auto& templates = ruleTemplates();
    const auto it = std::find_if( templates.begin(), templates.end(),
                                  [ & ]( const RuleTemplate& t ) { return t.name == name; } );
    REQUIRE( it != templates.end() );
    return *it;
}

/// The value the template named @p templateName extracts from a file of
/// @p lines through the scanner, as the footer would show it; nothing if it
/// does not match.
std::optional<QString> extractFromFile( const QString& templateName, const QStringList& lines )
{
    const auto entry = templateNamed( templateName ).entry();
    REQUIRE( FooterScanner::patternError( entry.linePattern ).isEmpty() );
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto path = dir.path() + "/sample.crash";
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    for ( const auto& line : lines ) {
        file.write( line.toUtf8() );
        file.write( "\n" );
    }
    file.close();

    const auto values = FooterScanner::scan( path, { entry } );
    if ( !values.contains( entry.key ) ) {
        return std::nullopt;
    }
    return values.value( entry.key );
}

std::optional<QString> extract( const QString& templateName, const QString& line )
{
    return extractFromFile( templateName, { "first line", line, "last line" } );
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

/// The header of a crash report of an iPad app, iPadOS 17.1.2, as posted on
/// the Apple Developer Forums (attachment 3db418ac-3560-4ff0-9180-feaf8b35fde5).
const QStringList kIPadHeader = {
    "-------------------------------------",
    "Translated Report (Full Report Below)",
    "-------------------------------------",
    "",
    "Incident Identifier: DBA6F606-C789-4A20-BD4F-ED24D4ABE2AB",
    "CrashReporter Key:      2f82a89732ce17f2b74b7c8542f5fd238e77ae15",
    "Hardware Model:            iPad14,5",
    "Process:                          iDentify [2730]",
    "Path:                                "
    "/private/var/containers/Bundle/Application/9C482633-822C-4CE4-99CA-33B955883980/"
    "iDentify.app/iDentify",
    "Identifier:                    com.iseektest.net",
    "Version:                          1.0 (1)",
    "Code Type:                      ARM-64 (Native)",
    "Role:                                Foreground",
    "Parent Process:            launchd [1]",
    "Coalition:                      com.iseektest.net [470]",
    "",
    "Date/Time:                      2024-05-15 10:18:34.3841 -0500",
    "Launch Time:                  2024-05-15 10:18:34.2695 -0500",
    "OS Version:                    iPhone OS 17.1.2 (21B101)",
    "Release Type:                User",
    "Report Version:            104",
    "",
    "Exception Type:    EXC_CRASH (SIGABRT)",
};

/// The header of an iPhone crash report, iOS 17.6.1, whose process fields
/// are empty, as posted on the Apple Developer Forums (attachment
/// 60a01133-acec-496d-9180-10e9342af69a). The values end in spaces.
const QStringList kEmptyVersionHeader = {
    "Incident Identifier: 32764EDA-B9AC-46BF-B26F-DDD66E37CCB8",
    "Distributor ID:      com.apple.AppStore",
    "Hardware Model:      iPhone15,5",
    "Process:             ",
    "Path:                ",
    "Identifier:          ",
    "Version:             ",
    "AppStoreTools:       16A242d",
    "AppVariant:          1:iPhone15,5:17",
    "Code Type:           ARM-64 (Native)",
    "Role:                Foreground",
    "Parent Process:      launchd [1]",
    "Coalition:           ",
    "",
    "Date/Time:           2024-10-30 20:44:26.7576 +0000",
    "Launch Time:         2024-10-30 04:41:25.5841 +0000",
    "OS Version:          iPhone OS 17.6.1 (21G93)",
    "Release Type:        User",
    "Baseband Version:    1.70.02",
    "Report Version:      104",
};

} // namespace

SCENARIO( "The iOS crash log group holds its templates", "[ruletemplate][ios]" )
{
    GIVEN( "the templates" )
    {
        THEN( "the group holds hardware model, OS version, incident identifier and app version, "
              "in this order" )
        {
            QStringList names;
            for ( const auto& t : ruleTemplates() ) {
                if ( t.group == kGroup ) {
                    names.append( t.name );
                }
            }
            REQUIRE( names
                     == QStringList{ "Hardware model", "OS version", "Incident identifier",
                                     "App version" } );
        }

        THEN( "each has its key" )
        {
            REQUIRE( templateNamed( "Hardware model" ).key == "Hardware model" );
            REQUIRE( templateNamed( "OS version" ).key == "OS version" );
            REQUIRE( templateNamed( "Incident identifier" ).key == "Incident identifier" );
            REQUIRE( templateNamed( "App version" ).key == "App version" );
        }

        THEN( "hardware model, OS version and incident identifier are simple rules" )
        {
            for ( const auto* name : { "Hardware model", "OS version", "Incident identifier" } ) {
                INFO( name );
                const auto& t = templateNamed( name );
                REQUIRE( t.isSimple() );
                const auto entry = t.entry();
                REQUIRE( simpleRuleOf( entry ).has_value() );
                REQUIRE( entry.linePattern == simpleLinePattern( *simpleRuleOf( entry ) ) );
            }
            REQUIRE( templateNamed( "Hardware model" ).simple.valueEnd == ValueEnd::Whitespace );
            REQUIRE( templateNamed( "Incident identifier" ).simple.valueEnd
                     == ValueEnd::Whitespace );
            REQUIRE( templateNamed( "OS version" ).simple.valueEnd == ValueEnd::EndOfLine );
        }

        THEN( "app version must not take OS Version: and is advanced" )
        {
            const auto& t = templateNamed( "App version" );
            REQUIRE( !t.isSimple() );
            REQUIRE( !simpleRuleOf( t.entry() ).has_value() );
        }
    }
}

SCENARIO( "The iOS crash log templates find the values of a crash report header",
          "[ruletemplate][ios]" )
{
    GIVEN( "the Hardware model template" )
    {
        THEN( "it finds the model identifier after Hardware Model:" )
        {
            requireFinds( "Hardware model",
                          {
                              // Apple, "Examining the fields in a crash report", iPhone OS 13.3.1
                              { "Hardware Model:      iPhone8,1", "iPhone8,1" },
                              // Apple Developer Forums, iPadOS 17.1.2
                              { "Hardware Model:            iPad14,5", "iPad14,5" },
                              // Apple Developer Forums, iOS 17.1.2, Report Version 105
                              { "Hardware Model:      iPhone16,1", "iPhone16,1" },
                              // Apple Developer Forums, macOS 14.6.1 (Simulator process)
                              { "Hardware Model:      Mac14,6", "Mac14,6" },
                          } );
        }

        THEN( "it finds nothing in other fields, the JSON of the full report or an empty field" )
        {
            requireFindsNothing( "Hardware model",
                                 {
                                     // The JSON below a translated report, iPadOS 17.1.2
                                     "    \"modelCode\" : \"iPad14,5\",",
                                     "AppVariant:          1:iPhone15,5:17",
                                     "hardware model: iPhone8,1",
                                     "Hardware Model:      ",
                                 } );
        }
    }

    GIVEN( "the OS version template" )
    {
        THEN( "it finds the system name, version and build up to the end of the line" )
        {
            requireFinds(
                "OS version",
                {
                    // Apple, "Examining the fields in a crash report"
                    { "OS Version:          iPhone OS 13.3.1 (17D50)", "iPhone OS 13.3.1 (17D50)" },
                    // Apple Developer Forums, iPadOS 17.1.2
                    { "OS Version:                    iPhone OS 17.1.2 (21B101)",
                      "iPhone OS 17.1.2 (21B101)" },
                    // Apple Developer Forums, iOS 17.1.2, Report Version 105
                    { "OS Version:          iOS 17.1.2 (21B101)", "iOS 17.1.2 (21B101)" },
                    // Apple Developer Forums, macOS 14.6.1
                    { "OS Version:          macOS 14.6.1 (23G93)", "macOS 14.6.1 (23G93)" },
                    // Trailing spaces are not part of the value.
                    { "OS Version:          iPhone OS 17.6.1 (21G93)   ",
                      "iPhone OS 17.6.1 (21G93)" },
                } );
        }

        THEN( "it finds nothing in other version fields, the JSON or an empty field" )
        {
            requireFindsNothing(
                "OS version",
                {
                    // The JSON of the full report, iPadOS 17.1.2 (one key of its first line)
                    "\"bug_type\":\"309\",\"os_version\":\"iPhone OS 17.1.2 (21B101)\"",
                    "Baseband Version:    1.70.02",
                    "Report Version:      104",
                    "Version:             1 (3.0)",
                    "OS Version:          ",
                } );
        }
    }

    GIVEN( "the Incident identifier template" )
    {
        THEN( "it finds the UUID after Incident Identifier:" )
        {
            requireFinds( "Incident identifier",
                          {
                              // Apple, "Examining the fields in a crash report"
                              { "Incident Identifier: 6156848E-344E-4D9E-84E0-87AFD0D0AE7B",
                                "6156848E-344E-4D9E-84E0-87AFD0D0AE7B" },
                              // Apple Developer Forums, macOS 14.6.1
                              { "Incident Identifier: 79694C62-168B-4EAE-B929-D5328B90E654",
                                "79694C62-168B-4EAE-B929-D5328B90E654" },
                          } );
        }

        THEN( "it finds nothing in other identifiers, the JSON or an empty field" )
        {
            requireFindsNothing( "Incident identifier",
                                 {
                                     // The JSON of the full report, macOS 14.6.1
                                     "  \"incident\" : \"79694C62-168B-4EAE-B929-D5328B90E654\",",
                                     "CrashReporter Key:   1032EC0D-AECB-8108-AECA-E5D59E38FB9F",
                                     "Identifier:          com.apple.WebKit.Networking",
                                     "Incident Identifier: ",
                                 } );
        }
    }

    GIVEN( "the App version template" )
    {
        THEN( "it finds the app's version up to the end of the line" )
        {
            requireFinds( "App version",
                          {
                              // Apple, "Examining the fields in a crash report": CFBundleVersion
                              // and, in parentheses, CFBundleShortVersionString
                              { "Version:             1 (3.0)", "1 (3.0)" },
                              // Apple Developer Forums, iPadOS 17.1.2
                              { "Version:                          1.0 (1)", "1.0 (1)" },
                              // Apple Developer Forums, iOS 17.1.2, Report Version 105
                              { "Version:             5.3.0", "5.3.0" },
                              // Apple Developer Forums, macOS 14.6.1
                              { "Version:             8619.1.26.30.5", "8619.1.26.30.5" },
                              // After other text, such as a prefix a log adds.
                              { "crash: Version: 1.0 (1)  ", "1.0 (1)" },
                          } );
        }

        THEN( "it finds nothing in OS, baseband or report version, the JSON or an empty field" )
        {
            requireFindsNothing(
                "App version",
                {
                    // Apple Developer Forums, iOS 17.6.1
                    "OS Version:          iPhone OS 17.6.1 (21G93)",
                    "Baseband Version:    1.70.02",
                    "Report Version:      104",
                    // The JSON of the full report, iPadOS 17.1.2 (one key of its first line)
                    "\"app_version\":\"1.0\"",
                    "AppVersion: 1.0",
                    "Version:             ",
                } );
        }
    }
}

SCENARIO( "The iOS crash log templates take the values meant from a whole header",
          "[ruletemplate][ios]" )
{
    GIVEN( "the header of an iPad crash report" )
    {
        THEN( "each template finds its value" )
        {
            REQUIRE( extractFromFile( "Hardware model", kIPadHeader ) == QString( "iPad14,5" ) );
            REQUIRE( extractFromFile( "OS version", kIPadHeader )
                     == QString( "iPhone OS 17.1.2 (21B101)" ) );
            REQUIRE( extractFromFile( "Incident identifier", kIPadHeader )
                     == QString( "DBA6F606-C789-4A20-BD4F-ED24D4ABE2AB" ) );
            REQUIRE( extractFromFile( "App version", kIPadHeader ) == QString( "1.0 (1)" ) );
        }
    }

    GIVEN( "a header whose Version: is empty" )
    {
        THEN( "app version finds nothing rather than the OS, baseband or report version" )
        {
            REQUIRE( extractFromFile( "App version", kEmptyVersionHeader ) == std::nullopt );
        }

        THEN( "the other templates still find their values" )
        {
            REQUIRE( extractFromFile( "Hardware model", kEmptyVersionHeader )
                     == QString( "iPhone15,5" ) );
            REQUIRE( extractFromFile( "OS version", kEmptyVersionHeader )
                     == QString( "iPhone OS 17.6.1 (21G93)" ) );
            REQUIRE( extractFromFile( "Incident identifier", kEmptyVersionHeader )
                     == QString( "32764EDA-B9AC-46BF-B26F-DDD66E37CCB8" ) );
        }
    }
}
