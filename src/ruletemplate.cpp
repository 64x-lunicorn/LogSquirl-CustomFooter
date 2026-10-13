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
        const QString h16 = QStringLiteral( "[0-9A-Fa-f]{1,4}" );
        const QString octet = QStringLiteral( "(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)" );
        const QString ls32 = QStringLiteral( "(?:%1:%1|(?:%2\\.){3}%2)" ).arg( h16, octet );
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
                        simple( QStringLiteral( "Linux version" ) ),
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
        SimpleRule rule;
        rule.textBefore = QLatin1Char( '[' ) + property + QStringLiteral( "]: [" );
        rule.valueEnd = ValueEnd::Character;
        rule.endCharacter = QStringLiteral( "]" );
        return rule;
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
    const auto upToComma = []( const QString& textBefore ) {
        auto rule = simple( textBefore, ValueEnd::Character );
        rule.endCharacter = QStringLiteral( "," );
        return rule;
    };

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
                        upToComma( QStringLiteral( "-- Python" ) ),
                        {} } );
    templates.append( { tr( "pytest version" ),
                        tr( "The pytest version in the pytest header, e.g. "
                            "platform linux -- Python 3.12.3, pytest-8.3.2" ),
                        QStringLiteral( "pytest" ),
                        upToComma( QStringLiteral( ", pytest-" ) ),
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
    appendGroup( templates, tr( "Linux and boot" ), linuxTemplates() );
    appendGroup( templates, tr( "Android" ), androidTemplates() );
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
