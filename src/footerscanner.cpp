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
#include "rulecheck.h"

#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>
#include <QSysInfo>

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

using Encoding = FooterScanner::Encoding;

/// A line without its line break, and at most maxBytes long: whole code
/// units, as maxBytes is a multiple of a code unit.
void trimLine( QByteArray& line, const Encoding& encoding, qint64 maxBytes )
{
    const int unit = encoding.unitBytes();
    while ( line.size() >= unit ) {
        const char* last = line.constData() + line.size() - unit;
        if ( !encoding.isUnit( last, '\n' ) && !encoding.isUnit( last, '\r' ) ) {
            break;
        }
        line.chop( unit );
    }
    line.truncate( maxBytes );
}

/// Whether no whole code unit is left to read: at the end of the file, or
/// before half a code unit still being written.
bool atTextEnd( const QFile& file, const Encoding& encoding )
{
    const int unit = encoding.unitBytes();
    return unit == 1 ? file.atEnd() : file.bytesAvailable() < unit;
}

/// Read the next piece of a line, of at most maxBytes, whole code units:
/// up to and including the line feed if there is one within maxBytes.
/// terminated tells whether it ends with the line feed.
QByteArray readPiece( QFile& file, const Encoding& encoding, qint64 maxBytes, bool& terminated )
{
    const int unit = encoding.unitBytes();
    if ( unit == 1 ) {
        auto piece = file.readLine( maxBytes );
        terminated = piece.endsWith( '\n' );
        return piece;
    }

    // A line feed counts as a whole code unit only, so the bytes are looked
    // at unit by unit, and only whole units are taken. In chunks, so that a
    // short line costs little more than its own bytes.
    constexpr qint64 chunkBytes = 1024;
    QByteArray piece;
    terminated = false;
    while ( !terminated && piece.size() < maxBytes ) {
        const auto ahead = file.peek( qMin( chunkBytes, maxBytes - piece.size() ) );
        const auto whole = ahead.size() - ahead.size() % unit;
        if ( whole == 0 ) {
            break;
        }
        auto size = whole;
        for ( qsizetype at = 0; at < whole; at += unit ) {
            if ( encoding.isUnit( ahead.constData() + at, '\n' ) ) {
                size = at + unit;
                terminated = true;
                break;
            }
        }
        piece += file.read( size );
    }
    return piece;
}

/// Read one line of at most maxBytes, skipping the rest of a longer one.
/// terminated tells whether the line ended with a line break, rather than
/// with the end of the file. Skipping the rest of a line stops once the file
/// position reaches stopAt, or when the scan is cancelled, so that a huge
/// line is not read to its end; line then holds the start read so far.
/// Only whole code units are read: half a code unit at the end of a file
/// being written is left for a later scan.
ReadResult readBoundedLine( QFile& file, const Encoding& encoding, qint64 maxBytes, qint64 stopAt,
                            const std::atomic_bool* cancelled, QByteArray& line, bool& terminated )
{
    if ( atTextEnd( file, encoding ) ) {
        return ReadResult::End;
    }

    // One code unit more than maxBytes, to read a line of maxBytes and its
    // line feed at once.
    const auto pieceBytes = maxBytes + encoding.unitBytes();
    line = readPiece( file, encoding, pieceBytes, terminated );
    bool complete = terminated || atTextEnd( file, encoding );
    while ( !complete ) {
        if ( file.pos() >= stopAt || isCancelled( cancelled ) ) {
            trimLine( line, encoding, maxBytes );
            return ReadResult::Stopped;
        }
        const auto rest = readPiece( file, encoding, pieceBytes, terminated );
        complete = rest.isEmpty() || terminated || atTextEnd( file, encoding );
    }

    trimLine( line, encoding, maxBytes );
    return ReadResult::Line;
}

/// Read size bytes at offset, or fewer at the end of the file.
QByteArray readAt( QFile& file, qint64 offset, qint64 size )
{
    return file.seek( offset ) ? file.read( size ) : QByteArray();
}

/// The encoding of a file, from the host's name of it and its first bytes.
FooterScanner::Encoding encodingOf( QFile& file, const QByteArray& hostEncoding )
{
    // Enough for a byte order mark.
    constexpr qint64 markBytes = 4; // a UTF-32 mark, to tell it from UTF-16's
    return FooterScanner::Encoding::forHost( hostEncoding, readAt( file, 0, markBytes ) );
}

