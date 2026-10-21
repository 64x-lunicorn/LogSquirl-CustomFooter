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

#include "footerdisplaywidget.h"

#include <QAccessible>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QToolTip>

#include <algorithm>
#include <utility>

namespace custom_footer {

namespace {

QString keyAndValue( const FooterValue& value )
{
    return QStringLiteral( "%1: %2" ).arg( value.key, value.value );
}

/// The key and the value as shown, for the accessible name.
QString shownKeyAndValue( const FooterValue& value )
{
    return QStringLiteral( "%1: %2" ).arg( value.key, elidedText( value.value ) );
}

/// Keys a focused value handles itself, before the window's shortcuts.
bool isCopyKey( const QKeyEvent* event )
{
    if ( event->matches( QKeySequence::Copy ) ) {
        return true;
    }
    if ( ( event->modifiers() & ~Qt::KeypadModifier ) != Qt::NoModifier ) {
        return false;
    }
    return event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
           || event->key() == Qt::Key_Space;
}

/// Button text shows "&" as a mnemonic marker unless it is doubled.
QString withoutMnemonics( QString text )
{
    return text.replace( QLatin1Char( '&' ), QStringLiteral( "&&" ) );
}

QString toolTipOf( const FooterValue& value, bool canGoToLine )
{
    QStringList lines;
    lines.append(
        QStringLiteral( "<b>%1:</b> %2" )
            .arg( value.key.toHtmlEscaped(), elidedText( value.value ).toHtmlEscaped() ) );
    if ( value.isMapped() ) {
        lines.append( FooterValueItem::tr( "Raw value: %1" )
                          .arg( elidedText( value.rawValue ).toHtmlEscaped() ) );
    }
    if ( value.rule >= 0 ) {
        lines.append( FooterValueItem::tr( "From rule %1" ).arg( value.rule + 1 ) );
    }
    if ( value.lineNumber > 0 ) {
        lines.append( FooterValueItem::tr( "Found in Log Line %1" ).arg( value.lineNumber ) );
    }
    lines.append( QStringLiteral( "<i>%1</i>" ).arg( FooterValueItem::tr( "Click to copy" ) ) );
    if ( canGoToLine ) {
        lines.append( QStringLiteral( "<i>%1</i>" )
                          .arg( FooterValueItem::tr( "Double-click to go to the Log Line" ) ) );
    }
    return lines.join( QStringLiteral( "<br>" ) );
}

/// What the tooltip adds to the accessible name, as plain text.
QString descriptionOf( const FooterValue& value, bool canGoToLine )
{
    QStringList sentences;
    if ( value.isMapped() ) {
        sentences.append(
            FooterValueItem::tr( "Raw value: %1." ).arg( elidedText( value.rawValue ) ) );
    }
    if ( value.rule >= 0 ) {
        sentences.append( FooterValueItem::tr( "From rule %1." ).arg( value.rule + 1 ) );
    }
    if ( value.lineNumber > 0 ) {
        sentences.append( FooterValueItem::tr( "Found in Log Line %1." ).arg( value.lineNumber ) );
    }
    sentences.append( FooterValueItem::tr( "Press Space to copy." ) );
    if ( canGoToLine ) {
        sentences.append( FooterValueItem::tr( "Open the context menu to go to the Log Line." ) );
    }
    return sentences.join( QLatin1Char( ' ' ) );
}

} // namespace

FooterValueItem::FooterValueItem( const FooterValue& value, QWidget* parent )
    : QToolButton( parent )
    , contextMenu_( new QMenu( this ) )
{
    setAutoRaise( true );
    setToolButtonStyle( Qt::ToolButtonTextOnly );
    setFocusPolicy( Qt::StrongFocus );
    setCursor( Qt::PointingHandCursor );
    connect( this, &QToolButton::clicked, this, &FooterValueItem::copyToClipboard );

    // A menu of the button itself would make it a menu button; this one is
    // only shown on request. Going to the Log Line comes first, where offered.
    goToLine_
        = contextMenu_->addAction( tr( "Go to Log Line" ), this, [ this ] { requestGoToLine(); } );
    goToLine_->setObjectName( QStringLiteral( "goToLogLine" ) );
    goToLine_->setVisible( false );
    contextMenu_->addAction( tr( "Copy Value" ), this, &FooterValueItem::copyToClipboard )
        ->setObjectName( QStringLiteral( "copyValue" ) );
    contextMenu_
        ->addAction( tr( "Copy Key and Value" ), this,
                     [ this ] { copyText( keyAndValue( value_ ) ); } )
        ->setObjectName( QStringLiteral( "copyKeyAndValue" ) );
    setContextMenuPolicy( Qt::CustomContextMenu );
    connect( this, &QWidget::customContextMenuRequested, this,
             [ this ]( const QPoint& pos ) { contextMenu_->popup( mapToGlobal( pos ) ); } );

    setValue( value );
}

void FooterValueItem::setValue( const FooterValue& value )
{
    value_ = value;
    // Shown elided, but copied in full.
    setText( withoutMnemonics( elidedText( value.value ) ) );
    setAccessibleName( shownKeyAndValue( value ) );
    updateOffers();
}

void FooterValueItem::setGoToLineOffered( bool offered )
{
    goToLineOffered_ = offered;
    updateOffers();
}

bool FooterValueItem::canGoToLine() const
{
    return goToLineOffered_ && value_.lineNumber > 0;
}

void FooterValueItem::updateOffers()
{
    const bool canGo = canGoToLine();
    setToolTip( toolTipOf( value_, canGo ) );
    setAccessibleDescription( descriptionOf( value_, canGo ) );
    goToLine_->setVisible( canGo );
}

void FooterValueItem::requestGoToLine()
{
    if ( canGoToLine() ) {
        emit goToLineRequested();
    }
}

void FooterValueItem::copyToClipboard()
{
    copyText( value_.value );
}

void FooterValueItem::copyText( const QString& text )
{
    QGuiApplication::clipboard()->setText( text );
    showNotice( tr( "Copied" ) );
}

void FooterValueItem::showNotice( const QString& message )
{
    // Show it at the value for a moment. The tooltip is not given this item:
    // it would become the item's child, and go away with the item even when
    // it shows another widget's tooltip by then, e.g. one of the host.
    QToolTip::showText( mapToGlobal( rect().center() ), message, nullptr, {}, kConfirmationMs );

#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
    QAccessibleAnnouncementEvent announcement( this, message );
    QAccessible::updateAccessibility( &announcement );
#endif
}

bool FooterValueItem::event( QEvent* event )
{
    // Claim the copy keys before a window shortcut, e.g. the host's Edit >
    // Copy, takes them; they then arrive in keyPressEvent().
    if ( event->type() == QEvent::ShortcutOverride
         && isCopyKey( static_cast<QKeyEvent*>( event ) ) ) {
        event->accept();
        return true;
    }
    return QToolButton::event( event );
}

void FooterValueItem::keyPressEvent( QKeyEvent* event )
{
    if ( event->key() != Qt::Key_Space && isCopyKey( event ) ) {
        copyToClipboard();
        event->accept();
        return;
    }
    // Space clicks the button, and so copies too.
    QToolButton::keyPressEvent( event );
}

void FooterValueItem::mouseDoubleClickEvent( QMouseEvent* event )
{
    // Its first click has copied the value already. Taken here, the second
    // press does not press the button again, so it neither copies a second
    // time nor makes a click of its own.
    if ( event->button() == Qt::LeftButton && canGoToLine() ) {
        event->accept();
        emit goToLineRequested();
        return;
    }
    QToolButton::mouseDoubleClickEvent( event );
}

FooterDisplayWidget::FooterDisplayWidget( QWidget* parent )
    : QWidget( parent )
    , layout_( new QHBoxLayout( this ) )
{
    layout_->setContentsMargins( 4, 0, 4, 0 );
    layout_->setSpacing( 0 );
    layout_->addStretch();
}

FooterDisplayWidget::Entry FooterDisplayWidget::createEntry( const FooterValue& value )
{
    Entry entry;
    entry.box = new QWidget( this );
    auto* row = new QHBoxLayout( entry.box );
    row->setContentsMargins( 0, 0, 0, 0 );
    row->setSpacing( 2 );

    // No buddy: it would make an "&" in the key a mnemonic.
    entry.key = new QLabel( entry.box );
    entry.key->setTextFormat( Qt::RichText );
    entry.key->setText( QStringLiteral( "<b>%1:</b>" ).arg( value.key.toHtmlEscaped() ) );
    row->addWidget( entry.key );

    entry.item = new FooterValueItem( value, entry.box );
    entry.item->setGoToLineOffered( goToLineOffered_ );
    connect( entry.item, &FooterValueItem::goToLineRequested, this,
             [ this, item = entry.item ] { emit goToLineRequested( item->value() ); } );
    entry.item->contextMenu()
        ->addAction( tr( "Copy All" ), entry.item,
                     [ this, item = entry.item ] { item->copyText( allValuesText() ); } )
        ->setObjectName( QStringLiteral( "copyAll" ) );
    row->addWidget( entry.item );
    return entry;
}

void FooterDisplayWidget::updateValues( const QList<FooterValue>& values )
{
    // Keep the entry of every key still shown, with its focus, confirmation
    // and context menu.
    auto old = entries_;
    QList<Entry> entries;
    for ( const auto& value : values ) {
        const auto same = std::find_if( old.begin(), old.end(), [ &value ]( const Entry& entry ) {
            return entry.item->value().key == value.key;
        } );
        if ( same == old.end() ) {
            entries.append( createEntry( value ) );
            continue;
        }
        if ( same->item->value() != value ) {
            same->item->setValue( value );
        }
        entries.append( *same );
        old.erase( same );
    }
    for ( const auto& gone : old ) {
        delete gone.box;
    }
    entries_ = entries;

    // Lay the entries out again in the new order, with separators between.
    qDeleteAll( separators_ );
    separators_.clear();
    for ( const auto& entry : entries_ ) {
        layout_->removeWidget( entry.box );
    }
    int position = 0;
    for ( const auto& entry : entries_ ) {
        if ( position > 0 ) {
            auto* separator = new QLabel( QStringLiteral( "|" ), this );
            separator->setContentsMargins( 6, 0, 6, 0 );
            separators_.append( separator );
            layout_->insertWidget( position++, separator );
        }
        layout_->insertWidget( position++, entry.box );
    }

    // Tab follows the order shown, not the order the items were made in.
    for ( int i = 1; i < entries_.size(); ++i ) {
        setTabOrder( entries_[ i - 1 ].item, entries_[ i ].item );
    }
}

void FooterDisplayWidget::clearValues()
{
    updateValues( {} );
}

QList<FooterValue> FooterDisplayWidget::values() const
{
    QList<FooterValue> shown;
    for ( const auto& entry : entries_ ) {
        shown.append( entry.item->value() );
    }
    return shown;
}

void FooterDisplayWidget::setGoToLineOffered( bool offered )
{
    goToLineOffered_ = offered;
    for ( const auto& entry : std::as_const( entries_ ) ) {
        entry.item->setGoToLineOffered( offered );
    }
}

void FooterDisplayWidget::notify( const QString& key, const QString& message )
{
    for ( const auto& entry : std::as_const( entries_ ) ) {
        if ( entry.item->value().key == key ) {
            entry.item->showNotice( message );
            return;
        }
    }
}

QString FooterDisplayWidget::allValuesText() const
{
    QStringList lines;
    for ( const auto& entry : entries_ ) {
        lines.append( keyAndValue( entry.item->value() ) );
    }
    return lines.join( QLatin1Char( '\n' ) );
}

} // namespace custom_footer
