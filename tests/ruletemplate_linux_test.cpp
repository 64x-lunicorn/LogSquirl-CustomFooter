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
 * @file ruletemplate_linux_test.cpp
 * @brief The Linux and boot templates: what each one finds in dmesg, boot
 * and syslog output.
 */

#include <catch2/catch.hpp>

#include "ruletemplate.h"
#include "simplerule.h"
#include "templatesamples.h"

using namespace custom_footer;

namespace {

const QString kGroup = QStringLiteral( "Linux and boot" );

} // namespace

SCENARIO( "The Linux and boot group holds its templates", "[ruletemplate]" )
{
    GIVEN( "the templates of the group" )
    {
        const auto names = namesOf( templatesIn( kGroup ) );

        THEN( "they are these, in this order" )
        {
            REQUIRE( names
                     == QStringList{ "Kernel version", "Hardware (DMI)", "Kernel command line",
                                     "U-Boot version", "Syslog host" } );
        }

        THEN( "each has its key" )
        {
            REQUIRE( templateNamed( "Kernel version" ).key == "Kernel" );
            REQUIRE( templateNamed( "Hardware (DMI)" ).key == "Hardware" );
            REQUIRE( templateNamed( "Kernel command line" ).key == "Command line" );
            REQUIRE( templateNamed( "U-Boot version" ).key == "U-Boot" );
            REQUIRE( templateNamed( "Syslog host" ).key == "Host" );
        }

        THEN( "the syslog host says it fits the log of one host" )
        {
            REQUIRE( templateNamed( "Syslog host" ).description.contains( "one host" ) );
        }
    }
}

SCENARIO( "Linux and boot templates are simple where the value follows a fixed text",
          "[ruletemplate]" )
{
    THEN( "kernel version and command line are simple rules" )
    {
        const auto& version = templateNamed( "Kernel version" );
        REQUIRE( version.isSimple() );
        REQUIRE( version.simple == SimpleRule{ "Linux version ", ValueEnd::Whitespace, {} } );

        const auto& commandLine = templateNamed( "Kernel command line" );
        REQUIRE( commandLine.isSimple() );
        REQUIRE( commandLine.simple == SimpleRule{ "Command line:", ValueEnd::EndOfLine, {} } );

        for ( const auto* name : { "Kernel version", "Kernel command line" } ) {
            INFO( name );
            const auto entry = templateNamed( name ).entry();
            REQUIRE( simpleRuleOf( entry ).has_value() );
        }
    }

    THEN( "hardware, U-Boot version and syslog host need a shape and are advanced" )
    {
        for ( const auto* name : { "Hardware (DMI)", "U-Boot version", "Syslog host" } ) {
            INFO( name );
            const auto& t = templateNamed( name );
            REQUIRE( !t.isSimple() );
            REQUIRE( !simpleRuleOf( t.entry() ).has_value() );
        }
    }
}

SCENARIO( "The kernel version template finds the version after Linux version", "[ruletemplate]" )
{
    THEN( "the word after Linux version, in dmesg and in the journal" )
    {
        requireFinds( "Kernel version",
                      { // dmesg, Ubuntu 24.04 (util-linux 2.39.3, kernel 6.8.0-45-generic)
                        { "[    0.000000] Linux version 6.8.0-45-generic (buildd@lcy02-amd64-075) "
                          "(x86_64-linux-gnu-gcc-13 (Ubuntu 13.2.0-23ubuntu4) 13.2.0, GNU ld (GNU "
                          "Binutils for Ubuntu) 2.42) #45-Ubuntu SMP PREEMPT_DYNAMIC Fri Aug 30 "
                          "12:02:04 UTC 2024 (Ubuntu 6.8.0-45.45-generic 6.8.12)",
                          "6.8.0-45-generic" },
                        // dmesg, Raspberry Pi OS 12 (kernel 6.6.31+rpt-rpi-v8)
                        { "[    0.000000] Linux version 6.6.31+rpt-rpi-v8 (serge@raspberrypi.com) "
                          "(gcc-12 (Debian 12.2.0-14) 12.2.0, GNU ld (GNU Binutils for Debian) "
                          "2.40) #1 SMP PREEMPT Debian 1:6.6.31-1+rpt1 (2024-05-29)",
                          "6.6.31+rpt-rpi-v8" },
                        // journalctl -k, Arch Linux (systemd 256, kernel 6.10.10-arch1-1)
                        { "Sep 16 09:41:07 archbox kernel: Linux version 6.10.10-arch1-1 "
                          "(linux@archlinux) (gcc (GCC) 14.2.1 20240910, GNU ld (GNU Binutils) "
                          "2.43.0) #1 SMP PREEMPT_DYNAMIC Thu, 12 Sep 2024 17:21:02 +0000",
                          "6.10.10-arch1-1" } } );
    }

    THEN( "not other kernel lines, another spelling, or no value" )
    {
        requireFindsNothing(
            "Kernel version",
            { // dmesg, Raspberry Pi OS 12 (kernel 6.6.31+rpt-rpi-v8)
              "[    0.000000] Booting Linux on physical CPU 0x0000000000 [0x410fd083]",
              // dmesg, Ubuntu 24.04 (kernel 6.8.0-45-generic)
              "[    0.000000] KERNEL supported cpus:", "linux version 6.8.0", "Linux 6.8.0",
              "Linux version" } );
    }
}

