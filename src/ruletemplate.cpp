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

#include "ruletemplate.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace custom_footer {

namespace {

QString tr( const char* text )
{
    return QCoreApplication::translate( "RuleTemplate", text );
}

SimpleRule simple( const QString& textBefore, ValueEnd valueEnd = ValueEnd::Whitespace )
{
    SimpleRule rule;
    rule.textBefore = textBefore;
    rule.valueEnd = valueEnd;
    return rule;
}

/// A simple rule whose value follows @p textBefore and ends before
/// @p endCharacter.
SimpleRule simpleUpTo( const QString& textBefore, const QString& endCharacter )
{
    auto rule = simple( textBefore, ValueEnd::Character );
    rule.endCharacter = endCharacter;
    return rule;
}

/// General: values most logs have, and `key=value` for any other.
QList<RuleTemplate> generalTemplates()
{
    QList<RuleTemplate> templates;

    // After `version` or `ver` in any case, or `Version` / `Ver` starting
    // a camel-case word (`appVersion`). Not after a letter or digit
    // otherwise, so `conversion` and `server` do not count, but after `_`
    // (`app_version`); `appversion` in lower case is missed for that. Then
    // an optional closing quote, `:`, `=`, `>` or `.`, opening quote and
    // `v`, for `{"version": "1.2.3"}`, `<version>1.2.3` or `Ver. 2.1`. Or
    // a `v` starting a word directly before a number with a dot (`v1.2.3`),
    // so that `v1` in prose is not taken.
    // The `version` of an XML declaration (`<?xml version="1.0"`) is the
    // XML version, not the app's, and is skipped.
    //
    // The value is the dotted number, not a date (`\d{4}-\d{2}`). A SemVer
    // pre-release or build suffix belongs to it only if its first
    // identifier has a letter (`-rc1`, `-beta.2`, `+build.5`), so a date
    // after the version (`1.2.3-2024-01-15`) is not taken; the `v` and the
    // quotes are not part of it.
    templates.append(
        { tr( "Version" ),
          tr( "A version number, e.g. version 1.2.3, Version: v2.0.1-rc1, v1.2" ),
          QStringLiteral( "Version" ),
          {},
          QStringLiteral(
              "(?:(?:(?<![A-Za-z0-9])(?<!<\\?xml\\s)(?i:version|ver)|(?<=[a-z0-9])(?:Version|Ver))"
              "[\"']?\\s*[:=>.]?\\s*[\"']?[vV]?|(?<![\\w.])[vV](?=\\d+\\.\\d))"
              "(?!\\d{4}-\\d{2})"
              "(\\d+(?:\\.\\d+)*"
              "(?:[-+][0-9A-Za-z]*[A-Za-z][0-9A-Za-z]*(?:\\.[0-9A-Za-z]+)*)*)" ) } );

    // Build numbers and serial numbers have no common shape: the word after
    // a text, which users adjust to their log.
    templates.append(
        { tr( "Build number" ),
          tr( "The word after Build:, e.g. Build: 1234; adjust the text to your log" ),
          QStringLiteral( "Build" ),
          simple( QStringLiteral( "Build:" ) ),
          {} } );
    templates.append(
        { tr( "Serial number" ),
          tr( "The word after Serial number:, e.g. Serial number: SN-0042; adjust the text" ),
          QStringLiteral( "Serial number" ),
          simple( QStringLiteral( "Serial number:" ) ),
          {} } );

    // The environment and the user a log was written in or by have no shape
    // either: the word after a text to adjust.
    templates.append(
        { tr( "Environment" ),
          tr( "The word after env:, e.g. env: prod or env: staging; adjust the text" ),
          QStringLiteral( "Environment" ),
          simple( QStringLiteral( "env:" ) ),
          {} } );
    templates.append( { tr( "User" ),
                        tr( "The word after user:, e.g. user: alice; adjust the text" ),
                        QStringLiteral( "User" ),
                        simple( QStringLiteral( "user:" ) ),
                        {} } );

    // A timestamp differs on almost every line; as the first match counts,
    // the first timestamp in the file is when the log starts.
    //
    // Date, `T` or a space as RFC 3339 allows, hours and minutes, optional
    // seconds (60 for a leap second) with a fraction after `.` or `,`, and
    // an optional `Z` or offset of `±HH`, `±HHMM` or `±HH:MM`; `t` and `z`
    // in lower case too. Not part of a longer number. The part after the
    // minutes is atomic and must not be followed by a digit or by `:` and
    // a digit, so a truncated field such as `10:30:5` or `+01:0` is no
    // match rather than cut back to `10:30`; a complete timestamp may be
    // followed by `:`, as in `10:30:00: started`.
    templates.append(
        { tr( "Log start" ),
          tr( "When the log starts: its first ISO 8601 date and time, e.g. "
              "2024-01-15T10:30:00Z or 2024-01-15 10:30:00.123" ),
          QStringLiteral( "Log start" ),
          {},
          QStringLiteral( "(?<!\\d)(\\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\\d|3[01])"
                          "[Tt ](?:[01]\\d|2[0-3]):[0-5]\\d"
                          "(?>(?::(?:[0-5]\\d|60)(?:[.,]\\d+)?)?"
                          "(?:[Zz]|[+-](?:[01]\\d|2[0-3])(?::?[0-5]\\d)?)?))(?!\\d|:\\d)" ) } );

    // The key the user gives, then `=` and the value up to whitespace, as
    // in logfmt. Advanced, as the key must not be the end of a longer one
    // (`superuser=`, `a.user=`, `my-user=`), and the value starts right
    // after `=`: `user= msg=hi` has no value for `user`. One or two dashes
    // starting a word may come first, for flags like `--user=alice`; the
    // lookbehind before them keeps out `my-user=`. For `a=1;b=2` or
    // `a=1, b=2`, edit the pattern.
    templates.append( { tr( "key=value" ),
                        tr( "The value after a key you give and =, up to the next whitespace" ),
                        {},
                        {},
                        QStringLiteral( "(?<![\\w.-])-{0,2}%1=(\\S+)" ) } );

    return templates;
}

/// Network: addresses.
QList<RuleTemplate> networkTemplates()
{
    QList<RuleTemplate> templates;

    // A log with clients and servers has many addresses, and the first one
    // in a line is an arbitrary one: the word after a text that says which
    // address is meant.
    templates.append(
        { tr( "IPv4 address" ),
          tr( "The address after IP:, e.g. IP: 192.168.1.20; adjust the text to your log" ),
          QStringLiteral( "IP address" ),
          simple( QStringLiteral( "IP:" ) ),
          {} } );

    // A MAC address after a text too, but advanced, as only its shape
    // tells it from the next word: six pairs of hex digits separated by `:`
    // or `-`, the same separator throughout (the backreference `\2`), or
    // three groups of four separated by `.`, as Cisco writes them; either
    // case. The pattern starts with the text, `MAC:`, matched literally,
    // and then optional whitespace, as a simple rule would; users adjust
    // it to their log (`link/ether`, `address is`). Not followed by a word
    // character, or by a separator and one, so a longer address, an EUI-64
    // or mixed separators are no match rather than cut short; a `,` or a
    // `.` ending a sentence may follow.
    templates.append(
        { tr( "MAC address" ),
          tr( "The address after MAC:, e.g. MAC: 52:54:00:12:34:56, 00-1A-2B-3C-4D-5E or "
              "0050.56a1.b2c3; adjust the text to your log" ),
          QStringLiteral( "MAC address" ),
          {},
          QStringLiteral( "MAC:\\s*("
                          "[0-9A-Fa-f]{2}([:-])[0-9A-Fa-f]{2}(?:\\2[0-9A-Fa-f]{2}){4}"
                          "|[0-9A-Fa-f]{4}\\.[0-9A-Fa-f]{4}\\.[0-9A-Fa-f]{4}"
                          ")(?!\\w|[:.-]\\w)" ) } );

    // An IPv6 address after a text to adjust, `IPv6:` at the start of the
    // pattern. Its shape is the IPv6address rule of RFC 3986, one
    // alternative per place of the `::`: eight groups of one to four hex
    // digits, fewer with one `::` standing for the groups left out, and
    // the last two groups may be an IPv4 address (`::ffff:192.0.2.10`,
    // `64:ff9b::192.0.2.33`). Not followed by a word character, by `:`
    // and another `:` or a word character, or by `.` and a word character,
    // so a ninth group, a second `::` or a truncated IPv4 address is no
    // match rather than cut short. A prefix length (`/64`), a zone
    // (`%eth0`) or a `:` ending a phrase is left out of the value.
    {
        // The names are those of the RFC's rules: a group of hex digits,
        // a number from 0 to 255 without leading zeros, and the last 32
        // bits, two groups or an IPv4 address.
        const QString h16 = QStringLiteral( "[0-9A-Fa-f]{1,4}" );
        const QString decOctet = QStringLiteral( "(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)" );
        const QString ls32 = QStringLiteral( "(?:%1:%1|(?:%2\\.){3}%2)" ).arg( h16, decOctet );
        const QString address = QStringLiteral( "(?:(?:%1:){6}%2"
                                                "|::(?:%1:){5}%2"
                                                "|(?:%1)?::(?:%1:){4}%2"
                                                "|(?:(?:%1:)?%1)?::(?:%1:){3}%2"
                                                "|(?:(?:%1:){0,2}%1)?::(?:%1:){2}%2"
                                                "|(?:(?:%1:){0,3}%1)?::%1:%2"
                                                "|(?:(?:%1:){0,4}%1)?::%2"
                                                "|(?:(?:%1:){0,5}%1)?::%1"
                                                "|(?:(?:%1:){0,6}%1)?::)" )
                                    .arg( h16, ls32 );
        templates.append(
            { tr( "IPv6 address" ),
              tr( "The address after IPv6:, e.g. IPv6: 2001:db8::1 or ::ffff:192.0.2.10; "
                  "adjust the text to your log" ),
              QStringLiteral( "IPv6 address" ),
              {},
              QStringLiteral( "IPv6:\\s*(%1)(?!\\w|:[\\w:]|\\.\\w)" ).arg( address ) } );
    }

    return templates;
}

/// Linux and boot: values of dmesg, boot and syslog output.
QList<RuleTemplate> linuxTemplates()
{
    QList<RuleTemplate> templates;

    // The kernel names its release once, as the first line of dmesg:
    // `Linux version 6.8.0-45-generic (buildd@…) (gcc …) #45-Ubuntu …`. The
    // release is the word after the text.
    templates.append( { tr( "Kernel version" ),
                        tr( "The kernel release after Linux version, e.g. Linux version "
                            "6.8.0-45-generic" ),
                        QStringLiteral( "Kernel" ),
                        simple( QStringLiteral( "Linux version " ) ),
                        {} } );

    // The DMI line, `DMI: <vendor> <product>/<board>, BIOS <version> <date>`
    // on x86, names the hardware. The value is what comes before `, BIOS`,
    // which a simple rule cannot end at; the product itself may hold a comma
    // (`QEMU Standard PC (i440FX + PIIX, 1996)`), so it is the shortest text
    // up to `, BIOS `. Without `, BIOS` there is no match, so the lines
    // `DMI: not present or invalid.` and `DMI: Memory slots populated: 2/2`
    // are not taken for the hardware. `DMI:` must not end a longer word.
    templates.append( { tr( "Hardware (DMI)" ),
                        tr( "Vendor and product before , BIOS in the DMI: line, e.g. "
                            "LENOVO 20XW0055GE/20XW0055GE" ),
                        QStringLiteral( "Hardware" ),
                        {},
                        QStringLiteral( "(?<![\\w-])DMI:\\s*(\\S.*?), BIOS " ) } );

    // The kernel command line, logged once at boot, is everything after
    // `Command line:`, spaces included. On x86 it comes first; ARM kernels
    // write only `Kernel command line:`, with a lower-case `c`, which the
    // text as written does not match.
    templates.append( { tr( "Kernel command line" ),
                        tr( "The rest of the line after Command line:, e.g. "
                            "BOOT_IMAGE=/boot/vmlinuz root=UUID=… ro quiet" ),
                        QStringLiteral( "Command line" ),
                        simple( QStringLiteral( "Command line:" ), ValueEnd::EndOfLine ),
                        {} } );

    // The U-Boot banner, `U-Boot 2024.01 (Mar 11 2024 - 18:45:23 +0000)`:
    // the version is the word between `U-Boot ` and the build date in
    // parentheses, and starts with a dotted number. The SPL and TPL banners
    // (`U-Boot SPL 2024.01 (…)`) are of the stages before U-Boot proper and
    // are skipped, as is `U-Boot` in other lines or ending a longer word.
    templates.append( { tr( "U-Boot version" ),
                        tr( "The version in the U-Boot banner, e.g. U-Boot 2024.01 "
                            "(Mar 11 2024 - 18:45:23 +0000)" ),
                        QStringLiteral( "U-Boot" ),
                        {},
                        QStringLiteral( "(?<![\\w-])U-Boot (\\d+\\.\\d+\\S*) \\(" ) } );

    // An RFC 3164 syslog line starts with an optional `<PRI>`, the time as
    // `Mmm dd hh:mm:ss` (the day padded with a space, as `Oct  3`, and a
    // fraction as journalctl's short-precise writes it), and then the host,
    // followed by a space. The host has no text before it, only its place:
    // anchored at the start of the line, so a timestamp later in a line is
    // not taken. A word ending in `:` there is the tag of a line without a
    // host (`kernel[0]:`), not a host. The first line counts, so this fits a
    // log of one host, not one merged from several.
    templates.append(
        { tr( "Syslog host" ),
          tr( "The host after the time of an RFC 3164 syslog line, e.g. Oct 13 08:12:01 "
              "webserver01 sshd[1234]: …; fits the log of one host, not merged ones" ),
          QStringLiteral( "Host" ),
          {},
          QStringLiteral( "^(?:<\\d{1,3}>)?(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)"
                          " [ \\d]\\d \\d{2}:\\d{2}:\\d{2}(?:\\.\\d+)? (\\S+)(?<!:) " ) } );

    return templates;
}

/// Android: logcat output and bug reports.
QList<RuleTemplate> androidTemplates()
{
    QList<RuleTemplate> templates;

    // The build fingerprint names one build of one device, and nothing else
    // has its shape, so it is found by shape alone: in a tombstone or a
    // crash in logcat (`Build fingerprint: '…'`), the header of a bug
    // report, or getprop. `brand/product/device:version/id/incremental`,
    // each part a word of letters, digits, `_`, `.` or `-`, then `:`, the
    // build type `user`, `userdebug` or `eng`, `/` and the tags, separated
    // by `,` (`release-keys`). Not inside a path or a longer word, and the
    // tags not cut short.
    templates.append(
        { tr( "Build fingerprint" ),
          tr( "The build fingerprint, e.g. "
              "google/sunfish/sunfish:11/RQ3A.210805.001.A1/7474174:user/release-keys" ),
          QStringLiteral( "Build fingerprint" ),
          {},
          QStringLiteral( "(?<![\\w./:-])"
                          "([\\w.-]+/[\\w.-]+/[\\w.-]+:[\\w.-]+/[\\w.-]+/[\\w.-]+"
                          ":(?:user|userdebug|eng)/[\\w-]+(?:,[\\w-]+)*)"
                          "(?![\\w,/-])" ) } );

    // getprop prints each property as `[key]: [value]`, in a bug report
    // too. The value is what is in the brackets, spaces included
    // (`[Pixel 7]`); the `[` before the key keeps out longer keys that end
    // the same (`ro.product.vendor.model`).
    const auto getprop = []( const QString& property ) {
        return simpleUpTo( QLatin1Char( '[' ) + property + QStringLiteral( "]: [" ),
                           QStringLiteral( "]" ) );
    };
    templates.append( { tr( "Device model" ),
                        tr( "The model from getprop, e.g. [ro.product.model]: [Pixel 7]" ),
                        QStringLiteral( "Device model" ),
                        getprop( QStringLiteral( "ro.product.model" ) ),
                        {} } );
    templates.append(
        { tr( "Android version" ),
          tr( "The Android version from getprop, e.g. [ro.build.version.release]: [14]" ),
          QStringLiteral( "Android version" ),
          getprop( QStringLiteral( "ro.build.version.release" ) ),
          {} } );
    // Named apart from General's Serial number, as the dialog lists
    // templates by name; the key is the same, as it is the same value.
    templates.append( { tr( "Device serial number" ),
                        tr( "The serial number from getprop, e.g. [ro.serialno]: [1A2B3C4D5E6F]" ),
                        QStringLiteral( "Serial number" ),
                        getprop( QStringLiteral( "ro.serialno" ) ),
                        {} } );

    return templates;
}

/// Test run: the header and the summary of a pytest run.
QList<RuleTemplate> testRunTemplates()
{
    QList<RuleTemplate> templates;

    // pytest starts a run with `platform linux -- Python 3.12.3,
    // pytest-8.3.2, pluggy-1.5.0`, once per run. The versions follow a fixed
    // text and end at the comma, or at the end of the line if none follows.
    // `-- Python` and `, pytest-` with the text around them keep out a
    // lower-case interpreter path (`-- /usr/bin/python3`) and temporary
    // paths such as `/tmp/pytest-of-user/pytest-3`. On PyPy the PyPy
    // version is part of the value (`3.9.10[pypy-7.3.8-final]`).
    templates.append( { tr( "Python version" ),
                        tr( "The Python version in the pytest header, e.g. "
                            "platform linux -- Python 3.12.3, pytest-8.3.2" ),
                        QStringLiteral( "Python" ),
                        simpleUpTo( QStringLiteral( "-- Python " ), QStringLiteral( "," ) ),
                        {} } );
    templates.append( { tr( "pytest version" ),
                        tr( "The pytest version in the pytest header, e.g. "
                            "platform linux -- Python 3.12.3, pytest-8.3.2" ),
                        QStringLiteral( "pytest" ),
                        simpleUpTo( QStringLiteral( ", pytest-" ), QStringLiteral( "," ) ),
                        {} } );

    // The summary pytest ends a run with, once per run: counts such as
    // `3 failed, 120 passed` or `no tests ran`, `in`, and the time in
    // seconds (`12.30s`, before pytest 5.1 `12.30 seconds`), with the
    // duration after a run longer than a minute (`336.42s (0:05:36)`).
    // The count words are lower case, one or two (`5 subtests passed`
    // from pytest-subtests). The `=` around it, which `-q` leaves out,
    // and spaces at the ends are not part of the value. It has no text
    // before it, so the whole line must have this shape; other separators
    // (`= FAILURES =`) and unittest's `Ran 120 tests in 12.300s` do not.
    templates.append(
        { tr( "Test result" ),
          tr( "The summary at the end of a pytest run, e.g. 3 failed, 120 passed in 12.3s; "
              "meant for one run per file" ),
          QStringLiteral( "Test result" ),
          {},
          QStringLiteral( "^=*\\s*("
                          "(?:\\d+ [a-z]+(?: [a-z]+)?(?:, \\d+ [a-z]+(?: [a-z]+)?)*|no tests ran)"
                          " in \\d+(?:\\.\\d+)?(?:s| seconds)(?: \\(\\d+:\\d{2}:\\d{2}\\))?"
                          ")\\s*=*\\s*$" ) } );

    return templates;
}

/// CI: build logs of GitHub Actions, GitLab and Jenkins.
QList<RuleTemplate> ciTemplates()
{
    QList<RuleTemplate> templates;

    // GitHub Actions names the runner image in the "Runner Image" section of
    // "Set up job", the first step of every job log: `Image: ubuntu-24.04`.
    // Self-hosted runners print no such section, so there a later `Image:`
    // may be found instead.
    templates.append(
        { tr( "Runner image" ),
          tr( "The GitHub Actions runner image after Image:, e.g. Image: ubuntu-24.04" ),
          QStringLiteral( "Runner image" ),
          simple( QStringLiteral( "Image:" ) ),
          {} } );

    // The image's version is the `Version:` right after `Image:`. It is
    // advanced because the "Runner Image Provisioner" section before it has
    // a `Version:` too (`Version: 20261002.596`): the image's version has
    // three parts, a date and two numbers (`20261004.327.1`), the
    // provisioner's two. Not after a letter, digit or `_`, so `UCRTVersion:`
    // does not count, and not followed by a word character or `.`.
    templates.append(
        { tr( "Runner image version" ),
          tr( "The GitHub Actions runner image version after Version:, e.g. "
              "Version: 20261004.327.1" ),
          QStringLiteral( "Runner image version" ),
          {},
          QStringLiteral( "(?<!\\w)Version:\\s*(\\d{8}\\.\\d+\\.\\d+)(?![\\w.])" ) } );

    // Git prints the abbreviated commit it checked out after `HEAD is now
    // at`, as actions/checkout does for a pull request. A log with several
    // checkouts has the line more than once; the first one counts.
    templates.append( { tr( "Commit" ),
                        tr( "The commit after HEAD is now at, e.g. HEAD is now at aa5a02d; "
                            "the first checkout in the log counts" ),
                        QStringLiteral( "Commit" ),
                        simple( QStringLiteral( "HEAD is now at " ) ),
                        {} } );

    // The first line of a GitLab job log: `Running with gitlab-runner
    // 16.2.0 (782e15da)`. The word after it is the version.
    templates.append(
        { tr( "GitLab Runner version" ),
          tr( "The version after Running with gitlab-runner, e.g. Running with gitlab-runner "
              "16.2.0" ),
          QStringLiteral( "GitLab Runner version" ),
          simple( QStringLiteral( "Running with gitlab-runner " ) ),
          {} } );

    // Jenkins starts a build's console output with its cause. For a build
    // started by hand that is `Started by user` and the user's full name,
    // which may have spaces, so the value ends at the end of the line.
    // `Started by timer` or `Started by an SCM change` give no value.
    templates.append(
        { tr( "Started by" ),
          tr( "The Jenkins user after Started by user, e.g. Started by user Jane Doe" ),
          QStringLiteral( "Started by" ),
          simple( QStringLiteral( "Started by user " ), ValueEnd::EndOfLine ),
          {} } );

    return templates;
}

/// iOS crash log: the header of an Apple crash report, which names the
/// device, the system and the app once.
QList<RuleTemplate> iosCrashTemplates()
{
    QList<RuleTemplate> templates;

    // The header fields have their names at the start of the line, padded
    // with spaces to a column, and appear once per report.
    //
    // The model identifier, such as `iPhone15,5` or `Mac14,6`, is one word.
    templates.append( { tr( "Hardware model" ),
                        tr( "The device after Hardware Model:, e.g. Hardware Model: iPhone15,5" ),
                        QStringLiteral( "Hardware model" ),
                        simple( QStringLiteral( "Hardware Model:" ) ),
                        {} } );

    // The system name, version and build, such as `iPhone OS 17.2 (21C62)`
    // or `macOS 14.6.1 (23G93)`, has spaces: up to the end of the line.
    templates.append(
        { tr( "OS version" ),
          tr( "The system after OS Version:, e.g. OS Version: iPhone OS 17.2 (21C62)" ),
          QStringLiteral( "OS version" ),
          simple( QStringLiteral( "OS Version:" ), ValueEnd::EndOfLine ),
          {} } );

    // A UUID unique to the report; one word.
    templates.append( { tr( "Incident identifier" ),
                        tr( "The report's UUID after Incident Identifier:" ),
                        QStringLiteral( "Incident identifier" ),
                        simple( QStringLiteral( "Incident Identifier:" ) ),
                        {} } );

    // The app's CFBundleVersion and, in parentheses, its
    // CFBundleShortVersionString (`1 (3.0)`), so up to the end of the line.
    // Advanced, as `Version:` also ends `OS Version:`, `Baseband Version:`
    // and `Report Version:`, which follow it in the header: a simple rule
    // would take the OS version where the app's `Version:` is empty. So
    // `Version:` must not follow a word and a space; it may start the line
    // or follow other text, such as a prefix a log adds (`crash: Version:`),
    // but not directly a letter (`AppVersion:`).
    templates.append(
        { tr( "App version" ),
          tr( "The app's version after Version:, e.g. Version: 1.0 (1); not OS Version:" ),
          QStringLiteral( "App version" ),
          {},
          QStringLiteral( "(?<!\\S|\\w\\s)Version:\\s*(.*\\S)" ) } );

    return templates;
}

/// Java and Spring Boot: the Spring Boot start line, the active profiles,
/// and the JRE of a JVM crash log.
QList<RuleTemplate> javaTemplates()
{
    QList<RuleTemplate> templates;

    // Spring Boot logs one start line per run, e.g. `Starting MyApp v1.2.0
    // using Java 17.0.8 with PID 4242 (...)`. Spring Boot 3 and later word
    // it so; Spring Boot 2.4 to 2.7 add `on <host>` before `with PID`, and
    // 2.0 to 2.3 have no `using Java`. A native image says `Starting
    // AOT-processed MyApp`. The application, its version, the Java version
    // and the host are single words in it. The application and its version
    // are only taken from a `Starting` that ends in this shape, up to `with
    // PID` and a digit, so `Starting service [Tomcat]` and `Starting Servlet
    // engine:` in the same log are not. startHead is the line up to the
    // application, startTail the part after its version.
    const QString startHead = QStringLiteral( "(?<!\\S)Starting (?:AOT-processed )?" );
    const QString startTail = QStringLiteral( "(?:using Java \\S+ )?(?:on \\S+ )?with PID \\d" );
    templates.append(
        { tr( "Application" ),
          tr( "The application of the Spring Boot start line, e.g. Starting MyApp v1.2.0 using "
              "Java 17 with PID 42" ),
          QStringLiteral( "Application" ),
          {},
          startHead + QStringLiteral( "(\\S+) (?=(?:v\\S+ )?" ) + startTail
              + QStringLiteral( ")" ) } );
    templates.append(
        { tr( "Application version" ),
          tr( "The version after v in the Spring Boot start line, e.g. Starting MyApp v1.2.0" ),
          QStringLiteral( "Application version" ),
          {},
          startHead + QStringLiteral( "\\S+ v(\\S+) (?=" ) + startTail + QStringLiteral( ")" ) } );

    // The Java version follows a fixed text, also in Spring Boot 3, which
    // has no host.
    templates.append(
        { tr( "Java version" ),
          tr( "The word after using Java in the Spring Boot start line, e.g. using Java 17.0.8" ),
          QStringLiteral( "Java version" ),
          simple( QStringLiteral( "using Java " ) ),
          {} } );

    // The host is between `on` and `with PID` of the start line of Spring
    // Boot 2; Spring Boot 3 no longer logs it.
    templates.append(
        { tr( "Host" ),
          tr( "The host of the Spring Boot 2 start line, e.g. Starting MyApp on myhost with PID "
              "42" ),
          QStringLiteral( "Host" ),
          {},
          startHead
              + QStringLiteral( "\\S+ (?:v\\S+ )?(?:using Java \\S+ )?on (\\S+) with PID \\d" ) } );

    // Since Spring Boot 2.7: `The following 1 profile is active: "prod"` or
    // `The following 2 profiles are active: "dev", "local"`; before that
    // `The following profiles are active: dev,local`. A single quoted
    // profile is taken without its quotes (the branch reset `(?|...)` makes
    // both alternatives group 1), anything else up to the end of the line
    // as listed. `No active profile set, falling back to ... default
    // profile` is not an active profile.
    templates.append(
        { tr( "Active profile" ),
          tr( "The active Spring profiles, e.g. The following 1 profile is active: \"prod\"" ),
          QStringLiteral( "Active profile" ),
          {},
          QStringLiteral( "The following (?:\\d+ )?profiles? (?:is|are) active: "
                          "(?|\"([^\"]*)\"\\s*$|(.*\\S))" ) } );

    // A JVM crash log (hs_err_pid*.log) has `# JRE version: <runtime name>
    // (<version>) (build <runtime version>)`, e.g. `(build 17.0.8+7)` or
    // `(build 1.8.0_201-b09)`; a debug JVM writes `(fastdebug build ...)`.
    // The value is the runtime version, as `java -version` shows it. A JVM
    // crashing while it starts leaves it empty, `(build )`, and the template
    // finds nothing.
    templates.append(
        { tr( "JRE version" ),
          tr( "The JRE build in a JVM crash log, e.g. # JRE version: ... (build 17.0.8+7)" ),
          QStringLiteral( "JRE version" ),
          {},
          QStringLiteral( "JRE version:.*\\((?:\\S+ )?build ([^\\s)]+)\\)" ) } );

    return templates;
}

/**
 * The shape of a VIN (ISO 3779), without a capturing group: 17 characters
 * from `A-Z` and `0-9` without `I`, `O` and `Q`, at least one of them a
 * letter and one a digit, not after or before another letter or digit.
 * `_` and punctuation may border it, as in `vin_WVWZZZ1JZXW000001`. No
 * check digit test, as only North American VINs have one. The letter and
 * digit lookaheads may look past the 17 characters, but then the closing
 * lookahead fails anyway.
 *
 * A template taking part of a VIN puts the shape in a lookahead and
 * captures the part, e.g. `(?=` kVin `)([A-HJ-NPR-Z0-9]{3})` for the first
 * three characters.
 */
const QString kVin = QStringLiteral( "(?<![A-Za-z0-9])"
                                     "(?=[A-HJ-NPR-Z0-9]{0,16}[A-HJ-NPR-Z])"
                                     "(?=[A-HJ-NPR-Z0-9]{0,16}[0-9])"
                                     "[A-HJ-NPR-Z0-9]{17}(?![A-Za-z0-9])" );

/// A manufacturer and its world manufacturer identifiers (WMI), the first
/// three characters of a VIN, separated by spaces.
struct Manufacturer {
    const char* name;
    const char* wmis;
};

/**
 * Common WMIs of major manufacturers, as listed in Wikibooks' "Vehicle
 * Identification Numbers (VIN codes)/World Manufacturer Identifier (WMI)".
 * Only WMIs of one brand; a WMI shared by several brands, such as
 * Chrysler Group's `1C4`, is left out and shows raw. The names are brand
 * names and not translated.
 */
const Manufacturer kManufacturers[] = {
    { "Volkswagen", "WVW WV1 WV2 1VW 3VW" },
    { "Audi", "WAU WUA TRU" },
    { "Porsche", "WP0 WP1" },
    { "Škoda", "TMB" },
    { "SEAT", "VSS" },
    { "BMW", "WBA WBS WBY" },
    { "MINI", "WMW" },
    { "Mercedes-Benz", "WDB WDD W1K W1N W1V" },
    { "Opel/Vauxhall", "W0L" },
    { "Ford", "1FA 1FM 1FT 3FA WF0" },
    { "Chevrolet", "1G1 2G1 1GC 1GN" },
    { "GMC", "1GT" },
    { "Cadillac", "1G6 1GY" },
    { "Tesla", "5YJ 7SA LRW" },
    { "Toyota", "JTD JTE JT2 2T1 4T1 5TD" },
    { "Lexus", "JTH" },
    { "Honda", "JHM 1HG 2HG 5J6 5FN" },
    { "Nissan", "JN1 JN8 1N4 5N1" },
    { "Mazda", "JM1" },
    { "Subaru", "JF1 4S3 4S4" },
    { "Hyundai", "KMH 5NP" },
    { "Kia", "KNA KND" },
    { "Volvo", "YV1 YV4" },
    { "Fiat", "ZFA" },
    { "Alfa Romeo", "ZAR" },
    { "Ferrari", "ZFF" },
    { "Renault", "VF1" },
    { "Peugeot", "VF3" },
    { "Citroën", "VF7" },
    { "Land Rover", "SAL" },
    { "Jaguar", "SAJ" },
    { "Bentley", "SCB" },
};

/// Each WMI of kManufacturers mapped to its manufacturer.
QList<ValueMapping> manufacturerMappings()
{
    QList<ValueMapping> mappings;
    for ( const auto& manufacturer : kManufacturers ) {
        const auto wmis = QString::fromLatin1( manufacturer.wmis ).split( QLatin1Char( ' ' ) );
        for ( const auto& wmi : wmis ) {
            mappings.append( { wmi, QString::fromUtf8( manufacturer.name ) } );
        }
    }
    return mappings;
}

/// The first year of the model year cycle the mappings cover.
constexpr int kFirstModelYear = 2010;

/// The model year codes of a VIN's 10th character, one per year from
/// kFirstModelYear: letters without I, O, Q, U and Z, then digits without
/// 0. They repeat every 30 years.
const char kModelYearCodes[] = "ABCDEFGHJKLMNPRSTVWXY123456789";

/// The years after which the model year codes repeat.
constexpr int kModelYearCycle = 30;
static_assert( sizeof( kModelYearCodes ) - 1 == kModelYearCycle, "one code per year" );

/// Each model year code mapped to its year in the cycle from
/// kFirstModelYear.
QList<ValueMapping> modelYearMappings()
{
    QList<ValueMapping> mappings;
    const auto codes = QString::fromLatin1( kModelYearCodes );
    for ( int i = 0; i < codes.size(); ++i ) {
        mappings.append( { codes.at( i ), QString::number( kFirstModelYear + i ) } );
    }
    return mappings;
}

/// Automotive: the vehicle and its telematics unit.
QList<RuleTemplate> automotiveTemplates()
{
    QList<RuleTemplate> templates;

    // A VIN identifies one vehicle and is unique by nature, so it is found
    // by its shape alone, with or without text before it: kVin as the
    // value.
    templates.append( { tr( "VIN" ),
                        tr( "A vehicle identification number by its shape: 17 letters and "
                            "digits without I, O and Q, e.g. WVWZZZ1JZXW000001" ),
                        QStringLiteral( "VIN" ),
                        {},
                        QStringLiteral( "(%1)" ).arg( kVin ) } );

    // The IMEI of the modem and the ICCID of the SIM card are 15 and 18 to
    // 22 digits, a shape far too many other numbers have: the word after a
    // text that says which number is meant, to adjust to the log.
    templates.append(
        { tr( "IMEI" ),
          tr( "The word after IMEI:, e.g. IMEI: 490154203237518; adjust the text to your log" ),
          QStringLiteral( "IMEI" ),
          simple( QStringLiteral( "IMEI:" ) ),
          {} } );
    templates.append(
        { tr( "ICCID" ),
          tr( "The word after ICCID:, e.g. ICCID: 89014103211118510720; adjust the text" ),
          QStringLiteral( "ICCID" ),
          simple( QStringLiteral( "ICCID:" ) ),
          {} } );

    // The manufacturer and the model year are parts of a VIN: kVin in a
    // lookahead, so they are found in exactly the VINs the VIN template
    // finds, then the part. The WMI is the first three characters, the
    // model year code the 10th. Mappings turn the codes into text; an
    // unmapped code shows as it is.
    RuleTemplate manufacturer{ tr( "Manufacturer" ),
                               tr( "The manufacturer from the first three characters of a VIN "
                                   "(the WMI), e.g. WVW: Volkswagen; mapped for common WMIs, "
                                   "another shows as it is" ),
                               QStringLiteral( "Manufacturer" ),
                               {},
                               QStringLiteral( "(?=%1)([A-HJ-NPR-Z0-9]{3})" ).arg( kVin ) };
    manufacturer.mappings = manufacturerMappings();
    templates.append( manufacturer );

    RuleTemplate modelYear{
        tr( "Model year" ),
        tr( "The model year from the 10th character of a VIN, e.g. A: 2010; the codes repeat "
            "every %1 years, and the mappings give the years %2 to %3" )
            .arg( kModelYearCycle )
            .arg( kFirstModelYear )
            .arg( kFirstModelYear + kModelYearCycle - 1 ),
        QStringLiteral( "Model year" ),
        {},
        QStringLiteral( "(?=%1)[A-HJ-NPR-Z0-9]{9}([A-HJ-NPR-Z0-9])" ).arg( kVin )
    };
    modelYear.mappings = modelYearMappings();
    templates.append( modelYear );

    return templates;
}

/// Database: the server version from the line a database server logs when
/// it starts.
QList<RuleTemplate> databaseTemplates()
{
    QList<RuleTemplate> templates;

    // PostgreSQL 10 and later log `starting PostgreSQL 16.1 (Debian
    // 16.1-1.pgdg120+1) on x86_64-pc-linux-gnu, ...` once per start; the
    // version is the word after it, also a beta's (`17beta1`). The lower
    // case `starting` keeps out the `Starting PostgreSQL 13 database
    // server` of Debian's init script, which names the cluster's major
    // version; its `Restarting PostgreSQL 13 database server` is taken, as
    // a simple text may end a longer word, and gives that major version,
    // still the server's. With several starts in the file, such as the Docker image's
    // init server before the real one, the first counts.
    templates.append(
        { tr( "PostgreSQL version" ),
          tr( "The version after starting PostgreSQL, e.g. starting PostgreSQL 16.1 on ..." ),
          QStringLiteral( "PostgreSQL version" ),
          simple( QStringLiteral( "starting PostgreSQL " ) ),
          {} } );

    // MySQL logs `ready for connections. Version: '8.0.36'  socket: ...`
    // once per start; MySQL 5.7 and MariaDB put `Version: '...'` on a line
    // of its own. The version is between the quotes, so MariaDB's suffix
    // (`11.2.2-MariaDB-1:11.2.2+maria~ubu2204`) is part of it. The quote
    // keeps out an unquoted `Version:`, as in other logs.
    templates.append( { tr( "MySQL version" ),
                        tr( "The quoted version after Version: in the MySQL or MariaDB ready "
                            "line, e.g. Version: '8.0.36'" ),
                        QStringLiteral( "MySQL version" ),
                        simpleUpTo( QStringLiteral( "Version: '" ), QStringLiteral( "'" ) ),
                        {} } );

    // Redis 4 and later log `Redis version=7.2.4, bits=64, commit=...`
    // once per start; the version ends at the comma.
    templates.append( { tr( "Redis version" ),
                        tr( "The version after Redis version=, e.g. Redis version=7.2.4, bits=64" ),
                        QStringLiteral( "Redis version" ),
                        simpleUpTo( QStringLiteral( "Redis version=" ), QStringLiteral( "," ) ),
                        {} } );

    return templates;
}

/// Appends the templates of @p group to @p templates, named @p name.
void appendGroup( QList<RuleTemplate>& templates, const QString& name, QList<RuleTemplate> group )
{
    for ( auto& ruleTemplate : group ) {
        ruleTemplate.group = name;
        templates.append( ruleTemplate );
    }
}

QList<RuleTemplate> makeTemplates()
{
    QList<RuleTemplate> templates;

    // One function per group, in the order the dialog shows the groups. A
    // new group is a function above and a line here.
    appendGroup( templates, tr( "General" ), generalTemplates() );
    appendGroup( templates, tr( "Network" ), networkTemplates() );
    appendGroup( templates, tr( "Automotive" ), automotiveTemplates() );
    appendGroup( templates, tr( "Android" ), androidTemplates() );
    appendGroup( templates, tr( "Linux and boot" ), linuxTemplates() );
    appendGroup( templates, tr( "iOS crash log" ), iosCrashTemplates() );
    appendGroup( templates, tr( "Java and Spring Boot" ), javaTemplates() );
    appendGroup( templates, tr( "Database" ), databaseTemplates() );
    appendGroup( templates, tr( "CI" ), ciTemplates() );
    appendGroup( templates, tr( "Test run" ), testRunTemplates() );

    return templates;
}

} // namespace

FooterEntry RuleTemplate::entry( const QString& askedKey ) const
{
    FooterEntry entry;
    entry.key = asksForKey() ? givenKey( askedKey ) : key;
    if ( entry.key.isEmpty() ) {
        return {};
    }
    if ( isSimple() ) {
        applySimpleRule( simple, entry );
    }
    else if ( asksForKey() ) {
        entry.linePattern = linePattern.arg( QRegularExpression::escape( entry.key ) );
    }
    else {
        entry.linePattern = linePattern;
    }
    entry.mappings = mappings;
    return entry;
}

QString RuleTemplate::givenKey( const QString& askedKey )
{
    auto key = askedKey.trimmed();
    if ( key.endsWith( QLatin1Char( '=' ) ) ) {
        key.chop( 1 );
    }
    return key.trimmed();
}

const QList<RuleTemplate>& ruleTemplates()
{
    static const QList<RuleTemplate> templates = makeTemplates();
    return templates;
}

} // namespace custom_footer
