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
 * @file ruletemplate_android_test.cpp
 * @brief The Android rule templates: build fingerprint and getprop values.
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

/// The template named @p name in the Android group.
const RuleTemplate& androidTemplate( const QString& name )
{
    const auto& templates = ruleTemplates();
    const auto it
        = std::find_if( templates.begin(), templates.end(), [ & ]( const RuleTemplate& t ) {
              return t.group == "Android" && t.name == name;
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
    const auto entry = androidTemplate( templateName ).entry();
    for ( const auto& sample : samples ) {
        INFO( sample.line );
        REQUIRE( extract( entry, sample.line ) == QString( sample.value ) );
    }
}

void requireFindsNothing( const QString& templateName, std::initializer_list<const char*> lines )
{
    const auto entry = androidTemplate( templateName ).entry();
    for ( const auto* line : lines ) {
        INFO( line );
        REQUIRE( extract( entry, line ) == std::nullopt );
    }
}

} // namespace

SCENARIO( "The Android group holds its templates in order", "[ruletemplate][android]" )
{
    GIVEN( "the templates" )
    {
        QStringList names;
        for ( const auto& t : ruleTemplates() ) {
            if ( t.group == "Android" ) {
                names.append( t.name );
            }
        }

        THEN( "Android has build fingerprint, device model, Android version and serial number" )
        {
            REQUIRE( names
                     == QStringList{ "Build fingerprint", "Device model", "Android version",
                                     "Device serial number" } );
        }

        THEN( "each has its key, the serial number the key of General's" )
        {
            for ( const auto& [ name, key ] :
                  { std::pair{ "Build fingerprint", "Build fingerprint" },
                    std::pair{ "Device model", "Device model" },
                    std::pair{ "Android version", "Android version" },
                    std::pair{ "Device serial number", "Serial number" } } ) {
                INFO( name );
                REQUIRE( androidTemplate( name ).key == key );
                REQUIRE( !androidTemplate( name ).asksForKey() );
            }
        }

        THEN( "the getprop values are simple rules ending at ], the fingerprint is advanced" )
        {
            for ( const auto& [ name, property ] :
                  { std::pair{ "Device model", "ro.product.model" },
                    std::pair{ "Android version", "ro.build.version.release" },
                    std::pair{ "Device serial number", "ro.serialno" } } ) {
                INFO( name );
                const auto& t = androidTemplate( name );
                REQUIRE( t.isSimple() );
                REQUIRE( t.simple.textBefore == QString( "[%1]: [" ).arg( property ) );
                REQUIRE( t.simple.valueEnd == ValueEnd::Character );
                REQUIRE( t.simple.endCharacter == "]" );
                const auto entry = t.entry();
                REQUIRE( simpleRuleOf( entry ) == t.simple );
            }
            const auto& fingerprint = androidTemplate( "Build fingerprint" );
            REQUIRE( !fingerprint.isSimple() );
            REQUIRE( !simpleRuleOf( fingerprint.entry() ).has_value() );
        }
    }
}

SCENARIO( "The build fingerprint template finds a fingerprint by its shape",
          "[ruletemplate][android]" )
{
    THEN( "the fingerprint in a tombstone, a crash in logcat, a bug report and getprop" )
    {
        requireFinds(
            "Build fingerprint",
            {
                // Tombstone, debuggerd of Android 11 (Pixel 4a)
                { "Build fingerprint: "
                  "'google/sunfish/sunfish:11/RQ3A.210805.001.A1/7474174:user/release-keys'",
                  "google/sunfish/sunfish:11/RQ3A.210805.001.A1/7474174:user/release-keys" },
                // logcat -v threadtime, Android 14 emulator (adb 35.0.1)
                { "02-20 10:15:32.481  1234  1234 F DEBUG   : Build fingerprint: "
                  "'google/sdk_gphone64_arm64/emu64a:14/UE1A.230829.036.A1/11228894:userdebug/"
                  "dev-keys'",
                  "google/sdk_gphone64_arm64/emu64a:14/UE1A.230829.036.A1/11228894:userdebug/"
                  "dev-keys" },
                // adb bugreport header, Android 8.1 (Pixel 2)
                { "Build fingerprint: "
                  "'google/walleye/walleye:8.1.0/OPM1.171019.011/4448085:user/release-keys'",
                  "google/walleye/walleye:8.1.0/OPM1.171019.011/4448085:user/release-keys" },
                // adb shell getprop, Android 14 emulator (adb 35.0.1)
                { "[ro.build.fingerprint]: "
                  "[google/sdk_gphone64_arm64/emu64a:14/UE1A.230829.036.A1/11228894:userdebug/"
                  "dev-keys]",
                  "google/sdk_gphone64_arm64/emu64a:14/UE1A.230829.036.A1/11228894:userdebug/"
                  "dev-keys" },
                // An eng build with several tags and an incremental with dots
                { "fingerprint=generic/aosp_x86/generic_x86:9/PQ3A.190801.002/"
                  "eng.user.20240101.120000:eng/test-keys,dev-keys",
                  "generic/aosp_x86/generic_x86:9/PQ3A.190801.002/"
                  "eng.user.20240101.120000:eng/test-keys,dev-keys" },
            } );
    }

    THEN( "not a path, a URL, another build type or a fingerprint cut short" )
    {
        requireFindsNothing(
            "Build fingerprint",
            {
                // Paths from logcat, Android 14 emulator (adb 35.0.1)
                "02-20 10:15:33.002  1234  1234 F DEBUG   :       #00 pc 000000000005b5c0  "
                "/apex/com.android.runtime/lib64/bionic/libc.so (abort+164)",
                "/data/app/com.example/base.apk:classes.dex/x/y:user/release-keys",
                "https://example.com/a/b:1/c/d:user/release-keys",
                "google/sunfish/sunfish:11/RQ3A.210805.001.A1/7474174:debug/release-keys",
                "google/sunfish/sunfish:11/RQ3A.210805.001.A1/7474174:user/",
                "google/sunfish:11/RQ3A.210805.001.A1/7474174:user/release-keys",
                "xgoogle/sunfish/sunfish:11/RQ3A/7474174:user/release-keys/more",
            } );
    }
}

SCENARIO( "The getprop templates find the value in brackets", "[ruletemplate][android]" )
{
    // adb shell getprop, Android 14 on a Pixel 7 (adb 35.0.1)
    THEN( "the device model, spaces included" )
    {
        requireFinds( "Device model", { { "[ro.product.model]: [Pixel 7]", "Pixel 7" },
                                        { "[ro.product.model]: [SM-G991B]", "SM-G991B" } } );
    }

    THEN( "the Android version" )
    {
        requireFinds( "Android version", { { "[ro.build.version.release]: [14]", "14" },
                                           { "[ro.build.version.release]: [8.1.0]", "8.1.0" } } );
    }

    THEN( "the serial number" )
    {
        requireFinds( "Device serial number",
                      { { "[ro.serialno]: [28031FDH2004VT]", "28031FDH2004VT" },
                        { "[ro.serialno]: [EMULATOR34X1X10X0]", "EMULATOR34X1X10X0" } } );
    }

    THEN( "not a longer key that ends the same, an empty value or the build.prop form" )
    {
        requireFindsNothing( "Device model",
                             { "[ro.product.vendor.model]: [Pixel 7]", "[ro.product.model]: []",
                               "ro.product.model=Pixel 7" } );
        requireFindsNothing( "Android version",
                             { "[ro.build.version.release_or_codename]: [14]",
                               "[ro.build.version.sdk]: [34]", "ro.build.version.release=14" } );
        requireFindsNothing( "Device serial number",
                             { "[ro.boot.serialno]: [28031FDH2004VT]", "[ro.serialno]: []",
                               "Serial number: SN-00042" } );
    }
}
