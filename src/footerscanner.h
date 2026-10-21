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

#pragma once

#include "footerentry.h"
#include "footervalue.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <atomic>
#include <optional>

class QFile;

namespace custom_footer {

/**
 * Scans a log file and extracts the first match per enabled FooterEntry.
 *
 * The rules are compiled once on construction. Rules sharing a key are
 * alternatives: the key's value comes from the first line that any of its
 * rules matches; if several of them match that line, the first rule in list
 * order wins. Which rules are used is the RuleCheck's decision, as in the
 * editor: an enabled rule without a line pattern is incomplete and
 * skipped; one with a line pattern but a problem, such as no key or an
 * invalid pattern, is skipped and reported in problems().
 *
 * Every value found carries the Log Line it was found in
 * (FooterValue::lineNumber), counting from 1 in the whole file as the host
 * does: a line ends with "\n", so a "\r\n" ends one line too, and a last
 * line without a line break counts as well. In a UTF-16 or UTF-32 file,
 * "\n" is a whole code unit (Encoding).
 *
 * A file is read in the encoding the host reads it in, given by its name as
 * hostEncoding; an empty name tells it from the file's byte order mark.
 */
class FooterScanner {
public:
    static constexpr int kDefaultMaxLines = 100000;

    /// Longer lines are matched against their first kMaxLineBytes only.
    static constexpr qint64 kMaxLineBytes = 64 * 1024;

    /// A scan stops after this many bytes, whatever maxLines says, even
    /// inside a line.
    static constexpr qint64 kMaxScanBytes = 64 * 1024 * 1024;

    /// Bytes compared at the start and before the end of the scanned part of
    /// a file, to tell whether it is still the file scanned before.
    static constexpr qint64 kIdentityBytes = 256;

    /// Found values by key, each with its raw value and rule.
    using Values = QMap<QString, FooterValue>;

    /**
     * How a file's bytes are text: how wide a code unit is, which code unit
     * ends a line, how a line is decoded, and where the first line starts.
     *
     * A scan from the start of a file reads it in the encoding the host
     * names for it, where the host says (forHost()): UTF-16 or UTF-32 of
     * either byte order, with or without a byte order mark, UTF-8, or any
     * other encoding the host names, such as windows-1252, whose lines end
     * at the byte 0x0A. Where the host does not say, the encoding is told
     * from the file's first bytes (detect()), as LogSquirl does for a byte
     * order mark: a UTF-16 mark makes the file UTF-16 of that byte order.
     * Any other file is then read as UTF-8: also one with a UTF-8 mark, a
     * single-byte one, and a UTF-16 file without a mark. A byte order mark
     * of the encoding's UTF-16 or UTF-32 is not part of the first line; a
     * UTF-8 mark is in the first line but not in its decoded text. (A
     * continued scan keeps the encoding it was made with, in its progress.)
     */
    struct Encoding {
        enum class Form {
            Utf8,    ///< Lines end at the byte 0x0A.
            Utf16LE, ///< Lines end at the code unit 0x000A, low byte first.
            Utf16BE, ///< Lines end at the code unit 0x000A, high byte first.
            Utf32LE, ///< Lines end at the code unit 0x0000000A, low byte first.
            Utf32BE, ///< Lines end at the code unit 0x0000000A, high byte first.
            Named,   ///< Lines end at the byte 0x0A; decoded as name says.
        };
        Form form = Form::Utf8;
        /// Bytes of a byte order mark before the first line, not part of it.
        qint64 textStart = 0;
        /// With Form::Named, the host's name of the encoding, such as
        /// "windows-1252"; a name this Qt cannot decode is read as Latin-1,
        /// windows-1252 as Windows decodes it.
        QByteArray name;

        /// The encoding of a file starting with @p start, told from a byte
        /// order mark.
        static Encoding detect( const QByteArray& start );

        /// The encoding the host reads a file starting with @p start in, by
        /// its IANA name @p hostName ("UTF-16LE", "windows-1252", …, in any
        /// case); detect( @p start ) for an empty name. A UTF-16 byte order
        /// mark wins over a name that is not UTF-16 or UTF-32: the host
        /// names the locale's encoding until it has detected the file's.
        static Encoding forHost( const QByteArray& hostName, const QByteArray& start );

        /// The encoding of the file at @p filePath, from its first bytes
        /// and @p hostName (forHost()); UTF-8 if it cannot be read and no
        /// name is given.
        static Encoding ofFile( const QString& filePath, const QByteArray& hostName = {} );

        /// Bytes per code unit: 1, 2 or 4.
        int unitBytes() const;

        /// Whether the code unit at @p at is @p unit (an ASCII character).
        bool isUnit( const char* at, char unit ) const;

        bool operator==( const Encoding& other ) const
        {
            return form == other.form && textStart == other.textStart && name == other.name;
        }
    };

