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

#include "footerscanner.h"

#include <QFile>
#include <QSet>

namespace custom_footer {

namespace {

/// The first capturing group of a match, or the full match without one.
QString capturedValue( const QRegularExpressionMatch& match )
{
    return match.lastCapturedIndex() >= 1 ? match.captured( 1 ) : match.captured( 0 );
}

/// Read one line of at most maxBytes, skipping the rest of a longer one.
/// Returns false at the end of the file.
bool readBoundedLine( QFile& file, qint64 maxBytes, QByteArray& line, qint64& bytesRead )
{
    if ( file.atEnd() ) {
        return false;
    }

    line = file.readLine( maxBytes + 1 );
    bytesRead += line.size();
    bool complete = line.endsWith( '\n' ) || file.atEnd();
    while ( !complete ) {
        const auto rest = file.readLine( maxBytes + 1 );
        bytesRead += rest.size();
        complete = rest.isEmpty() || rest.endsWith( '\n' ) || file.atEnd();
    }

    while ( line.endsWith( '\n' ) || line.endsWith( '\r' ) ) {
        line.chop( 1 );
    }
    line.truncate( maxBytes );
    return true;
}

} // namespace

FooterScanner::FooterScanner( const QList<FooterEntry>& entries )
{
    QSet<QString> keys;
    for ( int i = 0; i < entries.size(); ++i ) {
        const auto& entry = entries[ i ];
        if ( !entry.enabled || entry.linePattern.isEmpty() ) {
            continue;
        }

        const auto describe = [ &entry, i ]( const QString& problem ) {
            return QStringLiteral( "Rule %1 (%2): %3" ).arg( i + 1 ).arg( entry.key, problem );
        };

        if ( keys.contains( entry.key ) ) {
            problems_.append( describe( QStringLiteral( "key already used by an earlier rule" ) ) );
            continue;
        }

        const auto lineError = patternError( entry.linePattern );
        if ( !lineError.isEmpty() ) {
            problems_.append( describe( QStringLiteral( "line pattern: %1" ).arg( lineError ) ) );
            continue;
        }
        const auto valueError = patternError( entry.valuePattern );
        if ( !valueError.isEmpty() ) {
            problems_.append( describe( QStringLiteral( "value pattern: %1" ).arg( valueError ) ) );
            continue;
        }

        Rule rule;
        rule.key = entry.key;
        rule.lineRegex.setPattern( entry.linePattern );
        if ( !entry.valuePattern.isEmpty() ) {
            rule.valueRegex.setPattern( entry.valuePattern );
        }
        rule.mappings = entry.mappings;

        keys.insert( entry.key );
        rules_.append( std::move( rule ) );
    }
}

QMap<QString, QString> FooterScanner::scanFile( const QString& filePath, int maxLines,
                                                const std::atomic_bool* cancelled ) const
{
    QMap<QString, QString> results;

    if ( filePath.isEmpty() || rules_.isEmpty() ) {
        return results;
    }

    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly ) ) {
        return results;
    }

    QByteArray rawLine;
    qint64 bytesRead = 0;
    int lineCount = 0;

    while ( readBoundedLine( file, kMaxLineBytes, rawLine, bytesRead ) ) {
        if ( cancelled && cancelled->load( std::memory_order_relaxed ) ) {
            return {};
        }

        const QString line = QString::fromUtf8( rawLine );
        ++lineCount;

        for ( const auto& rule : rules_ ) {
            if ( results.contains( rule.key ) ) {
                continue;
            }

            const auto value = valueOf( rule, line );
            if ( !value ) {
                continue;
            }
            results.insert( rule.key, *value );

            if ( results.size() == rules_.size() ) {
                return results;
            }
        }

        if ( ( maxLines > 0 && lineCount >= maxLines ) || bytesRead >= kMaxScanBytes ) {
            break;
        }
    }

    return results;
}

std::optional<QString> FooterScanner::valueOf( const Rule& rule, const QString& line ) const
{
    const auto lineMatch = rule.lineRegex.match( line );
    if ( !lineMatch.hasMatch() ) {
        return std::nullopt;
    }

    QString rawValue;
    if ( rule.valueRegex.pattern().isEmpty() ) {
        rawValue = capturedValue( lineMatch );
    }
    else {
        const auto valueMatch = rule.valueRegex.match( line );
        if ( !valueMatch.hasMatch() ) {
            return std::nullopt;
        }
        rawValue = capturedValue( valueMatch );
    }

    // Apply value mappings (exact string match).
    for ( const auto& mapping : rule.mappings ) {
        if ( rawValue == mapping.pattern ) {
            return mapping.displayValue;
        }
    }
    return rawValue;
}

QList<QPair<QString, QString>>
FooterScanner::inRuleOrder( const QMap<QString, QString>& values ) const
{
    QList<QPair<QString, QString>> ordered;
    for ( const auto& rule : rules_ ) {
        const auto it = values.find( rule.key );
        if ( it != values.end() ) {
            ordered.append( { rule.key, it.value() } );
        }
    }
    return ordered;
}

QMap<QString, QString> FooterScanner::scan( const QString& filePath,
                                            const QList<FooterEntry>& entries, int maxLines )
{
    return FooterScanner( entries ).scanFile( filePath, maxLines );
}

QString FooterScanner::patternError( const QString& pattern )
{
    const QRegularExpression regex( pattern );
    return regex.isValid() ? QString() : regex.errorString();
}

} // namespace custom_footer
