/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "footerscanner.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace costume_footer {

QMap<QString, QString> FooterScanner::scan( const QString& filePath,
                                            const QList<FooterEntry>& entries,
                                            int maxLines )
{
    QMap<QString, QString> results;

    if ( filePath.isEmpty() || entries.isEmpty() ) {
        return results;
    }

    // Build compiled regexes for enabled entries only.
    struct Rule {
        QString key;
        QRegularExpression regex;
    };
    QList<Rule> rules;
    for ( const auto& entry : entries ) {
        if ( !entry.enabled || entry.regexPattern.isEmpty() ) {
            continue;
        }
        QRegularExpression re( entry.regexPattern );
        if ( re.isValid() ) {
            rules.append( { entry.key, std::move( re ) } );
        }
    }

    if ( rules.isEmpty() ) {
        return results;
    }

    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        return results;
    }

    QTextStream stream( &file );
    int lineCount = 0;

    while ( !stream.atEnd() ) {
        const QString line = stream.readLine();
        ++lineCount;

        // Try each rule that hasn't matched yet.
        for ( int i = rules.size() - 1; i >= 0; --i ) {
            const auto& rule = rules[ i ];
            if ( results.contains( rule.key ) ) {
                continue; // Already matched this key.
            }

            const auto match = rule.regex.match( line );
            if ( match.hasMatch() ) {
                // Use first capturing group if available, otherwise full match.
                const QString value = ( match.lastCapturedIndex() >= 1 )
                                          ? match.captured( 1 )
                                          : match.captured( 0 );
                results.insert( rule.key, value );

                // If all rules matched, stop early.
                if ( results.size() == rules.size() ) {
                    return results;
                }
            }
        }

        if ( maxLines > 0 && lineCount >= maxLines ) {
            break;
        }
    }

    return results;
}

} // namespace costume_footer
