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
        QRegularExpression lineRegex;
        QRegularExpression valueRegex;   // empty pattern = not used
        bool hasTwoStage = false;
        QList<ValueMapping> mappings;
    };
    QList<Rule> rules;
    for ( const auto& entry : entries ) {
        if ( !entry.enabled || entry.linePattern.isEmpty() ) {
            continue;
        }
        QRegularExpression lineRe( entry.linePattern );
        if ( !lineRe.isValid() ) {
            continue;
        }

        Rule rule;
        rule.key = entry.key;
        rule.lineRegex = std::move( lineRe );
        rule.mappings = entry.mappings;

        if ( !entry.valuePattern.isEmpty() ) {
            QRegularExpression valRe( entry.valuePattern );
            if ( valRe.isValid() ) {
                rule.valueRegex = std::move( valRe );
                rule.hasTwoStage = true;
            }
        }

        rules.append( std::move( rule ) );
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

        for ( int i = rules.size() - 1; i >= 0; --i ) {
            const auto& rule = rules[ i ];
            if ( results.contains( rule.key ) ) {
                continue;
            }

            const auto lineMatch = rule.lineRegex.match( line );
            if ( !lineMatch.hasMatch() ) {
                continue;
            }

            QString rawValue;

            if ( rule.hasTwoStage ) {
                // Two-stage: use valueRegex on the same line.
                const auto valMatch = rule.valueRegex.match( line );
                if ( valMatch.hasMatch() ) {
                    rawValue = ( valMatch.lastCapturedIndex() >= 1 )
                                   ? valMatch.captured( 1 )
                                   : valMatch.captured( 0 );
                }
                else {
                    continue; // Value pattern didn't match — skip this line.
                }
            }
            else {
                // Single-stage: extract from lineRegex capture group.
                rawValue = ( lineMatch.lastCapturedIndex() >= 1 )
                               ? lineMatch.captured( 1 )
                               : lineMatch.captured( 0 );
            }

            // Apply value mappings (exact string match).
            QString displayValue = rawValue;
            for ( const auto& mapping : rule.mappings ) {
                if ( rawValue == mapping.pattern ) {
                    displayValue = mapping.displayValue;
                    break;
                }
            }

            results.insert( rule.key, displayValue );

            if ( results.size() == rules.size() ) {
                return results;
            }
        }

        if ( maxLines > 0 && lineCount >= maxLines ) {
            break;
        }
    }

    return results;
}

} // namespace costume_footer
