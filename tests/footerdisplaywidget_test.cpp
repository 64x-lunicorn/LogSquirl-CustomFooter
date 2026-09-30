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

/**
 * @file footerdisplaywidget_test.cpp
 * @brief BDD tests for the FooterDisplayWidget and its value items.
 */

#include <catch2/catch.hpp>

#include "footerdisplaywidget.h"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QToolTip>

using namespace custom_footer;

namespace {

QList<FooterValueItem*> items( const FooterDisplayWidget& widget )
{
    return widget.findChildren<FooterValueItem*>();
}

/// The text of every label, e.g. the keys and separators.
QString labelText( const FooterDisplayWidget& widget )
{
    QStringList texts;
    for ( const auto* label : widget.findChildren<QLabel*>() ) {
        texts.append( label->text() );
    }
    return texts.join( ' ' );
}

void click( QWidget* widget )
{
    const QPointF pos = widget->rect().center();
    const QPointF global = widget->mapToGlobal( pos );
    QMouseEvent press( QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton,
                       Qt::NoModifier );
    QApplication::sendEvent( widget, &press );
    QMouseEvent release( QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton,
                         Qt::NoModifier );
    QApplication::sendEvent( widget, &release );
}

void pressKey( QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier )
{
    QKeyEvent press( QEvent::KeyPress, key, modifiers );
    QApplication::sendEvent( widget, &press );
    QKeyEvent release( QEvent::KeyRelease, key, modifiers );
    QApplication::sendEvent( widget, &release );
}

QString clipboardText()
{
    return QApplication::clipboard()->text();
}

} // namespace

SCENARIO( "FooterDisplayWidget shows key-value pairs", "[footerdisplaywidget]" )
{
    FooterDisplayWidget widget;

    GIVEN( "an empty widget" )
    {
        THEN( "it shows no values" )
        {
            REQUIRE( items( widget ).isEmpty() );
            REQUIRE( widget.values().isEmpty() );
        }
    }

    GIVEN( "a list of values" )
    {
        const QList<FooterValue> values = {
            { "VIN", "WVWZZZ1JZWW123456", "WVWZZZ1JZWW123456", 0 },
            { "Status", "OK", "0", 1 },
        };

        WHEN( "updateValues is called" )
        {
            widget.updateValues( values );

            THEN( "each value is shown in an item of its own, after its key" )
            {
                REQUIRE( widget.values() == values );
                const auto shown = items( widget );
                REQUIRE( shown.size() == 2 );
                REQUIRE( shown[ 0 ]->text() == "WVWZZZ1JZWW123456" );
                REQUIRE( shown[ 0 ]->value() == values[ 0 ] );
                REQUIRE( shown[ 1 ]->text() == "OK" );
                REQUIRE( labelText( widget ).contains( "VIN" ) );
                REQUIRE( labelText( widget ).contains( "Status" ) );
                REQUIRE( labelText( widget ).contains( "|" ) );
            }
        }

        WHEN( "updateValues is called with other values" )
        {
            widget.updateValues( values );
            widget.updateValues( { { "ID", "7", "7", 2 } } );

            THEN( "only the other values are shown" )
            {
                const auto shown = items( widget );
                REQUIRE( shown.size() == 1 );
                REQUIRE( shown[ 0 ]->text() == "7" );
                REQUIRE_FALSE( labelText( widget ).contains( "VIN" ) );
            }
        }

        WHEN( "a value changes, but the keys stay the same" )
        {
            widget.updateValues( values );
            const auto before = items( widget );
            auto changed = values;
            changed[ 1 ].value = "Failed";
            changed[ 1 ].rawValue = "E17";
            widget.updateValues( changed );

            THEN( "the same items show the new values, so the keyboard focus stays" )
            {
                REQUIRE( items( widget ) == before );
                REQUIRE( before[ 1 ]->text() == "Failed" );
                REQUIRE( before[ 1 ]->value() == changed[ 1 ] );
                REQUIRE( before[ 1 ]->toolTip().contains( "E17" ) );
                REQUIRE( widget.values() == changed );
            }
        }

        WHEN( "updateValues is called then clearValues is called" )
        {
            widget.updateValues( values );
            widget.clearValues();

            THEN( "nothing is shown" )
            {
                REQUIRE( items( widget ).isEmpty() );
                REQUIRE( widget.values().isEmpty() );
                REQUIRE( labelText( widget ).trimmed().isEmpty() );
            }
        }

        WHEN( "updateValues is called with an empty list" )
        {
            widget.updateValues( values );
            widget.updateValues( {} );

            THEN( "nothing is shown" )
            {
                REQUIRE( items( widget ).isEmpty() );
                REQUIRE( labelText( widget ).trimmed().isEmpty() );
            }
        }
    }

    GIVEN( "values containing HTML and mnemonic characters" )
    {
        widget.updateValues( { { "<i>Tag</i>", "<script>alert('xss')</script> & co",
                                 "<script>alert('xss')</script> & co", 0 } } );

        THEN( "the key's HTML is escaped and not injected" )
        {
            REQUIRE_FALSE( labelText( widget ).contains( "<i>" ) );
            REQUIRE( labelText( widget ).contains( "&lt;i&gt;Tag&lt;/i&gt;" ) );
        }

        THEN( "the value is shown as plain text" )
        {
            const auto* item = items( widget ).value( 0 );
            REQUIRE( item != nullptr );
            REQUIRE( item->toolTip().contains( "&lt;script&gt;" ) );
            REQUIRE_FALSE( item->toolTip().contains( "<script>" ) );
        }
    }
}

