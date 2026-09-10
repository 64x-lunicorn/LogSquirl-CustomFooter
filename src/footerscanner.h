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
#include <QString>

namespace custom_footer {

/// Scans a log file and extracts the first match per enabled FooterEntry.
class FooterScanner {
  public:
    /**
     * Scan a log file for matching key-value pairs.
     *
     * For each enabled entry, searches line by line using its regex.
     * The first capturing group of the first match is used as the value.
     * If the regex has no capturing groups, the full match is used.
     *
     * @param filePath  Path to the log file.
     * @param entries   List of extraction rules.
     * @param maxLines  Maximum number of lines to scan (0 = unlimited).
     * @return Map from entry key to matched value.
     */
    static QMap<QString, QString> scan( const QString& filePath,
                                        const QList<FooterEntry>& entries,
                                        int maxLines = 100000 );
};

} // namespace custom_footer
