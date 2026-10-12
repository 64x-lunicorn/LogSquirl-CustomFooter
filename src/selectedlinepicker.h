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

#include "rulefromline.h"

#include <QGroupBox>
#include <QString>

class QLabel;
class QLineEdit;

namespace custom_footer {

/**
 * The Log Line a rule is made from, for the user to click or mark its value
 * in, with a hint above it that says what to do and what was found.
 *
 * The line is read-only. Every click into it and every mark emits marked();
 * the owner makes the rule and shows the result with showRule().
 */
class SelectedLinePicker : public QGroupBox {
    Q_OBJECT

public:
    explicit SelectedLinePicker( QWidget* parent = nullptr );

    /// Show @p line, nothing marked yet. @p selectedLines is how many Log
    /// Lines were selected, of which this is the first. Of a long line only
    /// the part the footer reads is shown (footerReadLength()), with a note
    /// that the rest is cut off: the rest can make no rule, and a QLineEdit
    /// lays out and paints its whole text on every click.
    void showLine( const QString& line, int selectedLines = 1 );

    /// The line as shown: the start of the given line, at the same
    /// positions, so marks in it are marks in that line.
    QString line() const;

    /// Show what the last mark made: the value and the text before it, or
    /// why it makes no rule.
    void showRule( const RuleFromLine& rule );

    /// Show that the last mark was not taken, and why. The same mark again
    /// is emitted again.
    void showMarkIgnored( const QString& reason );

    /// Add to the hint that in the active file Log Line @p lineNumber
    /// matches first, so the footer shows its value @p value rather than
    /// the shown line's. Until the hint changes otherwise.
    void showEarlierMatch( qint64 lineNumber, const QString& value );

    /// Put the focus into the line, to click or mark the value.
    void focusLine();

Q_SIGNALS:
    /// The user clicked into the line (@p length 0) or marked @p length
    /// UTF-16 units of it at @p start. The same mark twice in a row is
    /// emitted once.
    void marked( qsizetype start, qsizetype length );

private:
    void markChanged();
    /// Show @p hint, without a note of an earlier match.
    void setHint( const QString& hint );

    QLabel* hint_ = nullptr;
    QLineEdit* line_ = nullptr;
    /// Says that only the start of a long line is shown.
    QLabel* cutNote_ = nullptr;
    /// The hint without a note of an earlier match.
    QString hintText_;
    /// The last mark emitted, to emit each once.
    qsizetype markStart_ = -1;
    qsizetype markLength_ = -1;
};

} // namespace custom_footer
