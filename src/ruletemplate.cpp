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

/// A number from 0 to 255 without leading zeros.
const QLatin1String kOctet( "(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)" );

QList<RuleTemplate> makeTemplates()
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

    // Four numbers from 0 to 255, not part of a word, a longer number or
    // dotted sequence: no word character or `.` before it, and no word
    // character, or `.` and one, after it; a dot ending a sentence may
    // follow.
    templates.append( { tr( "IPv4 address" ),
                        tr( "The first IPv4 address in a line, e.g. 192.168.1.20" ),
                        QStringLiteral( "IP address" ),
                        {},
                        QStringLiteral( "(?<![\\w.])(" ) + kOctet + QStringLiteral( "(?:\\." )
                            + kOctet + QStringLiteral( "){3})(?!\\w|\\.\\w)" ) } );

    // Date, `T` or a space as RFC 3339 allows, hours and minutes, optional
    // seconds (60 for a leap second) with a fraction after `.` or `,`, and
    // an optional `Z` or offset of `±HH`, `±HHMM` or `±HH:MM`; `t` and `z`
    // in lower case too. Not part of a longer number. The part after the
    // minutes is atomic and must not be followed by a digit or by `:` and
    // a digit, so a truncated field such as `10:30:5` or `+01:0` is no
    // match rather than cut back to `10:30`; a complete timestamp may be
    // followed by `:`, as in `10:30:00: started`.
    templates.append(
        { tr( "Timestamp (ISO 8601)" ),
          tr( "The first date and time in a line, e.g. 2024-01-15T10:30:00Z or "
              "2024-01-15 10:30:00.123" ),
          QStringLiteral( "Timestamp" ),
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
