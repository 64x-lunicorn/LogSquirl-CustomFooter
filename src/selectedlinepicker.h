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
 * The line is read-only. Every click into it and every mark emits marked(),
 * with the mark's start and length in UTF-16 units (0 for a click); the
 * same mark twice in a row is emitted once. The owner makes the rule and
 * shows the result with showRule().
 */
class SelectedLinePicker : public QGroupBox {
    Q_OBJECT

public:
    explicit SelectedLinePicker( QWidget* parent = nullptr );

    /// Show @p line, nothing marked yet. @p selectedLines is how many Log
    /// Lines were selected, of which this is the first.
    void showLine( const QString& line, int selectedLines = 1 );

    /// The line shown.
    QString line() const;

    /// Show what the last mark made: the value and the text before it, or
    /// why it makes no rule.
    void showRule( const RuleFromLine& rule );

    /// Put the focus into the line, to click or mark the value.
    void focusLine();

Q_SIGNALS:
    void marked( qsizetype start, qsizetype length );

private:
    void markChanged();

    QLabel* hint_ = nullptr;
    QLineEdit* line_ = nullptr;
    /// What showLine() says, before any mark.
    QString lineHint_;
    /// The last mark emitted, to emit each once.
    qsizetype markStart_ = -1;
    qsizetype markLength_ = -1;
};

} // namespace custom_footer
