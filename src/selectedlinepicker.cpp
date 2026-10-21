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
#include "elidedtext.h"

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

    cutNote_ = new QLabel(
        tr( "Only the first 64 KiB of this Log Line are shown: the footer reads no further." ),
        this );
    cutNote_->setObjectName( "selectedLineCutNote" );
    cutNote_->setWordWrap( true );
    cutNote_->hide();
    layout->addWidget( cutNote_ );

    // A click moves the cursor; a mark changes the selection, and the
    // cursor with it. Either way the mark is read as it is then.
    connect( line_, &QLineEdit::cursorPositionChanged, this, &SelectedLinePicker::markChanged );
    connect( line_, &QLineEdit::selectionChanged, this, &SelectedLinePicker::markChanged );
}

void SelectedLinePicker::showLine( const QString& line, int selectedLines,
                                   const FooterScanner::Encoding& encoding )
{
    encoding_ = encoding;
    {
        const QSignalBlocker blocker( line_ );
        const auto shown = footerReadLength( line, encoding );
        line_->setText( line.left( shown ) );
        line_->setCursorPosition( 0 );
        cutNote_->setVisible( shown < line.size() );
    }
    markStart_ = -1;
    markLength_ = -1;
    earlierMatch_.clear();
    auto hint = tr( "Click where the value starts in this Log Line, or mark the value." );
    if ( selectedLines > 1 ) {
        hint += QLatin1Char( ' ' )
                + tr( "%1 Log Lines are selected: the rule is made from the first." )
                      .arg( selectedLines );
    }
    setHint( hint );
}

QString SelectedLinePicker::line() const
{
    return line_->text();
}

FooterScanner::Encoding SelectedLinePicker::encoding() const
{
    return encoding_;
}

void SelectedLinePicker::showRule( const RuleFromLine& rule )
{
    switch ( rule.problem ) {
    case MarkProblem::None:
        setHint( tr( "Value: “%1”, after “%2”. Click or mark again to change it." )
                     .arg( elidedText( rule.value ), elidedText( rule.rule.textBefore ) ) );
        return;
    case MarkProblem::NothingMarked:
        setHint( tr( "Click where the value starts in this Log Line, or mark the value." ) );
        return;
    case MarkProblem::NoTextBefore:
        setHint( tr( "This value starts the line, but a simple rule needs text before the "
                     "value. Mark another value, or write an advanced rule." ) );
        return;
    case MarkProblem::BeyondLineLimit:
        setHint( tr( "The footer reads only the first 64 KiB of a line, and this value lies "
                     "beyond them. Mark a value nearer the start of the line." ) );
        return;
    case MarkProblem::TextBeforeEarlier:
        setHint( tr( "The text before this value, “%1”, also appears earlier in the line, so "
                     "the rule would find the earlier value. Mark a value whose text before it "
                     "is unique, or write an advanced rule." )
                     .arg( elidedText( rule.rule.textBefore ) ) );
        return;
    case MarkProblem::NoSimpleRule:
        setHint( tr( "No simple rule finds exactly this value. Mark another value, or write an "
                     "advanced rule." ) );
        return;
    }
}

void SelectedLinePicker::showMarkIgnored( const QString& reason )
{
    markStart_ = -1;
    markLength_ = -1;
    setHint( reason );
}

void SelectedLinePicker::showRuleEdited()
{
    setHint( tr( "Click where the value starts in this Log Line, or mark the value, to make the "
                 "rule from the line again." ) );
}

void SelectedLinePicker::showEarlierMatch( qint64 lineNumber, const QString& value )
{
    earlierMatch_ = tr( "In the active file, Log Line %1 matches first: the footer shows its "
                        "value “%2”, not this line's." )
                        .arg( lineNumber )
                        .arg( elidedText( value ) );
    updateHint();
}

void SelectedLinePicker::clearEarlierMatch()
{
    earlierMatch_.clear();
    updateHint();
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

void SelectedLinePicker::setHint( const QString& hint )
{
    hintText_ = hint;
    updateHint();
}

void SelectedLinePicker::updateHint()
{
    hint_->setText( earlierMatch_.isEmpty() ? hintText_
                                            : hintText_ + QLatin1Char( ' ' ) + earlierMatch_ );
}

} // namespace custom_footer