/// Whether @p name is windows-1252, by one of its usual names.
bool isWindows1252( const QByteArray& name )
{
    const auto key = name.toUpper().replace( '-', "" ).replace( '_', "" );
    return key == "WINDOWS1252" || key == "CP1252";
}

/// windows-1252 bytes as text, where Qt has no decoder for it (one without
/// ICU): Latin-1 but for 0x80 to 0x9F, as Windows decodes them.
QString fromWindows1252( const QByteArray& bytes )
{
    static const char16_t high[ 32 ] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
        0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
        0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    };
    QString text( bytes.size(), Qt::Uninitialized );
    for ( qsizetype at = 0; at < bytes.size(); ++at ) {
        const auto byte = static_cast<unsigned char>( bytes.at( at ) );
        text[ at ] = byte >= 0x80 && byte < 0xA0 ? QChar( high[ byte - 0x80 ] ) : QChar( byte );
    }
    return text;
}

/// Decodes the lines of one scan: made once, for the many lines it reads.
class LineDecoder {
public:
    explicit LineDecoder( const Encoding& encoding )
        : utf8_( encoding.form == Encoding::Form::Utf8 )
        , decoder_( decoderFor( encoding ) )
        , windows1252_( encoding.form == Encoding::Form::Named && !decoder_.isValid()
                        && isWindows1252( encoding.name ) )
    {
    }

    QString operator()( const QByteArray& bytes )
    {
        if ( utf8_ ) {
            return QString::fromUtf8( bytes );
        }
        if ( windows1252_ ) {
            return fromWindows1252( bytes );
        }
        if ( !decoder_.isValid() ) {
            // Not one this Qt decodes: its lines are still split right, and
            // their ASCII is still read right.
            return QString::fromLatin1( bytes );
        }
        return decoder_.decode( bytes );
    }

private:
    static QStringDecoder decoderFor( const Encoding& encoding )
    {
        // Each line on its own, and a U+FEFF at its start kept: the file's
        // mark is not in any line, so one there is text.
        const auto flags
            = QStringConverter::Flag::Stateless | QStringConverter::Flag::ConvertInitialBom;
        switch ( encoding.form ) {
        case Encoding::Form::Utf8:
            break;
        case Encoding::Form::Utf16LE:
            return QStringDecoder( QStringConverter::Utf16LE, flags );
        case Encoding::Form::Utf16BE:
            return QStringDecoder( QStringConverter::Utf16BE, flags );
        case Encoding::Form::Utf32LE:
            return QStringDecoder( QStringConverter::Utf32LE, flags );
        case Encoding::Form::Utf32BE:
            return QStringDecoder( QStringConverter::Utf32BE, flags );
        case Encoding::Form::Named:
            // Invalid where this Qt does not know the name.
            return QStringDecoder( encoding.name.constData(), flags );
        }
        return QStringDecoder( QStringConverter::Utf8, flags );
    }

    bool utf8_;
    QStringDecoder decoder_;
    bool windows1252_;
};

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
 * read on. The one place that decides how lines are read, split, decoded,
 * counted and limited, for the footer's scans and the editor's previews
 * alike: in the given encoding, from an offset on a code unit boundary.
 *
 * A complete line advances the position past it. A last line without a
 * line break is passed but not counted. A line the scan limit stopped in is
 * passed with its start, the position set to where reading stopped, and no
 * more lines are read. Returns false when cancelled.
 */