    /// How far a file has been scanned, to continue there once it has grown.
    struct Progress {
        /// How the file is read, told when it was scanned from its start.
        Encoding encoding;
        /// End of the last complete line scanned, or where the scan limit
        /// stopped the scan inside a line; after a byte order mark at least.
        qint64 offset = 0;
        int lines = 0;       ///< Complete lines scanned.
        Values values;       ///< Found in those lines.
        bool done = false;   ///< Every key has a value, or a limit was reached.
        QByteArray head;     ///< The file's first bytes, up to offset.
        QByteArray tail;     ///< The bytes just before offset.
        QDateTime birthTime; ///< Invalid where the platform has none.
    };

    struct Scan {
        /// Found so far, including in a last line that is not terminated yet.
        Values values;
        Progress progress;
        /// Whether the scan continued from the given progress.
        bool resumed = false;
        /// The file's size when the scan opened it, to tell whether a change
        /// of it can alter the scan (fileUnchangedFor()).
        qint64 fileSize = 0;
    };

    /// What one rule finds in a file, for the editor's live preview.
    struct Preview {
        enum class Status {
            Scanned,        ///< The file was scanned; see the fields below.
            NoFile,         ///< No file was given.
            NoLinePattern,  ///< The rule has no line pattern, so it finds nothing.
            InvalidPattern, ///< A pattern of the rule does not compile; see error.
            Unreadable,     ///< The file is not a regular file, or could not be opened.
            Cancelled,      ///< The preview was cancelled; nothing else is set.
        };
        Status status = Status::NoFile;
        QString error;       ///< Why a pattern is invalid, in the RuleCheck's words.
        QString filePath;    ///< The file previewed.
        QString key;         ///< The rule's key.
        bool enabled = true; ///< Whether the rule is enabled; a disabled one supplies nothing.

        /// The rule's first match, if any: the value the footer would show
        /// from it, and the line it is in. The value's lineNumber is
        /// lineNumber, copied once the preview is made.
        std::optional<FooterValue> value;
        qint64 lineNumber = 0; ///< Of the first match, counting from 1.
        QString line;          ///< The first matching line, as matched: at most kMaxLineBytes.
        /// Where the line pattern matched in line, and where the value was
        /// captured; -1 where the value was captured by an optional group
        /// that did not take part.
        qsizetype lineMatchStart = -1;
        qsizetype lineMatchLength = 0;
        qsizetype valueStart = -1;
        qsizetype valueLength = 0;

        /// Lines the rule takes a value from, within the limits.
        qint64 matches = 0;
        /// Lines scanned, including a last line without a line break.
        qint64 lines = 0;
        /// The scan stopped at maxLines, or at kMaxScanBytes, before the end
        /// of the file.
        bool lineLimitReached = false;
        bool byteLimitReached = false;

        /// Other enabled rules have the same key.
        bool sharedKey = false;
        /// The value the footer shows for the rule's key in this file, from
        /// whichever of its rules supplies it, and the line it is in. The
        /// value's lineNumber is keyLineNumber, as in the footer.
        std::optional<FooterValue> keyValue;
        qint64 keyLineNumber = 0;

        /// What was read of the file, to tell whether a change of it can
        /// alter the preview (fileUnchangedFor()); no values.
        Progress read;
        /// The file's size when the preview opened it, as Scan::fileSize, to
        /// tell whether a change of it can alter the preview
        /// (fileUnchangedFor()).
        qint64 fileSize = 0;

        bool limitReached() const
        {
            return lineLimitReached || byteLimitReached;
        }
    };

    explicit FooterScanner( const QList<FooterEntry>& entries );

    /// The RuleCheck's listing of the skipped rules' problems ("Rule N: …"),
    /// in rule order.
    const QStringList& problems() const
    {
        return problems_;
    }

    /**
     * Scan a log file for matching key-value pairs.
     *
     * For each key, the first line that one of its rules matches provides the
     * value: the linePattern must match, and the valuePattern too if set; the
     * value is the first capturing group, or the full match without one.
     * Mappings are then applied.
     *
     * Only a regular file, or a link to one, is read; anything else, such as
     * a FIFO or a device, is not even opened, as that could block, and has
     * no values. So it is for every function here that reads a file.
     *
     * @param filePath   Path to the log file.
     * @param maxLines   Maximum number of lines to scan (0 = unlimited).
     * @param cancelled  Checked per line, and while skipping over-long lines;
     *                   when set, the scan returns nothing.
     * @return Map from entry key to matched value, as shown.
     */
    QMap<QString, QString> scanFile( const QString& filePath, int maxLines = kDefaultMaxLines,
                                     const std::atomic_bool* cancelled = nullptr ) const;

