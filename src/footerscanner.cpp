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

#include <algorithm>
#include <utility>

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

/// A raw value as shown: the display value of the first mapping of exactly
/// this raw value, or the raw value itself.
QString mapped( const QList<ValueMapping>& mappings, const QString& rawValue )
{
    for ( const auto& mapping : mappings ) {
        if ( rawValue == mapping.pattern ) {
            return mapping.displayValue;
        }
    }
    return rawValue;
}

/// How a line read by forEachLine() ended.
enum class LineKind {
    Complete,     ///< With a line break.
    Unterminated, ///< The file's last line, without a line break: it may still be being written.
    Stopped,      ///< The scan limit was reached inside it; only its start was read.
};

/// Where reading lines stands: the end of the last complete line, or where
/// the scan limit stopped reading inside a line; and the complete lines read.
struct LinePosition {
    qint64 offset = 0;
    int lines = 0;

    bool limitReached( int maxLines ) const
    {
        return ( maxLines > 0 && lines >= maxLines ) || offset >= FooterScanner::kMaxScanBytes;
    }
};

/**
 * Read the lines of a file from position.offset, within the scan limits, and
 * pass each to onLine( line, kind, lineNumber ), which returns whether to
 * read on. The one place that decides how lines are read, counted and
 * limited, for the footer's scans and the editor's previews alike.
 *
 * A complete line advances the position past it. A last line without a
 * line break is passed but not counted. A line the scan limit stopped in is
 * passed with its start, the position set to where reading stopped, and no
 * more lines are read. Returns false when cancelled.
 */
template <typename OnLine>
bool forEachLine( QFile& file, LinePosition& position, int maxLines,
                  const std::atomic_bool* cancelled, OnLine&& onLine )
{
    if ( !file.seek( position.offset ) ) {
        return true;
    }
    QByteArray rawLine;
    bool terminated = false;
    while ( !position.limitReached( maxLines ) ) {
        if ( isCancelled( cancelled ) ) {
            return false;
        }
        const auto read
            = readBoundedLine( file, FooterScanner::kMaxLineBytes, FooterScanner::kMaxScanBytes,
                               cancelled, rawLine, terminated );
        if ( read == ReadResult::End ) {
            break;
        }
        auto kind = terminated ? LineKind::Complete : LineKind::Unterminated;
        if ( read == ReadResult::Stopped ) {
            if ( isCancelled( cancelled ) ) {
                return false;
            }
            kind = LineKind::Stopped;
        }

        const bool readOn = onLine( QString::fromUtf8( rawLine ), kind,
                                    static_cast<qint64>( position.lines ) + 1 );
        if ( kind == LineKind::Stopped ) {
            // Remember where the scan stopped, so that a rescan knows the
            // file and does not read the line again.
            position.offset = file.pos();
            break;
        }
        if ( kind == LineKind::Complete ) {
            ++position.lines;
            position.offset = file.pos();
        }
        if ( !readOn ) {
            break;
        }
    }
    return true;
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

        QString problem;
        auto rule = compile( entry, i, &problem );
        if ( !rule ) {
            problems_.append( describe( problem ) );
            continue;
        }

        if ( !keys_.contains( entry.key ) ) {
            keys_.append( entry.key );
        }
        rules_.append( std::move( *rule ) );
    }
}

std::optional<FooterScanner::Rule> FooterScanner::compile( const FooterEntry& entry, int index,
                                                           QString* problem )
{
    const auto lineError = patternError( entry.linePattern );
    if ( !lineError.isEmpty() ) {
        *problem = QStringLiteral( "line pattern: %1" ).arg( lineError );
        return std::nullopt;
    }
    const auto valueError = patternError( entry.valuePattern );
    if ( !valueError.isEmpty() ) {
        *problem = QStringLiteral( "value pattern: %1" ).arg( valueError );
        return std::nullopt;
    }

    Rule rule;
    rule.index = index;
    rule.key = entry.key;
    rule.lineRegex.setPattern( entry.linePattern );
    if ( !entry.valuePattern.isEmpty() ) {
        rule.valueRegex.setPattern( entry.valuePattern );
    }
    rule.mappings = entry.mappings;
    return rule;
}

