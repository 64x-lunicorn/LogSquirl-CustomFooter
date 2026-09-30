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

    // After `version` or `ver`, in any case, with an optional `:` or `=`
    // and `v`: any number of dotted parts, `version 3` included. Or a `v`
    // starting a word, directly before a number with at least one dot, so
    // that `v1` in prose is not taken. A SemVer pre-release or build suffix
    // (`-rc1`, `+build.5`) belongs to the value; the `v` does not.
    templates.append(
        { tr( "Version" ),
          tr( "A version number, e.g. version 1.2.3, Version: v2.0.1-rc1, v1.2" ),
          QStringLiteral( "Version" ),
          {},
          QStringLiteral( "(?i)(?:\\b(?:version|ver)\\s*[:=]?\\s*v?|\\bv(?=\\d+\\.\\d))"
                          "(\\d+(?:\\.\\d+)*(?:[-+][0-9a-z]+(?:\\.[0-9a-z]+)*)*)" ) } );

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

    // Four numbers from 0 to 255, not part of a longer number or dotted
    // sequence; a dot ending a sentence may follow.
    templates.append( { tr( "IPv4 address" ),
                        tr( "The first IPv4 address in a line, e.g. 192.168.1.20" ),
                        QStringLiteral( "IP address" ),
                        {},
                        QStringLiteral( "(?<!\\d|\\d\\.)(" ) + kOctet + QStringLiteral( "(?:\\." )
                            + kOctet + QStringLiteral( "){3})(?!\\d|\\.\\d)" ) } );

    // Date, `T` or a space as RFC 3339 allows, hours and minutes, optional
    // seconds (60 for a leap second) with a fraction after `.` or `,`, and
    // an optional `Z` or offset. Not part of a longer number.
    templates.append(
        { tr( "Timestamp (ISO 8601)" ),
          tr( "The first date and time in a line, e.g. 2024-01-15T10:30:00Z or "
              "2024-01-15 10:30:00.123" ),
          QStringLiteral( "Timestamp" ),
          {},
          QStringLiteral( "(?<!\\d)(\\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\\d|3[01])"
                          "[T ](?:[01]\\d|2[0-3]):[0-5]\\d(?::(?:[0-5]\\d|60)(?:[.,]\\d+)?)?"
                          "(?:Z|[+-](?:[01]\\d|2[0-3]):?[0-5]\\d)?)(?!\\d)" ) } );

    // The key the user gives, then `=`; the value ends at whitespace, as in
    // logfmt. For `a=1;b=2` or `a=1, b=2`, choose the character in the panel.
    templates.append( { tr( "key=value" ),
                        tr( "The value after a key you give and =, up to the next whitespace" ),
                        {},
                        simple( QStringLiteral( "=" ) ),
                        {} } );

    return templates;
}

} // namespace

FooterEntry RuleTemplate::entry( const QString& askedKey ) const
{
    FooterEntry entry;
    entry.key = asksForKey() ? askedKey.trimmed() : key;
    if ( entry.key.isEmpty() ) {
        return {};
    }
    if ( !isSimple() ) {
        entry.linePattern = linePattern;
        return entry;
    }
    auto rule = simple;
    if ( asksForKey() ) {
        rule.textBefore.prepend( entry.key );
    }
    applySimpleRule( rule, entry );
    return entry;
}

const QList<RuleTemplate>& ruleTemplates()
{
    static const QList<RuleTemplate> templates = makeTemplates();
    return templates;
}

} // namespace custom_footer
