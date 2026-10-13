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
 * @file ruletemplate_network_test.cpp
 * @brief The Network template group: IP, MAC and IPv6 addresses after a
 * text.
 */

#include <catch2/catch.hpp>

#include "footerscanner.h"
#include "ruletemplate.h"
#include "simplerule.h"

#include <QFile>
#include <QRegularExpression>
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

/// The rule of the template named @p name, with the text before the value
/// that starts its pattern, @p text, replaced by @p adjusted, as a user
/// adjusts it to the log.
FooterEntry adjusted( const QString& name, const QString& text, const QString& adjusted )
{
    auto entry = templateNamed( name ).entry();
    REQUIRE( entry.linePattern.startsWith( text ) );
    entry.linePattern.replace( 0, text.size(), QRegularExpression::escape( adjusted ) );
    return entry;
}

/// Lines a rule finds, with the value it takes from each.
struct Sample {
    const char* line;
    const char* value;
};

void requireFinds( const FooterEntry& entry, std::initializer_list<Sample> samples )
{
    for ( const auto& sample : samples ) {
        INFO( sample.line );
        REQUIRE( extract( entry, sample.line ) == QString( sample.value ) );
    }
}

void requireFinds( const QString& templateName, std::initializer_list<Sample> samples )
{
    requireFinds( templateNamed( templateName ).entry(), samples );
}

void requireFindsNothing( const FooterEntry& entry, std::initializer_list<const char*> lines )
{
    for ( const auto* line : lines ) {
        INFO( line );
        REQUIRE( extract( entry, line ) == std::nullopt );
    }
}

void requireFindsNothing( const QString& templateName, std::initializer_list<const char*> lines )
{
    requireFindsNothing( templateNamed( templateName ).entry(), lines );
}

} // namespace

SCENARIO( "The Network group holds the address templates", "[ruletemplate]" )
{
    GIVEN( "the templates" )
    {
        THEN( "Network holds IPv4, MAC and IPv6 address, in this order" )
        {
            QStringList names;
            for ( const auto& t : ruleTemplates() ) {
                if ( t.group == "Network" ) {
                    names.append( t.name );
                }
            }
            REQUIRE( names == QStringList{ "IPv4 address", "MAC address", "IPv6 address" } );
        }

        THEN( "each has its key" )
        {
            REQUIRE( templateNamed( "IPv4 address" ).key == "IP address" );
            REQUIRE( templateNamed( "MAC address" ).key == "MAC address" );
            REQUIRE( templateNamed( "IPv6 address" ).key == "IPv6 address" );
        }

        THEN( "the IPv4 address is simple; MAC and IPv6 need a shape and are advanced" )
        {
            REQUIRE( templateNamed( "IPv4 address" ).isSimple() );
            for ( const auto* name : { "MAC address", "IPv6 address" } ) {
                INFO( name );
                const auto& t = templateNamed( name );
                REQUIRE( !t.isSimple() );
                REQUIRE( !t.asksForKey() );
                REQUIRE( !simpleRuleOf( t.entry() ).has_value() );
            }
        }

        THEN( "MAC and IPv6 start with the text to adjust, and their descriptions say so" )
        {
            REQUIRE( templateNamed( "MAC address" ).linePattern.startsWith( "MAC:" ) );
            REQUIRE( templateNamed( "IPv6 address" ).linePattern.startsWith( "IPv6:" ) );
            for ( const auto* name : { "MAC address", "IPv6 address" } ) {
                INFO( name );
                REQUIRE( templateNamed( name ).description.contains( "adjust the text" ) );
            }
        }
    }
}

SCENARIO( "The MAC address template finds the address after MAC:", "[ruletemplate]" )
{
    THEN( "the three forms, with or without a space, in either case" )
    {
        requireFinds( "MAC address", { { "MAC: 52:54:00:12:34:56", "52:54:00:12:34:56" },
                                       { "wlan0 MAC:A4-83-E7-1F-2B-9C up", "A4-83-E7-1F-2B-9C" },
                                       { "MAC:   0050.56a1.b2c3", "0050.56a1.b2c3" },
                                       { "MAC: 3C:22:FB:aa:0b:9e", "3C:22:FB:aa:0b:9e" } } );
    }

    THEN( "the address after the text, not the first address in the line" )
    {
        requireFinds( "MAC address", { { "peer 11:22:33:44:55:66 local MAC: 52:54:00:12:34:56",
                                         "52:54:00:12:34:56" },
                                       { "MAC: 52:54:00:12:34:56, link up", "52:54:00:12:34:56" },
                                       { "(MAC: 0050.56a1.b2c3).", "0050.56a1.b2c3" } } );
    }

    THEN( "the text adjusted to a real log" )
    {
        // iproute2 6.1.0 (Debian 12), `ip link show`
        requireFinds(
            adjusted( "MAC address", "MAC:", "link/ether" ),
            { { "    link/ether 52:54:00:3a:7c:01 brd ff:ff:ff:ff:ff:ff", "52:54:00:3a:7c:01" } } );
        // Windows 11 23H2 (10.0.22631), `ipconfig /all`
        requireFinds( adjusted( "MAC address", "MAC:", "Physical Address. . . . . . . . . :" ),
                      { { "   Physical Address. . . . . . . . . : 00-1A-2B-3C-4D-5E",
                          "00-1A-2B-3C-4D-5E" } } );
        // Cisco IOS 15.2, `show interfaces`
        requireFinds( adjusted( "MAC address", "MAC:", "address is" ),
                      { { "  Hardware is Gigabit Ethernet, address is 0050.56a1.b2c3 "
                          "(bia 0050.56a1.b2c3)",
                          "0050.56a1.b2c3" } } );
        // macOS 14 (Sonoma), `ifconfig en0`
        requireFinds( adjusted( "MAC address", "MAC:", "ether" ),
                      { { "\tether a4:83:e7:1f:2b:9c", "a4:83:e7:1f:2b:9c" } } );
    }

    THEN( "not mixed separators, a wrong length, no hex, or without the text" )
    {
        requireFindsNothing(
            "MAC address",
            { "MAC: 52:54:00-12:34:56", "MAC: 52-54-00:12:34:56", "MAC: 0050.56a1:b2c3",
              "MAC: 52:54:00:12:34", "MAC: 52:54:00:12:34:56:78:9a", "MAC: 52:54:00:12:34:5",
              "MAC: 52:54:00:12:34:567", "MAC: 5254.0012.34", "MAC: 525400123456",
              "MAC: 52:54:00:12:34:zz", "MAC: unknown", "MAC:", "link/ether 52:54:00:3a:7c:01",
              "mac: 52:54:00:12:34:56", "MAC 52:54:00:12:34:56" } );
    }
}

