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
#include <QToolTip>

namespace custom_footer {

namespace {

/// Button text shows "&" as a mnemonic marker unless it is doubled.
QString withoutMnemonics( QString text )
{
    return text.replace( QLatin1Char( '&' ), QStringLiteral( "&&" ) );
}

QString toolTipOf( const FooterValue& value )
{
    QStringList lines;
    lines.append( QStringLiteral( "<b>%1:</b> %2" )
                      .arg( value.key.toHtmlEscaped(), value.value.toHtmlEscaped() ) );
    if ( value.isMapped() ) {
        lines.append(
            FooterValueItem::tr( "Raw value: %1" ).arg( value.rawValue.toHtmlEscaped() ) );
    }
    if ( value.rule >= 0 ) {
        lines.append( FooterValueItem::tr( "From rule %1" ).arg( value.rule + 1 ) );
    }
    lines.append( QStringLiteral( "<i>%1</i>" ).arg( FooterValueItem::tr( "Click to copy" ) ) );
    return lines.join( QStringLiteral( "<br>" ) );
}

/// What the tooltip adds to the accessible name, as plain text.
QString descriptionOf( const FooterValue& value )
{
    QStringList sentences;
    if ( value.isMapped() ) {
        sentences.append( FooterValueItem::tr( "Raw value: %1." ).arg( value.rawValue ) );
    }
    if ( value.rule >= 0 ) {
        sentences.append( FooterValueItem::tr( "From rule %1." ).arg( value.rule + 1 ) );
    }
    sentences.append( FooterValueItem::tr( "Press Space to copy." ) );
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
    // only shown on request.
    contextMenu_->addAction( tr( "Copy Value" ), this, &FooterValueItem::copyToClipboard );
    setContextMenuPolicy( Qt::CustomContextMenu );
    connect( this, &QWidget::customContextMenuRequested, this,
             [ this ]( const QPoint& pos ) { contextMenu_->popup( mapToGlobal( pos ) ); } );

    setValue( value );
}

FooterValueItem::~FooterValueItem()
{
    // The confirmation must not outlive the item it points at.
    if ( confirmed_ && QToolTip::isVisible() ) {
        QToolTip::hideText();
    }
}

void FooterValueItem::setValue( const FooterValue& value )
{
    value_ = value;
    setText( withoutMnemonics( value.value ) );
    setToolTip( toolTipOf( value ) );
    setAccessibleName( QStringLiteral( "%1: %2" ).arg( value.key, value.value ) );
    setAccessibleDescription( descriptionOf( value ) );
}

void FooterValueItem::copyToClipboard()
{
    QGuiApplication::clipboard()->setText( value_.value );

    // Confirm at the value, until the mouse leaves it or the time is up.
    const auto confirmation = tr( "Copied" );
    QToolTip::showText( mapToGlobal( rect().center() ), confirmation, this, rect(),
                        kConfirmationMs );
    confirmed_ = true;

#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
    QAccessibleAnnouncementEvent announcement( this, confirmation );
    QAccessible::updateAccessibility( &announcement );
#endif
}

void FooterValueItem::keyPressEvent( QKeyEvent* event )
{
    if ( event->matches( QKeySequence::Copy ) || event->key() == Qt::Key_Return
         || event->key() == Qt::Key_Enter ) {
        copyToClipboard();
        event->accept();
        return;
    }
    // Space clicks the button, and so copies too.
    QToolButton::keyPressEvent( event );
}

FooterDisplayWidget::FooterDisplayWidget( QWidget* parent )
    : QWidget( parent )
    , layout_( new QHBoxLayout( this ) )
{
    layout_->setContentsMargins( 4, 0, 4, 0 );
}

void FooterDisplayWidget::updateValues( const QList<FooterValue>& values )
{
    const auto sameKeys = [ this, &values ] {
        if ( values.size() != values_.size() ) {
            return false;
        }
        for ( int i = 0; i < values.size(); ++i ) {
            if ( values[ i ].key != values_[ i ].key ) {
                return false;
            }
        }
        return true;
    };

    if ( content_ && sameKeys() ) {
        // Keep the items, and with them the keyboard focus and a tooltip.
        for ( int i = 0; i < values.size(); ++i ) {
            if ( values[ i ] != values_[ i ] ) {
                items_[ i ]->setValue( values[ i ] );
            }
        }
        values_ = values;
        return;
    }

    clearValues();
    if ( values.isEmpty() ) {
        return;
    }

    content_ = new QWidget( this );
    auto* row = new QHBoxLayout( content_ );
    row->setContentsMargins( 0, 0, 0, 0 );
    row->setSpacing( 2 );
    for ( const auto& value : values ) {
        if ( !items_.isEmpty() ) {
            auto* separator = new QLabel( QStringLiteral( "|" ), content_ );
            separator->setContentsMargins( 6, 0, 6, 0 );
            row->addWidget( separator );
        }

        auto* key = new QLabel( content_ );
        key->setTextFormat( Qt::RichText );
        key->setText( QStringLiteral( "<b>%1:</b>" ).arg( value.key.toHtmlEscaped() ) );
        row->addWidget( key );

        auto* item = new FooterValueItem( value, content_ );
        key->setBuddy( item );
        row->addWidget( item );
        items_.append( item );
    }
    row->addStretch();
    layout_->addWidget( content_ );
    values_ = values;
}

void FooterDisplayWidget::clearValues()
{
    delete content_;
    content_ = nullptr;
    items_.clear();
    values_.clear();
}

} // namespace custom_footer
