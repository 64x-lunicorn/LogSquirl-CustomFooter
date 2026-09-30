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

#include "footerscanner.h"

#include <QGroupBox>

class QLabel;
class QTextEdit;

namespace custom_footer {

/**
 * The live preview section of the rule detail panel: what the selected rule
 * finds in the active file.
 *
 * It shows the first matching line with the line pattern's match and the
 * captured value highlighted, the raw value and the value after mappings,
 * the number of matching lines within the scan limits, and for a key that
 * other rules share, which rule supplies the key's value in the file.
 * Without a file, or for a rule that cannot match, it explains why instead.
 */
class RulePreviewView : public QGroupBox {
    Q_OBJECT

public:
    /// Characters shown around the match of a long line.
    static constexpr int kContextChars = 200;

    explicit RulePreviewView( QWidget* parent = nullptr );

    /// Show a preview, replacing the one shown.
    void showPreview( const custom_footer::FooterScanner::Preview& preview );

    /// Mark the shown preview as outdated until the next one is shown.
    void showUpdating();

    /// Show that no rule is selected.
    void showNoRule();

    /// The number of matching lines, and how much of the file they are in.
    static QString countText( const FooterScanner::Preview& preview );

    /// The backgrounds highlighting the line pattern's match, and the value,
    /// in the shown line.
    static QColor lineMatchColor();
    static QColor valueColor();

private:
    void showMessage( const QString& message );
    void showLine( const FooterScanner::Preview& preview );

    QLabel* message_ = nullptr;
    QLabel* updating_ = nullptr;
    QLabel* location_ = nullptr;
    QTextEdit* line_ = nullptr;
    QLabel* value_ = nullptr;
    QLabel* count_ = nullptr;
    QLabel* key_ = nullptr;
};

} // namespace custom_footer