SCENARIO( "The hardware template finds vendor and product in the DMI line", "[ruletemplate]" )
{
    THEN( "the vendor, product and board before , BIOS" )
    {
        requireFinds( "Hardware (DMI)",
                      { // dmesg, Ubuntu 24.04 on a ThinkPad (kernel 6.8.0-45-generic)
                        { "[    0.000000] DMI: LENOVO 20XW0055GE/20XW0055GE, BIOS N32ET86W (1.62 ) "
                          "06/26/2023",
                          "LENOVO 20XW0055GE/20XW0055GE" },
                        // dmesg, QEMU 8.2 guest (kernel 6.8.0); the product has a comma
                        { "[    0.000000] DMI: QEMU Standard PC (i440FX + PIIX, 1996), BIOS "
                          "1.16.3-debian-1.16.3-2 04/01/2014",
                          "QEMU Standard PC (i440FX + PIIX, 1996)" },
                        // journalctl -k, Fedora 40 (systemd 255, kernel 6.10.9)
                        { "Sep 18 07:02:11 fedora kernel: DMI: Dell Inc. XPS 13 9310/0MRT12, BIOS "
                          "3.21.0 03/26/2024",
                          "Dell Inc. XPS 13 9310/0MRT12" } } );
    }

    THEN( "not a DMI line without a BIOS, or another line" )
    {
        requireFindsNothing( "Hardware (DMI)",
                             { // dmesg, QEMU guest without SMBIOS (kernel 6.8.0)
                               "[    0.000000] DMI: not present or invalid.",
                               // dmesg, Ubuntu 24.04 (kernel 6.8.0-45-generic)
                               "[    0.000000] DMI: Memory slots populated: 2/2",
                               "[    0.000000] SMBIOS 3.2.0 present.", "DMI: , BIOS 1.0",
                               "XDMI: A B, BIOS 1" } );
    }
}

SCENARIO( "The kernel command line template finds the rest of the line after Command line:",
          "[ruletemplate]" )
{
    THEN( "everything after Command line:, without trailing whitespace" )
    {
        requireFinds(
            "Kernel command line",
            { // dmesg, Ubuntu 24.04 (kernel 6.8.0-45-generic)
              { "[    0.000000] Command line: BOOT_IMAGE=/boot/vmlinuz-6.8.0-45-generic "
                "root=UUID=3f1b2c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d ro quiet splash vt.handoff=7  ",
                "BOOT_IMAGE=/boot/vmlinuz-6.8.0-45-generic "
                "root=UUID=3f1b2c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d ro quiet splash vt.handoff=7" },
              // journalctl -k, Arch Linux (systemd 256, kernel 6.10.10-arch1-1)
              { "Sep 16 09:41:07 archbox kernel: Command line: initrd=\\initramfs-linux.img "
                "root=PARTUUID=9b1c3a2e-01 rw",
                "initrd=\\initramfs-linux.img root=PARTUUID=9b1c3a2e-01 rw" } } );
    }

    THEN( "not Kernel command line: in lower case, or no value" )
    {
        requireFindsNothing(
            "Kernel command line",
            { // dmesg, Raspberry Pi OS 12 (kernel 6.6.31+rpt-rpi-v8)
              "[    0.000000] Kernel command line: coherent_pool=1M 8250.nr_uarts=1 "
              "console=ttyS0,115200 root=PARTUUID=a1b2c3d4-02 rootwait",
              "Command line:", "Command line:   ", "command line: ro quiet" } );
    }
}

