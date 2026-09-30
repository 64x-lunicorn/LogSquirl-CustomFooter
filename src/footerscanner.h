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
 * order wins. An enabled rule without a line pattern is incomplete and
 * skipped; one with a line pattern but without a key, or with an invalid
 * pattern, is skipped and reported in problems().
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

    /// How far a file has been scanned, to continue there once it has grown.
    struct Progress {
        /// End of the last complete line scanned, or where the scan limit
        /// stopped the scan inside a line.
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
    };

    /// What one rule finds in a file, for the editor's live preview.
    struct Preview {
        enum class Status {
            Scanned,        ///< The file was scanned; see the fields below.
            NoFile,         ///< No file was given.
            NoLinePattern,  ///< The rule has no line pattern, so it finds nothing.
            InvalidPattern, ///< A pattern of the rule does not compile; see error.
            Unreadable,     ///< The file could not be opened.
            Cancelled,      ///< The preview was cancelled; nothing else is set.
        };
        Status status = Status::NoFile;
        QString error;       ///< Why a pattern is invalid.
        QString filePath;    ///< The file previewed.
        QString key;         ///< The rule's key.
        bool enabled = true; ///< Whether the rule is enabled; a disabled one supplies nothing.

        /// The rule's first match, if any: the value the footer would show
        /// from it, and the line it is in.
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
        /// whichever of its rules supplies it, and the line it is in.
        std::optional<FooterValue> keyValue;
        qint64 keyLineNumber = 0;

        bool limitReached() const
        {
            return lineLimitReached || byteLimitReached;
        }
    };

    explicit FooterScanner( const QList<FooterEntry>& entries );

    /// One message per skipped rule, in rule order.
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
     * A cancelled scan returns no values and empty progress.
     */
    Scan scanFrom( const QString& filePath, const Progress& from, int maxLines = kDefaultMaxLines,
                   const std::atomic_bool* cancelled = nullptr ) const;

    /**
     * Preview what the rule at @p rule of @p entries finds in a file, with
     * the matching of scanFile() and its limits.
     *
     * Unlike a scan, the preview reads on to the limits after the first
     * match, to count the matching lines. The rule is previewed even when
     * disabled; which rule supplies its key's value is decided among the
     * enabled rules with that key, as the footer does.
     *
     * @param cancelled  Checked per line, and while skipping over-long lines.
     */
    static Preview preview( const QString& filePath, const QList<FooterEntry>& entries, int rule,
                            int maxLines = kDefaultMaxLines,
                            const std::atomic_bool* cancelled = nullptr );

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

    /// Compile a rule; empty and with @p problem set if it cannot be used.
    static std::optional<Rule> compile( const FooterEntry& entry, int index, QString* problem );

    /// The value a rule extracts from a line, if it matches, and where.
    static std::optional<Match> matchOf( const Rule& rule, const QString& line );

    /// The value a rule extracts from a line, if it matches.
    static std::optional<FooterValue> valueOf( const Rule& rule, const QString& line );

    /// Whether the file is still the one the progress was made on.
    static bool continues( QFile& file, const Progress& from );

    QList<Rule> rules_;
    QStringList keys_; ///< Distinct keys, in the order of their first rule.
    QStringList problems_;
};

} // namespace custom_footer