QMap<QString, QString> FooterScanner::scanFile( const QString& filePath, int maxLines,
                                                const std::atomic_bool* cancelled ) const
{
    QMap<QString, QString> shown;
    const auto values = scanFrom( filePath, {}, maxLines, cancelled ).values;
    for ( auto it = values.begin(); it != values.end(); ++it ) {
        shown.insert( it.key(), it->value );
    }
    return shown;
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

    // Values found in a last line without a line break.
    Values unterminated;
    // The scan limit was reached inside a line, before its end.
    bool stoppedInLine = false;

    LinePosition position{ progress.offset, progress.lines };
    if ( progress.values.size() < keys_.size() ) {
        const bool completed
            = forEachLine( file, position, maxLines, cancelled,
                           [ this, &progress, &unterminated,
                             &stoppedInLine ]( const QString& line, LineKind kind, qint64 ) {
                               stoppedInLine = kind == LineKind::Stopped;
                               // The start of a line the limit stopped in is matched like
                               // that of any over-long line.
                               const bool terminated = kind != LineKind::Unterminated;
                               auto& values = terminated ? progress.values : unterminated;
                               if ( !terminated ) {
                                   values = progress.values;
                               }
                               for ( const auto& rule : rules_ ) {
                                   if ( values.contains( rule.key ) ) {
                                       continue;
                                   }
                                   if ( auto found = valueOf( rule, line ) ) {
                                       values.insert( rule.key, std::move( *found ) );
                                   }
                               }
                               return progress.values.size() < keys_.size();
                           } );
        if ( !completed ) {
            return {};
        }
    }
    progress.offset = position.offset;
    progress.lines = position.lines;

    progress.done = progress.values.size() == keys_.size() || position.limitReached( maxLines )
                    || stoppedInLine;
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

std::optional<FooterValue> FooterScanner::valueOf( const Rule& rule, const QString& line )
{
    // The footer's hot path: nothing but the value.
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
    return FooterValue{ rule.key, mapped( rule.mappings, rawValue ), rawValue, rule.index };
}

std::optional<FooterScanner::Match> FooterScanner::matchOf( const Rule& rule, const QString& line )
{
    // As valueOf(), and where in the line the value was found.
    const auto lineMatch = rule.lineRegex.match( line );
    if ( !lineMatch.hasMatch() ) {
        return std::nullopt;
    }
    auto valueMatch = lineMatch;
    if ( !rule.valueRegex.pattern().isEmpty() ) {
        valueMatch = rule.valueRegex.match( line );
        if ( !valueMatch.hasMatch() ) {
            return std::nullopt;
        }
    }

    Match match;
    match.lineMatchStart = lineMatch.capturedStart( 0 );
    match.lineMatchLength = lineMatch.capturedLength( 0 );
    const int group = valueMatch.lastCapturedIndex() >= 1 ? 1 : 0;
    match.valueStart = valueMatch.capturedStart( group );
    match.valueLength = match.valueStart < 0 ? 0 : valueMatch.capturedLength( group );

    const auto rawValue = capturedValue( valueMatch );
    match.value = FooterValue{ rule.key, mapped( rule.mappings, rawValue ), rawValue, rule.index };
    return match;
}

FooterScanner::Preview FooterScanner::preview( const QString& filePath,
                                               const QList<FooterEntry>& entries, int rule,
                                               int maxLines, const std::atomic_bool* cancelled )
{
    Preview preview;
    preview.filePath = filePath;
    if ( rule < 0 || rule >= entries.size() ) {
        return preview;
    }

    const auto& entry = entries[ rule ];
    preview.enabled = entry.enabled;
    preview.key = entry.key;
    if ( entry.linePattern.isEmpty() ) {
        preview.status = Preview::Status::NoLinePattern;
        return preview;
    }
    QString problem;
    const auto selected = compile( entry, rule, &problem );
    if ( !selected ) {
        preview.status = Preview::Status::InvalidPattern;
        preview.error = problem;
        return preview;
    }
    if ( filePath.isEmpty() ) {
        preview.status = Preview::Status::NoFile;
        return preview;
    }

    // The rules the footer would take the key's value from: the enabled and
    // valid ones with this key, in list order, as the constructor keeps them.
    QList<Rule> keyRules;
    if ( !entry.key.trimmed().isEmpty() ) {
        QList<FooterEntry> sameKey = entries;
        for ( auto& other : sameKey ) {
            other.enabled = other.enabled && other.key == entry.key;
        }
        keyRules = FooterScanner( sameKey ).rules_;
        preview.sharedKey = std::any_of( keyRules.begin(), keyRules.end(),
                                         [ rule ]( const Rule& r ) { return r.index != rule; } );
    }

    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly ) ) {
        preview.status = Preview::Status::Unreadable;
        return preview;
    }

    const auto cancel = [] {
        Preview cancelledPreview;
        cancelledPreview.status = Preview::Status::Cancelled;
        return cancelledPreview;
    };

    LinePosition position;
    bool partialLine = false;
    bool stoppedInLine = false;
    const bool completed
        = forEachLine( file, position, maxLines, cancelled,
                       [ & ]( const QString& line, LineKind kind, qint64 lineNumber ) {
                           partialLine = kind != LineKind::Complete;
                           stoppedInLine = kind == LineKind::Stopped;

                           // Where the first match is, is only needed once.
                           std::optional<FooterValue> own;
                           if ( preview.matches == 0 ) {
                               if ( auto match = matchOf( *selected, line ) ) {
                                   own = match->value;
                                   preview.value = std::move( match->value );
                                   preview.lineNumber = lineNumber;
                                   preview.line = line;
                                   preview.lineMatchStart = match->lineMatchStart;
                                   preview.lineMatchLength = match->lineMatchLength;
                                   preview.valueStart = match->valueStart;
                                   preview.valueLength = match->valueLength;
                               }
                           }
                           else {
                               own = valueOf( *selected, line );
                           }
                           if ( own ) {
                               ++preview.matches;
                           }

                           if ( !preview.keyValue ) {
                               for ( const auto& keyRule : std::as_const( keyRules ) ) {
                                   // The rule itself, when it is one of the key's: matched above.
                                   auto value
                                       = keyRule.index == rule ? own : valueOf( keyRule, line );
                                   if ( value ) {
                                       preview.keyValue = std::move( value );
                                       preview.keyLineNumber = lineNumber;
                                       break;
                                   }
                               }
                           }
                           return true;
                       } );
    if ( !completed ) {
        return cancel();
    }

    // Counting a last line that is unterminated, or that the limit stopped in.
    preview.lines = position.lines + ( partialLine ? 1 : 0 );
    const bool more = !file.atEnd();
    preview.byteLimitReached = more && ( stoppedInLine || position.offset >= kMaxScanBytes );
    preview.lineLimitReached
        = more && !preview.byteLimitReached && maxLines > 0 && position.lines >= maxLines;
    preview.status = Preview::Status::Scanned;
    return preview;
}

QList<FooterValue> FooterScanner::footerValues( const Values& values ) const
{
    QList<FooterValue> ordered;
    for ( const auto& key : keys_ ) {
        const auto it = values.find( key );
        if ( it != values.end() ) {
            ordered.append( it.value() );
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
