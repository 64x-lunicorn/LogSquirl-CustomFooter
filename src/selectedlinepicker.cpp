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

#include "selectedlinepicker.h"

#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <limits>

namespace custom_footer {

SelectedLinePicker::SelectedLinePicker( QWidget* parent )
    : QGroupBox( tr( "Selected Log Line" ), parent )
{
    setObjectName( "selectedLinePicker" );
    auto* layout = new QVBoxLayout( this );

    hint_ = new QLabel( this );
    hint_->setObjectName( "selectedLineHint" );
    // Log content is shown as it is, never as rich text.
    hint_->setTextFormat( Qt::PlainText );
    hint_->setWordWrap( true );
    layout->addWidget( hint_ );

    line_ = new QLineEdit( this );
    line_->setObjectName( "selectedLineEdit" );
    line_->setReadOnly( true );
    // Not the default 32767 characters: the host hands over up to 1 MiB.
    line_->setMaxLength( std::numeric_limits<int>::max() );
    line_->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
    line_->setToolTip( tr( "Click where the value starts, or mark the value" ) );
    layout->addWidget( line_ );

    // A click moves the cursor; a mark changes the selection, and the
    // cursor with it. Either way the mark is read as it is then.
    connect( line_, &QLineEdit::cursorPositionChanged, this, &SelectedLinePicker::markChanged );
    connect( line_, &QLineEdit::selectionChanged, this, &SelectedLinePicker::markChanged );
}

void SelectedLinePicker::showLine( const QString& line, int selectedLines )
{
    {
        const QSignalBlocker blocker( line_ );
        line_->setText( line );
        line_->setCursorPosition( 0 );
    }
    markStart_ = -1;
    markLength_ = -1;
    lineHint_ = tr( "Click where the value starts in this Log Line, or mark the value." );
    if ( selectedLines > 1 ) {
        lineHint_ += QLatin1Char( ' ' )
                     + tr( "%1 Log Lines are selected: the rule is made from the first." )
                           .arg( selectedLines );
    }
    hint_->setText( lineHint_ );
}

QString SelectedLinePicker::line() const
{
    return line_->text();
}

void SelectedLinePicker::showRule( const std::optional<RuleFromLine>& rule )
{
    if ( !rule ) {
        hint_->setText( tr( "No simple rule finds this value. Mark a value that has text before "
                            "it, or edit the patterns under Advanced." ) );
        return;
    }
    hint_->setText( tr( "Value: “%1”, after “%2”. Click or mark again to change it." )
                        .arg( rule->value, rule->rule.textBefore ) );
}

void SelectedLinePicker::focusLine()
{
    line_->setFocus();
}

void SelectedLinePicker::markChanged()
{
    const bool selected = line_->hasSelectedText();
    const qsizetype start = selected ? line_->selectionStart() : line_->cursorPosition();
    const qsizetype length = selected ? line_->selectionLength() : 0;
    if ( start == markStart_ && length == markLength_ ) {
        return;
    }
    markStart_ = start;
    markLength_ = length;
    Q_EMIT marked( start, length );
}

} // namespace custom_footer
