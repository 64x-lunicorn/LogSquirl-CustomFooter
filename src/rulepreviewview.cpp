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

#include "rulepreviewview.h"

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace custom_footer {

namespace {

QString quoted( const QString& text )
{
    return QStringLiteral( "“%1”" ).arg( text );
}

QLabel* addLabel( QWidget* parent, QVBoxLayout* layout, const char* objectName )
{
    auto* label = new QLabel( parent );
    label->setObjectName( objectName );
    // Log content is shown as it is, never as rich text.
    label->setTextFormat( Qt::PlainText );
    label->setWordWrap( true );
    label->setTextInteractionFlags( Qt::TextSelectableByMouse );
    layout->addWidget( label );
    return label;
}

} // namespace

RulePreviewView::RulePreviewView( QWidget* parent )
    : QGroupBox( tr( "Preview" ), parent )
{
    setObjectName( "rulePreview" );
    auto* layout = new QVBoxLayout( this );

    updating_ = addLabel( this, layout, "previewUpdating" );
    updating_->setText( tr( "Updating…" ) );
    updating_->setEnabled( false );
    message_ = addLabel( this, layout, "previewMessage" );
    location_ = addLabel( this, layout, "previewLocation" );

    line_ = new QTextEdit( this );
    line_->setObjectName( "previewLine" );
    line_->setReadOnly( true );
    line_->setAcceptRichText( false );
    line_->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
    line_->setLineWrapMode( QTextEdit::WidgetWidth );
    line_->setWordWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
    line_->setMaximumHeight( line_->fontMetrics().lineSpacing() * 4 + 2 * line_->frameWidth()
                             + static_cast<int>( line_->document()->documentMargin() * 2 ) );
    layout->addWidget( line_ );

    value_ = addLabel( this, layout, "previewValue" );
    count_ = addLabel( this, layout, "previewCount" );
    key_ = addLabel( this, layout, "previewKey" );

    showNoRule();
}

QColor RulePreviewView::lineMatchColor()
{
    return QColor( 255, 190, 0, 90 );
}

QColor RulePreviewView::valueColor()
{
    return QColor( 0, 150, 255, 120 );
}

void RulePreviewView::showNoRule()
{
    showMessage( tr( "Select a rule to preview what it finds in the active file." ) );
}

void RulePreviewView::showUpdating()
{
    updating_->show();
}

void RulePreviewView::showMessage( const QString& message )
{
    updating_->hide();
    message_->setText( message );
    message_->setVisible( !message.isEmpty() );
    for ( auto* label : { location_, value_, count_, key_ } ) {
        label->clear();
        label->hide();
    }
    line_->clear();
    line_->hide();
}

void RulePreviewView::showPreview( const FooterScanner::Preview& preview )
{
    using Status = FooterScanner::Preview::Status;

    const auto fileName = QFileInfo( preview.filePath ).fileName();
    setTitle( fileName.isEmpty() ? tr( "Preview" ) : tr( "Preview in %1" ).arg( fileName ) );
    setToolTip( QDir::toNativeSeparators( preview.filePath ) );

    switch ( preview.status ) {
    case Status::NoFile:
        showMessage( tr( "Open a log file in LogSquirl to preview what this rule finds in it." ) );
        return;
    case Status::NoLinePattern:
        showMessage( tr( "Enter a line pattern to preview what this rule finds." ) );
        return;
    case Status::InvalidPattern:
        showMessage( tr( "This rule cannot match anything until its %1 is fixed: %2" )
                         .arg( preview.error.section( QStringLiteral( ": " ), 0, 0 ),
                               preview.error.section( QStringLiteral( ": " ), 1 ) ) );
        return;
    case Status::Unreadable:
        showMessage( tr( "The active file %1 cannot be read." )
                         .arg( QDir::toNativeSeparators( preview.filePath ) ) );
        return;
    case Status::Cancelled:
        return;
    case Status::Scanned:
        break;
    }

    QStringList notes;
    if ( !preview.enabled ) {
        notes.append( tr( "This rule is disabled: the footer does not use it." ) );
    }
    if ( !preview.value ) {
        notes.append( preview.limitReached()
                          ? tr( "No line matches this rule within the scan limits." )
                          : tr( "No line of the file matches this rule." ) );
    }
    showMessage( notes.join( '\n' ) );

    if ( preview.value ) {
        location_->setText( tr( "First match, line %L1:" ).arg( preview.lineNumber ) );
        location_->show();
        showLine( preview );

        const auto& value = *preview.value;
        value_->setText( tr( "Raw value: %1" ).arg( quoted( value.rawValue ) ) + '\n'
                         + ( value.isMapped()
                                 ? tr( "After mappings: %1" ).arg( quoted( value.value ) )
                                 : tr( "After mappings: %1, no mapping applies" )
                                       .arg( quoted( value.value ) ) ) );
        value_->show();
    }

    count_->setText( countText( preview ) );
    count_->show();

    if ( preview.sharedKey ) {
        const auto& supplier = preview.keyValue;
        QString text;
        if ( !supplier ) {
            text = tr( "No rule for %1 finds a value in this file." ).arg( quoted( preview.key ) );
        }
        else if ( preview.value && supplier->rule == preview.value->rule ) {
            text = tr( "This rule supplies %1 in this file; the other rules for the key are "
                       "not used here." )
                       .arg( quoted( preview.key ) );
        }
        else {
            text = tr( "Rule %1 supplies %2 in this file instead, from line %L3: %4" )
                       .arg( supplier->rule + 1 )
                       .arg( quoted( preview.key ) )
                       .arg( preview.keyLineNumber )
                       .arg( quoted( supplier->value ) );
        }
        key_->setText( text );
        key_->show();
    }
}

QString RulePreviewView::countText( const FooterScanner::Preview& preview )
{
    const auto matches = preview.matches == 1 ? tr( "1 matching line" )
                                              : tr( "%L1 matching lines" ).arg( preview.matches );
    if ( preview.lineLimitReached ) {
        return tr( "%1 in the first %L2 lines: the scan stops at this line limit." )
            .arg( matches )
            .arg( preview.lines );
    }
    if ( preview.byteLimitReached ) {
        return tr( "%1 in the first %L2 MiB (%L3 lines): the scan stops at this size limit." )
            .arg( matches )
            .arg( FooterScanner::kMaxScanBytes / ( 1024 * 1024 ) )
            .arg( preview.lines );
    }
    return preview.lines == 1
               ? tr( "%1 in the file's only line." ).arg( matches )
               : tr( "%1 in all %L2 lines of the file." ).arg( matches ).arg( preview.lines );
}

void RulePreviewView::showLine( const FooterScanner::Preview& preview )
{
    const auto& line = preview.line;
    const auto matchStart = preview.lineMatchStart;
    const auto matchEnd = matchStart + preview.lineMatchLength;
    const auto valueStart = preview.valueStart;
    const auto valueEnd = valueStart < 0 ? valueStart : valueStart + preview.valueLength;

    // Only the part around the highlights of a long line.
    auto first = matchStart;
    auto last = matchEnd;
    if ( valueStart >= 0 ) {
        first = std::min( first, valueStart );
        last = std::max( last, valueEnd );
    }
    qsizetype from = std::max<qsizetype>( 0, first - kContextChars );
    qsizetype to = std::min<qsizetype>( line.size(), last + kContextChars );
    // Never cut a character in two: move the cuts out to whole code points.
    if ( from > 0 && line.at( from ).isLowSurrogate() && line.at( from - 1 ).isHighSurrogate() ) {
        --from;
    }
    if ( to > 0 && to < line.size() && line.at( to - 1 ).isHighSurrogate()
         && line.at( to ).isLowSurrogate() ) {
        ++to;
    }

    std::vector<qsizetype> bounds = { from, to, matchStart, matchEnd };
    if ( valueStart >= 0 ) {
        bounds.push_back( valueStart );
        bounds.push_back( valueEnd );
    }
    for ( auto& bound : bounds ) {
        bound = std::clamp( bound, from, to );
    }
    std::sort( bounds.begin(), bounds.end() );
    bounds.erase( std::unique( bounds.begin(), bounds.end() ), bounds.end() );

    QTextCharFormat plain;
    QTextCharFormat inMatch;
    inMatch.setBackground( lineMatchColor() );
    QTextCharFormat inValue;
    inValue.setBackground( valueColor() );
    inValue.setFontWeight( QFont::Bold );

    line_->clear();
    QTextCursor cursor( line_->document() );
    if ( from > 0 ) {
        cursor.insertText( QStringLiteral( "…" ), plain );
    }
    for ( size_t i = 0; i + 1 < bounds.size(); ++i ) {
        const auto start = bounds[ i ];
        const auto end = bounds[ i + 1 ];
        const bool value = valueStart >= 0 && start >= valueStart && end <= valueEnd;
        const bool match = start >= matchStart && end <= matchEnd;
        cursor.insertText( line.mid( start, end - start ),
                           value ? inValue : ( match ? inMatch : plain ) );
    }
    if ( to < line.size() ) {
        cursor.insertText( QStringLiteral( "…" ), plain );
    }
    line_->show();
}

} // namespace custom_footer