SCENARIO( "The U-Boot version template finds the version in the U-Boot banner", "[ruletemplate]" )
{
    THEN( "the version between U-Boot and the build date in parentheses" )
    {
        requireFinds(
            "U-Boot version",
            { // serial console, BeagleBone Black (U-Boot 2019.04, Debian image)
              { "U-Boot 2019.04-00002-g07d5700e21 (Mar 06 2020 - 11:24:55 -0600), Build: "
                "jenkins-github_Bootloader-Builder-137",
                "2019.04-00002-g07d5700e21" },
              // serial console, Raspberry Pi 4 (U-Boot 2024.01, Debian package)
              { "U-Boot 2024.01+dfsg-5 (Mar 11 2024 - 18:45:23 +0000)", "2024.01+dfsg-5" },
              // serial console, old Allwinner board (U-Boot 2011.09)
              { "U-Boot 2011.09-rc1 (Nov 06 2013 - 18:44:23) Allwinner Technology",
                "2011.09-rc1" } } );
    }

    THEN( "not the SPL banner, other U-Boot lines, or prose" )
    {
        requireFindsNothing(
            "U-Boot version",
            { // serial console, BeagleBone Black (U-Boot 2019.04, Debian image)
              "U-Boot SPL 2019.04-00002-g07d5700e21 (Mar 06 2020 - 11:24:55 -0600)",
              "Hit any key to stop autoboot:  0", "updated to U-Boot 2024.01 today",
              "U-Boot version 2024.01 (Mar 11 2024)", "MyU-Boot 2024.01 (Mar 11 2024)" } );
    }
}

SCENARIO( "The syslog host template finds the host after the timestamp", "[ruletemplate]" )
{
    THEN( "the host after an RFC 3164 timestamp at the start of the line" )
    {
        requireFinds(
            "Syslog host",
            { // /var/log/syslog, Debian 12 (rsyslog 8.2302.0)
              { "Oct 13 08:12:01 webserver01 sshd[1234]: Accepted publickey for alice from "
                "10.0.0.7 port 52144 ssh2",
                "webserver01" },
              // RFC 3164, section 5.4, example 1, with its PRI
              { "<34>Oct 11 22:14:15 mymachine su: 'su root' failed for lonvick on /dev/pts/8",
                "mymachine" },
              // journalctl, Arch Linux (systemd 256); a day below 10 padded with a space
              { "Oct  3 09:41:07 archbox systemd[1]: Started Session 4 of User alice.", "archbox" },
              // journalctl -o short-precise, Fedora 40 (systemd 255)
              { "Sep 18 07:02:11.482915 fedora kernel: Linux version 6.10.9", "fedora" },
              // /var/log/messages, rsyslog 8.2310.0, host logged by address
              { "Jan  5 14:03:22 192.168.1.20 dhclient[812]: bound to 192.168.1.20",
                "192.168.1.20" },
              // the first line counts: the host of a log of one host
              { "Oct 13 08:12:01 webserver01 cron[99]: start\n"
                "Oct 13 08:12:02 webserver02 cron[42]: start",
                "webserver01" } } );
    }

    THEN( "not a line without a host, another timestamp, or a timestamp later in the line" )
    {
        requireFindsNothing(
            "Syslog host",
            { // macOS system.log (macOS 10.15): no host, the tag follows the time
              "Oct 13 08:12:01 kernel[0]: Previous shutdown cause: 5",
              // dmesg -T, util-linux 2.39.3
              "[Sun Oct 13 08:12:01 2024] usb 1-1: new high-speed USB device number 2",
              // journalctl -o short-iso, systemd 255
              "2024-10-13T08:12:01+0200 fedora systemd[1]: Started Session 4.",
              "received: Oct 13 08:12:01 webserver01 sshd[1234]: hi", "Oct 13 08:12 host x",
              "Foo 13 08:12:01 host x", "Oct 13 08:12:01 host" } );
    }
}