SCENARIO( "The IPv6 address template finds the address after IPv6:", "[ruletemplate]" )
{
    THEN( "full, shortened with ::, and with an embedded IPv4 address" )
    {
        requireFinds(
            "IPv6 address",
            { { "IPv6: 2001:0db8:85a3:0000:0000:8a2e:0370:7334",
                "2001:0db8:85a3:0000:0000:8a2e:0370:7334" },
              { "IPv6: 2001:db8::1", "2001:db8::1" },
              { "IPv6:fe80::5054:ff:fe3a:7c01 up", "fe80::5054:ff:fe3a:7c01" },
              { "IPv6:   ::1", "::1" },
              { "IPv6: ::", "::" },
              { "IPv6: 2001:db8::", "2001:db8::" },
              { "IPv6: ::ffff:192.0.2.10", "::ffff:192.0.2.10" },
              { "IPv6: 64:ff9b::192.0.2.33", "64:ff9b::192.0.2.33" },
              { "IPv6: 2001:DB8:0:0:8:800:200C:417A", "2001:DB8:0:0:8:800:200C:417A" } } );
    }

    THEN( "the address after the text, up to a prefix length, zone or punctuation" )
    {
        requireFinds( "IPv6 address", { { "from 2001:db8::7 IPv6: 2001:db8::1", "2001:db8::1" },
                                        { "IPv6: 2001:db8::1/64", "2001:db8::1" },
                                        { "IPv6: fe80::1%eth0", "fe80::1" },
                                        { "IPv6: 2001:db8::1, gw 2001:db8::ff", "2001:db8::1" },
                                        { "IPv6: 2001:db8::1. Done", "2001:db8::1" },
                                        { "IPv6: 2001:db8::1: reachable", "2001:db8::1" } } );
    }

    THEN( "the text adjusted to a real log" )
    {
        // iproute2 6.1.0 (Debian 12), `ip -6 addr show`
        requireFinds(
            adjusted( "IPv6 address", "IPv6:", "inet6" ),
            { { "    inet6 fe80::5054:ff:fe3a:7c01/64 scope link", "fe80::5054:ff:fe3a:7c01" } } );
        // macOS 14 (Sonoma), `ifconfig lo0`
        requireFinds( adjusted( "IPv6 address", "IPv6:", "inet6" ),
                      { { "\tinet6 fe80::1%lo0 prefixlen 64 scopeid 0x1", "fe80::1" } } );
        // Windows 11 23H2 (10.0.22631), `ipconfig /all`
        requireFinds( adjusted( "IPv6 address", "IPv6:", "IPv6 Address. . . . . . . . . . . :" ),
                      { { "   IPv6 Address. . . . . . . . . . . : "
                          "2001:db8:4f2a:1200:9d3e:2b1c:aa10:5e(Preferred)",
                          "2001:db8:4f2a:1200:9d3e:2b1c:aa10:5e" } } );
        // OpenSSH 9.2p1 sshd (Debian 12), dual-stack listener
        requireFinds( adjusted( "IPv6 address", "IPv6:", "from" ),
                      { { "sshd[1234]: Accepted publickey for alice from ::ffff:192.0.2.10 port "
                          "52314 ssh2: ED25519 SHA256:abc",
                          "::ffff:192.0.2.10" } } );
    }

    THEN( "not an IPv4 address, a malformed address, or without the text" )
    {
        requireFindsNothing( "IPv6 address",
                             { "IPv6: 192.168.1.20", "IPv6: 2001:db8:::1", "IPv6: 2001::db8::1",
                               "IPv6: 1:2:3:4:5:6:7:8:9", "IPv6: 1:2:3:4:5:6:7",
                               "IPv6: 2001:db8::12345", "IPv6: 2001:db8::g1",
                               "IPv6: ::ffff:192.0.2", "IPv6: ::ffff:192.0.2.256", "IPv6: :1",
                               "IPv6: disabled", "IPv6:", "inet6 fe80::1", "ipv6: 2001:db8::1",
                               "IPv6 2001:db8::1", "IP: 2001:db8::1" } );
    }
}