template <typename OnLine>
bool forEachLine( QFile& file, const Encoding& encoding, LinePosition& position, int maxLines,
                  const std::atomic_bool* cancelled, OnLine&& onLine )
{
    if ( !file.seek( position.offset ) ) {
        return true;
    }
    QByteArray rawLine;
    bool terminated = false;
    LineDecoder decode( encoding );
    while ( !position.limitReached( maxLines ) ) {
        if ( isCancelled( cancelled ) ) {
            return false;
        }
        const auto read
            = readBoundedLine( file, encoding, FooterScanner::kMaxLineBytes,
                               FooterScanner::kMaxScanBytes, cancelled, rawLine, terminated );
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

        const bool readOn
            = onLine( decode( rawLine ), kind, static_cast<qint64>( position.lines ) + 1 );
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

FooterScanner::Encoding FooterScanner::Encoding::detect( const QByteArray& start )
{
    // Only a UTF-16 mark, as LogSquirl takes it for the encoding. A UTF-8
    // mark stays in the first line, as it always has; decoding drops it.
    // UTF-32 is not read as UTF-16: LogSquirl checks its marks first, and
    // UTF-32LE's starts with UTF-16LE's.
    if ( start.startsWith( QByteArray( "\xFF\xFE\x00\x00", 4 ) )
         || start.startsWith( QByteArray( "\x00\x00\xFE\xFF", 4 ) ) ) {
        return {};
    }
    if ( start.startsWith( "\xFF\xFE" ) ) {
        return { Form::Utf16LE, 2 };
    }
    if ( start.startsWith( "\xFE\xFF" ) ) {
        return { Form::Utf16BE, 2 };
    }
    return {};
}

FooterScanner::Encoding FooterScanner::Encoding::forHost( const QByteArray& hostName,
                                                          const QByteArray& start )
{
    if ( hostName.isEmpty() ) {
        return detect( start );
    }

    // The names Qt gives the Unicode encodings, as LogSquirl passes them on,
    // in any case and with or without '-'; each with its byte order mark.
    struct Unicode {
        QByteArray key;
        Form form;
        QByteArray mark;
    };
    static const Unicode unicode[] = {
        { "UTF16LE", Form::Utf16LE, QByteArray( "\xFF\xFE", 2 ) },
        { "UTF16BE", Form::Utf16BE, QByteArray( "\xFE\xFF", 2 ) },
        { "UTF32LE", Form::Utf32LE, QByteArray( "\xFF\xFE\x00\x00", 4 ) },
        { "UTF32BE", Form::Utf32BE, QByteArray( "\x00\x00\xFE\xFF", 4 ) },
    };
    const auto find = []( const QByteArray& key ) -> const Unicode* {
        for ( const auto& encoding : unicode ) {
            if ( encoding.key == key ) {
                return &encoding;
            }
        }
        return nullptr;
    };

    auto key = hostName.toUpper().replace( '-', "" ).replace( '_', "" );
    // A UTF-16 mark wins over a name of another encoding: until its first
    // load has detected the encoding, the host names the locale's, and a
    // file with a UTF-16 mark read in any other encoding is no text anyway.
    const auto marked = detect( start );
    if ( marked.form != Form::Utf8 && !key.startsWith( "UTF16" ) && !key.startsWith( "UTF32" ) ) {
        return marked;
    }
    if ( key == "UTF8" ) {
        // A UTF-8 mark stays in the first line, as it always has.
        return {};
    }
    if ( key == "UTF16" || key == "UTF32" ) {
        // Of the mark's byte order, else of this machine's, as Qt decodes it.
        const bool bigEndian = start.startsWith( find( key + "BE" )->mark )
                               || ( !start.startsWith( find( key + "LE" )->mark )
                                    && QSysInfo::ByteOrder == QSysInfo::BigEndian );
        key += bigEndian ? "BE" : "LE";
    }
    if ( const auto* found = find( key ) ) {
        // The mark of this encoding, if the file starts with it, is no text.
        return { found->form, start.startsWith( found->mark ) ? found->mark.size() : 0, {} };
    }
    return { Form::Named, 0, hostName };
}

FooterScanner::Encoding FooterScanner::Encoding::ofFile( const QString& filePath,
                                                         const QByteArray& hostName )
{
    QFile file( filePath );
    if ( filePath.isEmpty() || !file.open( QIODevice::ReadOnly ) ) {
        return forHost( hostName, {} );
    }
    return encodingOf( file, hostName );
}

int FooterScanner::Encoding::unitBytes() const
{
    switch ( form ) {
    case Form::Utf16LE:
    case Form::Utf16BE:
        return 2;
    case Form::Utf32LE:
    case Form::Utf32BE:
        return 4;
    case Form::Utf8:
    case Form::Named:
        break;
    }
    return 1;
}

bool FooterScanner::Encoding::isUnit( const char* at, char unit ) const
{
    switch ( form ) {
    case Form::Utf16LE:
        return at[ 0 ] == unit && at[ 1 ] == '\0';
    case Form::Utf16BE:
        return at[ 0 ] == '\0' && at[ 1 ] == unit;
    case Form::Utf32LE:
        return at[ 0 ] == unit && at[ 1 ] == '\0' && at[ 2 ] == '\0' && at[ 3 ] == '\0';
    case Form::Utf32BE:
        return at[ 0 ] == '\0' && at[ 1 ] == '\0' && at[ 2 ] == '\0' && at[ 3 ] == unit;
    case Form::Utf8:
    case Form::Named:
        break;
    }
    return at[ 0 ] == unit;
}

FooterScanner::FooterScanner( const QList<FooterEntry>& entries )
{
    for ( int i = 0; i < entries.size(); ++i ) {
        const auto& entry = entries[ i ];
        if ( !RuleCheck::isUsed( entry ) ) {
            continue;
        }

        // Skipped for the problems the editor shows, in its words.
        auto rule = compile( entry, i );
        const auto problems
            = RuleCheck::problemsOf( RuleCheck::Rule( entry ), rule.lineRegex, rule.valueRegex );
        if ( !problems.isEmpty() ) {
            problems_ += RuleCheck::listing( i, problems );
            continue;
        }

        if ( !keys_.contains( entry.key ) ) {
            keys_.append( entry.key );
        }
        rules_.append( std::move( rule ) );
    }
}

FooterScanner::Rule FooterScanner::compile( const FooterEntry& entry, int index )
{
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
                                             int maxLines, const std::atomic_bool* cancelled,
                                             const QByteArray& hostEncoding ) const
{
    Scan scan;

    if ( filePath.isEmpty() || rules_.isEmpty() ) {
        return scan;
    }

    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly ) ) {
        return scan;
    }
    // Before reading: bytes appended while the scan reads count as unread.
    scan.fileSize = file.size();

    auto& progress = scan.progress;
    // Told from the file's start and the host's name: read in another
    // encoding than before, the file's lines are others.
    const auto encoding = encodingOf( file, hostEncoding );
    scan.resumed = continues( file, from ) && from.encoding == encoding;
    if ( scan.resumed ) {
        progress = from;
        if ( progress.done ) {
            scan.values = progress.values;
            return scan;
        }
    }
    else {
        progress.birthTime = QFileInfo( file ).birthTime();
        progress.encoding = encoding;
        progress.offset = progress.encoding.textStart;
    }

    // Values found in a last line without a line break.
    Values unterminated;
    // The scan limit was reached inside a line, before its end.
    bool stoppedInLine = false;

    LinePosition position{ progress.offset, progress.lines };
    if ( progress.values.size() < keys_.size() ) {
        const bool completed
            = forEachLine( file, progress.encoding, position, maxLines, cancelled,
                           [ this, &progress, &unterminated, &stoppedInLine ](
                               const QString& line, LineKind kind, qint64 lineNumber ) {
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
                                   // The line is counted on from the progress: a
                                   // continued scan numbers the lines of the whole file.
                                   if ( auto found = valueOf( rule, line ) ) {
                                       found->lineNumber = lineNumber;
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

std::optional<FooterScanner::Matches> FooterScanner::matchesOf( const Rule& rule,
                                                                const QString& line )
{
    Matches matches;
    matches.line = rule.lineRegex.match( line );
    if ( !matches.line.hasMatch() ) {
        return std::nullopt;
    }
    if ( rule.valueRegex.pattern().isEmpty() ) {
        matches.value = matches.line;
    }
    else {
        matches.value = rule.valueRegex.match( line );
        if ( !matches.value.hasMatch() ) {
            return std::nullopt;
        }
    }
    return matches;
}

std::optional<FooterValue> FooterScanner::valueOf( const Rule& rule, const QString& line )
{
    // The footer's hot path: the value, without where it was found.
    const auto matches = matchesOf( rule, line );
    if ( !matches ) {
        return std::nullopt;
    }
    const auto rawValue = capturedValue( matches->value );
    return FooterValue{ rule.key, mapped( rule.mappings, rawValue ), rawValue, rule.index };
}

std::optional<FooterScanner::Match> FooterScanner::matchOf( const Rule& rule, const QString& line )
{
    const auto matches = matchesOf( rule, line );
    if ( !matches ) {
        return std::nullopt;
    }

    Match match;
    match.lineMatchStart = matches->line.capturedStart( 0 );
    match.lineMatchLength = matches->line.capturedLength( 0 );
    const int group = matches->value.lastCapturedIndex() >= 1 ? 1 : 0;
    match.valueStart = matches->value.capturedStart( group );
    match.valueLength = match.valueStart < 0 ? 0 : matches->value.capturedLength( group );

    const auto rawValue = capturedValue( matches->value );
    match.value = FooterValue{ rule.key, mapped( rule.mappings, rawValue ), rawValue, rule.index };
    return match;
}

FooterScanner::Preview FooterScanner::preview( const QString& filePath,
                                               const QList<FooterEntry>& entries, int rule,
                                               int maxLines, const std::atomic_bool* cancelled,
                                               const QByteArray& hostEncoding )
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
    // A rule without a key still matches; only its patterns can keep it
    // from matching.
    const auto selected = compile( entry, rule );
    const auto problem
        = RuleCheck::problemsOf( RuleCheck::Rule( entry ), selected.lineRegex, selected.valueRegex )
              .preventingMatches();
    if ( !problem.isEmpty() ) {
        preview.status = Preview::Status::InvalidPattern;
        preview.error = problem;
        return preview;
    }
    if ( filePath.isEmpty() ) {
        preview.status = Preview::Status::NoFile;
        return preview;
    }

    // The rules the footer would take the key's value from: the enabled
    // ones with this key that the rule check accepts, in list order, as the
    // constructor keeps them. None for a rule without a key.
    QList<FooterEntry> sameKey = entries;
    for ( auto& other : sameKey ) {
        other.enabled = other.enabled && other.key == entry.key;
    }
    const QList<Rule> keyRules = FooterScanner( sameKey ).rules_;
    preview.sharedKey = std::any_of( keyRules.begin(), keyRules.end(),
                                     [ rule ]( const Rule& r ) { return r.index != rule; } );

    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly ) ) {
        preview.status = Preview::Status::Unreadable;
        return preview;
    }
    // Before reading, as the scan does: bytes appended while the preview
    // reads count as unread.
    preview.fileSize = file.size();

    const auto cancel = [] {
        Preview cancelledPreview;
        cancelledPreview.status = Preview::Status::Cancelled;
        return cancelledPreview;
    };

    auto& read = preview.read;
    read.encoding = encodingOf( file, hostEncoding );
    LinePosition position{ read.encoding.textStart, 0 };
    bool partialLine = false;
    bool stoppedInLine = false;
    const bool completed
        = forEachLine( file, read.encoding, position, maxLines, cancelled,
                       [ & ]( const QString& line, LineKind kind, qint64 lineNumber ) {
                           partialLine = kind != LineKind::Complete;
                           stoppedInLine = kind == LineKind::Stopped;

                           // Where the first match is, is only needed once.
                           std::optional<FooterValue> own;
                           if ( preview.matches == 0 ) {
                               if ( auto match = matchOf( selected, line ) ) {
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
                               own = valueOf( selected, line );
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
    // The values carry their lines, as the footer's do: derived here, and
    // only here, from lineNumber and keyLineNumber, the one record of them.
    if ( preview.value ) {
        preview.value->lineNumber = preview.lineNumber;
    }
    if ( preview.keyValue ) {
        preview.keyValue->lineNumber = preview.keyLineNumber;
    }

    // Counting a last line that is unterminated, or that the limit stopped in.
    preview.lines = position.lines + ( partialLine ? 1 : 0 );
    // Part of a code unit at the end of a growing UTF-16 or UTF-32 file is
    // no more to read.
    const bool more = !atTextEnd( file, read.encoding );
    preview.byteLimitReached = more && ( stoppedInLine || position.offset >= kMaxScanBytes );
    preview.lineLimitReached
        = more && !preview.byteLimitReached && maxLines > 0 && position.lines >= maxLines;

    read.offset = position.offset;
    read.lines = position.lines;
    read.done = preview.limitReached();
    read.birthTime = QFileInfo( file ).birthTime();
    read.head = readAt( file, 0, qMin( read.offset, kIdentityBytes ) );
    const auto tailSize = qMin( read.offset, kIdentityBytes );
    read.tail = readAt( file, read.offset - tailSize, tailSize );

    preview.status = Preview::Status::Scanned;
    return preview;
}

bool FooterScanner::fileUnchangedFor( const QString& filePath, const Preview& preview )
{
    if ( preview.status != Preview::Status::Scanned || preview.filePath != filePath ) {
        return false;
    }
    return unchangedSince( filePath, preview.read, preview.limitReached(), preview.fileSize );
}

bool FooterScanner::fileUnchangedFor( const QString& filePath, const Scan& scan )
{
    return unchangedSince( filePath, scan.progress, scan.progress.done, scan.fileSize );
}

bool FooterScanner::unchangedSince( const QString& filePath, const Progress& read, bool beyondRead,
                                    qint64 size )
{
    QFile file( filePath );
    if ( !file.open( QIODevice::ReadOnly ) || !continues( file, read ) ) {
        return false;
    }
    // Bytes after what was read, such as a last line without a line break,
    // are not remembered: rewritten in place they keep the size, so only a
    // file read to its end counts as unchanged by its size.
    return beyondRead || ( file.size() == size && size == read.offset );
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
