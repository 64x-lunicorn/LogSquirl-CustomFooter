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

#include <QList>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <atomic>
#include <optional>

namespace custom_footer {

/**
 * Scans a log file and extracts the first match per enabled FooterEntry.
 *
 * The rules are compiled once on construction. A rule is skipped, and
 * reported in problems(), if one of its patterns is invalid or if an earlier
 * enabled rule already uses its key: per key, the first rule in list order
 * wins.
 */
class FooterScanner {
public:
    static constexpr int kDefaultMaxLines = 100000;

    /// Longer lines are matched against their first kMaxLineBytes only.
    static constexpr qint64 kMaxLineBytes = 64 * 1024;

    /// A scan stops after this many bytes, whatever maxLines says.
    static constexpr qint64 kMaxScanBytes = 64 * 1024 * 1024;

    explicit FooterScanner( const QList<FooterEntry>& entries );

    /// One message per skipped rule, in rule order.
    const QStringList& problems() const
    {
        return problems_;
    }

    /**
     * Scan a log file for matching key-value pairs.
     *
     * For each rule, the first line its linePattern matches (and its
     * valuePattern, if set) provides the value: the first capturing group, or
     * the full match without one. Mappings are then applied.
     *
     * @param filePath   Path to the log file.
     * @param maxLines   Maximum number of lines to scan (0 = unlimited).
     * @param cancelled  Checked per line; when set, the scan returns nothing.
     * @return Map from entry key to matched value.
     */
    QMap<QString, QString> scanFile( const QString& filePath, int maxLines = kDefaultMaxLines,
                                     const std::atomic_bool* cancelled = nullptr ) const;

    /// The given values as key-value pairs, in rule order.
    QList<QPair<QString, QString>> inRuleOrder( const QMap<QString, QString>& values ) const;

    /// Compile the entries and scan a file in one go.
    static QMap<QString, QString> scan( const QString& filePath, const QList<FooterEntry>& entries,
                                        int maxLines = kDefaultMaxLines );

    /// Why a pattern does not compile, or an empty string if it does.
    static QString patternError( const QString& pattern );

private:
    struct Rule {
        QString key;
        QRegularExpression lineRegex;
        QRegularExpression valueRegex; // empty pattern = not used
        QList<ValueMapping> mappings;
    };

    /// The value a rule extracts from a line, if it matches.
    std::optional<QString> valueOf( const Rule& rule, const QString& line ) const;

    QList<Rule> rules_;
    QStringList problems_;
};

} // namespace custom_footer