SCENARIO( "A footer value is copied with a click", "[footerdisplaywidget][copy]" )
{
    FooterDisplayWidget widget;
    widget.updateValues( {
        { "Component Protection", "Enabled", "true", 1 },
        { "Note", "a & b", "a & b", 2 },
    } );
    widget.show();
    QApplication::clipboard()->setText( "before" );

    const auto shown = items( widget );
    REQUIRE( shown.size() == 2 );

    WHEN( "a value is clicked" )
    {
        click( shown[ 0 ] );

        THEN( "exactly the displayed value is on the clipboard, not the key or raw value" )
        {
            REQUIRE( clipboardText() == "Enabled" );
        }

        THEN( "a short confirmation shows next to it, without a dialog" )
        {
            REQUIRE( QToolTip::isVisible() );
            REQUIRE( QToolTip::text().contains( "Copied" ) );
            REQUIRE( QApplication::activeModalWidget() == nullptr );
        }

        AND_WHEN( "another value is clicked" )
        {
            click( shown[ 1 ] );

            THEN( "that value replaces it on the clipboard, with its ampersand intact" )
            {
                REQUIRE( clipboardText() == "a & b" );
            }
        }
    }

    WHEN( "a value has the keyboard focus and Space is pressed" )
    {
        pressKey( shown[ 1 ], Qt::Key_Space );

        THEN( "the value is copied" )
        {
            REQUIRE( clipboardText() == "a & b" );
        }
    }

    WHEN( "a value has the keyboard focus and Return is pressed" )
    {
        pressKey( shown[ 0 ], Qt::Key_Return );

        THEN( "the value is copied" )
        {
            REQUIRE( clipboardText() == "Enabled" );
        }
    }

    WHEN( "a value has the keyboard focus and the copy shortcut is pressed" )
    {
        const auto copy = QKeySequence( QKeySequence::Copy )[ 0 ];
        pressKey( shown[ 0 ], copy.key(), copy.keyboardModifiers() );

        THEN( "the value is copied" )
        {
            REQUIRE( clipboardText() == "Enabled" );
        }
    }

    WHEN( "the value's context menu is asked for" )
    {
        THEN( "it offers to copy the value" )
        {
            REQUIRE( shown[ 0 ]->contextMenuPolicy() == Qt::CustomContextMenu );
            REQUIRE( shown[ 0 ]->contextMenu() != nullptr );
            const auto actions = shown[ 0 ]->contextMenu()->actions();
            REQUIRE( actions.size() == 1 );
            actions[ 0 ]->trigger();
            REQUIRE( clipboardText() == "Enabled" );
        }
    }

    WHEN( "the values are replaced while the old ones are shown" )
    {
        widget.updateValues( { { "ID", "42", "42", 0 } } );

        THEN( "a click on the new value copies it" )
        {
            click( items( widget ).value( 0 ) );
            REQUIRE( clipboardText() == "42" );
        }
    }
}

SCENARIO( "A footer value tells where it came from", "[footerdisplaywidget][tooltip]" )
{
    FooterDisplayWidget widget;

    GIVEN( "a mapped value" )
    {
        widget.updateValues( { { "Component Protection", "Enabled", "true", 1 } } );
        const auto* item = items( widget ).value( 0 );
        REQUIRE( item != nullptr );

        THEN( "its tooltip names the key, the raw value and the rule" )
        {
            const auto tip = item->toolTip();
            REQUIRE( tip.contains( "Component Protection" ) );
            REQUIRE( tip.contains( "Enabled" ) );
            REQUIRE( tip.contains( "true" ) );
            REQUIRE( tip.contains( "rule 2" ) );
        }
    }

    GIVEN( "an unmapped value" )
    {
        widget.updateValues( { { "VIN", "WVWZZZ1JZWW123456", "WVWZZZ1JZWW123456", 0 } } );
        const auto* item = items( widget ).value( 0 );
        REQUIRE( item != nullptr );

        THEN( "its tooltip names the key and the rule, and shows the value only once" )
        {
            const auto tip = item->toolTip();
            REQUIRE( tip.contains( "VIN" ) );
            REQUIRE( tip.contains( "rule 1" ) );
            REQUIRE( tip.count( "WVWZZZ1JZWW123456" ) == 1 );
        }
    }
}

SCENARIO( "Footer values are reachable with the keyboard and a screen reader",
          "[footerdisplaywidget][accessibility]" )
{
    FooterDisplayWidget widget;
    widget.updateValues( {
        { "VIN", "ABC123", "ABC123", 0 },
        { "Mode", "Production", "0x04", 1 },
    } );
    const auto shown = items( widget );
    REQUIRE( shown.size() == 2 );

    THEN( "each value takes the focus with Tab" )
    {
        for ( const auto* item : shown ) {
            REQUIRE( ( item->focusPolicy() & Qt::TabFocus ) == Qt::TabFocus );
        }
    }

    THEN( "the keys and separators do not" )
    {
        for ( const auto* label : widget.findChildren<QLabel*>() ) {
            REQUIRE( label->focusPolicy() == Qt::NoFocus );
        }
    }

    THEN( "each value is a button whose accessible name holds its key and value" )
    {
        auto* first = QAccessible::queryAccessibleInterface( shown[ 0 ] );
        REQUIRE( first != nullptr );
        REQUIRE( first->role() == QAccessible::Button );
        REQUIRE( first->text( QAccessible::Name ) == "VIN: ABC123" );

        auto* second = QAccessible::queryAccessibleInterface( shown[ 1 ] );
        REQUIRE( second != nullptr );
        REQUIRE( second->text( QAccessible::Name ) == "Mode: Production" );
        REQUIRE( second->text( QAccessible::Description ).contains( "0x04" ) );
        REQUIRE_FALSE( second->text( QAccessible::Description ).contains( "<" ) );
    }
}