    /**
     * Scan a file, continuing where a scan of the same file stopped.
     *
     * Where the file only grew since @p from, only the bytes appended since
     * are read, and only while keys are missing and the limits allow. A file
     * that shrank, or whose start or scanned end differs, or with another
     * birth time, was replaced and is scanned from its start. A last line
     * without a line break is matched but not remembered in the progress, as
     * it may still be being written.
     *
     * A continued scan counts lines on from @p from, so the line numbers of
     * the values it finds are those in the whole file; values kept from
     * @p from keep theirs. A scan from the start numbers them afresh.
     *
     * The file is read in @p hostEncoding, the name of the encoding the
     * host reads it in (Encoding::forHost()); an empty name tells it from
     * the file's byte order mark. A file read in another encoding than
     * @p from is scanned from its start.
     *
     * A cancelled scan returns no values and empty progress.
     */
    Scan scanFrom( const QString& filePath, const Progress& from, int maxLines = kDefaultMaxLines,
                   const std::atomic_bool* cancelled = nullptr,
                   const QByteArray& hostEncoding = {} ) const;

    /**
     * Preview what the rule at @p rule of @p entries finds in a file, with
     * the matching of scanFile() and its limits.
     *
     * Unlike a scan, the preview reads on to the limits after the first
     * match, to count the matching lines. The rule is previewed even when
     * disabled; which rule supplies its key's value is decided among the
     * enabled rules with that key, as the footer does.
     *
     * @param cancelled     Checked per line, and while skipping over-long lines.
     * @param hostEncoding  The encoding the file is read in, as scanFrom() takes it.
     */
    static Preview preview( const QString& filePath, const QList<FooterEntry>& entries, int rule,
                            int maxLines = kDefaultMaxLines,
                            const std::atomic_bool* cancelled = nullptr,
                            const QByteArray& hostEncoding = {} );

    /**
     * Whether a change of @p filePath since @p preview was made cannot alter
     * it: the part of the file read is unchanged, and either the preview
     * stopped at a limit, so appended lines are beyond it, or the file has
     * its size still and ends with the last complete line read. A truncated
     * or replaced file is changed, and so is one ending in a line without a
     * line break, which may have been rewritten in place.
     */
    static bool fileUnchangedFor( const QString& filePath, const Preview& preview );

    /**
     * Whether a change of @p filePath since @p scan of it cannot alter it:
     * the part of the file scanned is unchanged, and either the scan is
     * done (every key has a value, or a limit was reached), so appended
     * lines are not scanned, or the file has its size still and ends with
     * the last complete line scanned. A truncated or replaced file is
     * changed, and so is one ending in a line without a line break, which
     * may have been rewritten in place, or one of which no complete line
     * was scanned, as there is nothing to compare.
     */
    static bool fileUnchangedFor( const QString& filePath, const Scan& scan );

    /// The given values, once per key, at the position of the key's first rule.
    QList<FooterValue> footerValues( const Values& values ) const;

    /// Compile the entries and scan a file in one go.
    static QMap<QString, QString> scan( const QString& filePath, const QList<FooterEntry>& entries,
                                        int maxLines = kDefaultMaxLines );

    /// Why a pattern does not compile, or an empty string if it does.
    static QString patternError( const QString& pattern );

private:
    struct Rule {
        int index = -1; ///< In the entries.
        QString key;
        QRegularExpression lineRegex;
        QRegularExpression valueRegex; // empty pattern = not used
        QList<ValueMapping> mappings;
    };

    /// A rule's value in a line, and where it was found there.
    struct Match {
        FooterValue value;
        qsizetype lineMatchStart = -1;
        qsizetype lineMatchLength = 0;
        qsizetype valueStart = -1;
        qsizetype valueLength = 0;
    };

    /// A rule's match of a line: the line pattern's, and the value
    /// pattern's, which is the line pattern's without a value pattern.
    struct Matches {
        QRegularExpressionMatch line;
        QRegularExpressionMatch value;
    };

    /// The matches of a rule that takes a value from a line, if it does:
    /// the one extraction valueOf() and matchOf() share.
    static std::optional<Matches> matchesOf( const Rule& rule, const QString& line );

    /// Compile a rule. Its patterns are compiled on first use, and once:
    /// RuleCheck::problemsOf() tells from them whether it can be used.
    static Rule compile( const FooterEntry& entry, int index );

    /// The value a rule extracts from a line, if it matches, and where. Only
    /// the preview needs where, once per preview. The caller sets the value's
    /// line number.
    static std::optional<Match> matchOf( const Rule& rule, const QString& line );

    /// The value a rule extracts from a line, if it matches; the scan's hot
    /// path, so it computes nothing else. The caller sets the value's line
    /// number.
    static std::optional<FooterValue> valueOf( const Rule& rule, const QString& line );

    /// Whether the file is still the one the progress was made on.
    static bool continues( QFile& file, const Progress& from );

    /// Whether the file still holds what @p read covers, and, unless
    /// appended lines are beyond what was read (@p beyondRead), has
    /// @p size still and nothing after what was read: bytes that were not
    /// remembered, such as a last line without a line break, may have
    /// changed in place.
    static bool unchangedSince( const QString& filePath, const Progress& read, bool beyondRead,
                                qint64 size );

    QList<Rule> rules_;
    QStringList keys_; ///< Distinct keys, in the order of their first rule.
    QStringList problems_;
};

} // namespace custom_footer
