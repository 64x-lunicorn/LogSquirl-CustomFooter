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
#include <QFileInfo>

namespace custom_footer {

namespace {

/// The first capturing group of a match, or the full match without one.
QString capturedValue( const QRegularExpressionMatch& match )
{
    return match.lastCapturedIndex() >= 1 ? match.captured( 1 ) : match.captured( 0 );
}

bool isCancelled( const std::atomic_bool* cancelled )
{
    return cancelled && cancelled->load( std::memory_order_relaxed );
}

enum class ReadResult {
    Line,    ///< A line was read.
    End,     ///< The end of the file was reached.
    Stopped, ///< Cancelled, or stopAt was passed inside a line.
};

/// A line without its line break, and at most maxBytes long.
void trimLine( QByteArray& line, qint64 maxBytes )
{
    while ( line.endsWith( '\n' ) || line.endsWith( '\r' ) ) {
        line.chop( 1 );
    }
    line.truncate( maxBytes );
}

/// Read one line of at most maxBytes, skipping the rest of a longer one.
/// terminated tells whether the line ended with a line break, rather than
/// with the end of the file. Skipping the rest of a line stops once the file
/// position reaches stopAt, or when the scan is cancelled, so that a huge
/// line is not read to its end; line then holds the start read so far.
ReadResult readBoundedLine( QFile& file, qint64 maxBytes, qint64 stopAt,
                            const std::atomic_bool* cancelled, QByteArray& line, bool& terminated )
{
    if ( file.atEnd() ) {
        return ReadResult::End;
    }

    line = file.readLine( maxBytes + 1 );
    terminated = line.endsWith( '\n' );
    bool complete = terminated || file.atEnd();
    while ( !complete ) {
        if ( file.pos() >= stopAt || isCancelled( cancelled ) ) {
            trimLine( line, maxBytes );
            return ReadResult::Stopped;
        }
        const auto rest = file.readLine( maxBytes + 1 );
        terminated = rest.endsWith( '\n' );
        complete = rest.isEmpty() || terminated || file.atEnd();
    }

    trimLine( line, maxBytes );
    return ReadResult::Line;
}

/// Read size bytes at offset, or fewer at the end of the file.
QByteArray readAt( QFile& file, qint64 offset, qint64 size )
{
    return file.seek( offset ) ? file.read( size ) : QByteArray();
}

} // namespace

FooterScanner::FooterScanner( const QList<FooterEntry>& entries )
{
    for ( int i = 0; i < entries.size(); ++i ) {
        const auto& entry = entries[ i ];
        if ( !entry.enabled || entry.linePattern.isEmpty() ) {
            continue;
        }

        const auto describe = [ &entry, i ]( const QString& problem ) {
            return QStringLiteral( "Rule %1 (%2): %3" ).arg( i + 1 ).arg( entry.key, problem );
        };

        if ( entry.key.trimmed().isEmpty() ) {
            problems_.append( describe( QStringLiteral( "no key" ) ) );
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

        if ( !keys_.contains( entry.key ) ) {
            keys_.append( entry.key );
        }
        rules_.append( std::move( rule ) );
    }
}

QMap<QString, QString> FooterScanner::scanFile( const QString& filePath, int maxLines,
                                                const std::atomic_bool* cancelled ) const
{
    return scanFrom( filePath, {}, maxLines, cancelled ).values;
}

FooterScanner::Scan FooterScanner::scanFrom( const QString& filePath, const Progress& from,
                                             int maxLines, const std::atomic_bool* cancelled ) const
{
    Scan scan;

    if ( filePath.isEmpty() || rules_.isEmpty() ) {
        return scan;
    }

    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly ) ) {
        return scan;
    }

    auto& progress = scan.progress;
    scan.resumed = continues( file, from );
    if ( scan.resumed ) {
        progress = from;
        if ( progress.done ) {
            scan.values = progress.values;
            return scan;
        }
    }
    else {
        progress.birthTime = QFileInfo( file ).birthTime();
    }

    const auto limitReached = [ &progress, maxLines ] {
        return ( maxLines > 0 && progress.lines >= maxLines ) || progress.offset >= kMaxScanBytes;
    };

    // Values found in a last line without a line break.
    QMap<QString, QString> unterminated;
    // The scan limit was reached inside a line, before its end.
    bool stoppedInLine = false;

    if ( file.seek( progress.offset ) ) {
        QByteArray rawLine;
        bool terminated = false;
        while ( progress.values.size() < keys_.size() && !limitReached() ) {
            if ( isCancelled( cancelled ) ) {
                return {};
            }
            const auto read = readBoundedLine( file, kMaxLineBytes, kMaxScanBytes, cancelled,
                                               rawLine, terminated );
            if ( read == ReadResult::End ) {
                break;
            }
            if ( read == ReadResult::Stopped ) {
                if ( isCancelled( cancelled ) ) {
                    return {};
                }
                // The start of the line was read: match it like that of any
                // over-long line, then stop there.
                stoppedInLine = true;
                terminated = true;
            }

            auto& values = terminated ? progress.values : unterminated;
            if ( !terminated ) {
                values = progress.values;
            }

            const QString line = QString::fromUtf8( rawLine );
            for ( const auto& rule : rules_ ) {
                if ( values.contains( rule.key ) ) {
                    continue;
                }
                if ( const auto value = valueOf( rule, line ) ) {
                    values.insert( rule.key, *value );
                }
            }

            if ( stoppedInLine ) {
                // Remember where the scan stopped, so that a rescan knows
                // the file and does not read the line again.
                progress.offset = file.pos();
                break;
            }
            if ( terminated ) {
                ++progress.lines;
                progress.offset = file.pos();
            }
        }
    }

    progress.done = progress.values.size() == keys_.size() || limitReached() || stoppedInLine;
    progress.head = readAt( file, 0, qMin( progress.offset, kIdentityBytes ) );
    const auto tailSize = qMin( progress.offset, kIdentityBytes );
    progress.tail = readAt( file, progress.offset - tailSize, tailSize );

    scan.values = unterminated.isEmpty() ? progress.values : unterminated;
    return scan;
}

bool FooterScanner::continues( QFile& file, const Progress& from )
{
    if ( from.offset <= 0 || file.size() < from.offset ) {
        return false;
    }

    const auto birthTime = QFileInfo( file ).birthTime();
    if ( from.birthTime.isValid() && birthTime.isValid() && from.birthTime != birthTime ) {
        return false;
    }

    return readAt( file, 0, from.head.size() ) == from.head
           && readAt( file, from.offset - from.tail.size(), from.tail.size() ) == from.tail;
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
    for ( const auto& key : keys_ ) {
        const auto it = values.find( key );
        if ( it != values.end() ) {
            ordered.append( { key, it.value() } );
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
