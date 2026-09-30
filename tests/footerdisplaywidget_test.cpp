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
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QTest>
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

QAction* action( const QMenu* menu, const char* name )
{
    for ( auto* candidate : menu->actions() ) {
        if ( candidate->objectName() == QLatin1String( name ) ) {
            return candidate;
        }
    }
    return nullptr;
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
        REQUIRE( shown[ 0 ]->contextMenuPolicy() == Qt::CustomContextMenu );
        const auto* menu = shown[ 0 ]->contextMenu();
        REQUIRE( menu != nullptr );

        THEN( "it offers to copy the value, key and value, or all values" )
        {
            REQUIRE( action( menu, "copyValue" ) != nullptr );
            REQUIRE( action( menu, "copyKeyAndValue" ) != nullptr );
            REQUIRE( action( menu, "copyAll" ) != nullptr );
        }

        THEN( "\"Copy Value\" copies the value" )
        {
            action( menu, "copyValue" )->trigger();
            REQUIRE( clipboardText() == "Enabled" );
        }

        THEN( "\"Copy Key and Value\" copies both, as shown" )
        {
            action( menu, "copyKeyAndValue" )->trigger();
            REQUIRE( clipboardText() == "Component Protection: Enabled" );
            REQUIRE( QToolTip::text().contains( "Copied" ) );
        }

        THEN( "\"Copy All\" copies every key and value, one per line" )
        {
            action( menu, "copyAll" )->trigger();
            REQUIRE( clipboardText() == "Component Protection: Enabled\nNote: a & b" );
            REQUIRE( QToolTip::text().contains( "Copied" ) );
        }
    }

    WHEN( "a new key appears after the values were shown" )
    {
        widget.updateValues( { { "Component Protection", "Enabled", "true", 1 },
                               { "Late", "x", "x", 3 },
                               { "Note", "a & b", "a & b", 2 } } );

        THEN( "\"Copy All\" of an old item copies the new values too" )
        {
            action( shown[ 0 ]->contextMenu(), "copyAll" )->trigger();
            REQUIRE( clipboardText() == "Component Protection: Enabled\nLate: x\nNote: a & b" );
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

SCENARIO( "The copy shortcut on a focused value is not taken by the window",
          "[footerdisplaywidget][copy]" )
{
    QMainWindow window;
    int hostCopies = 0;
    auto* hostCopy = new QAction( "Copy", &window );
    hostCopy->setShortcut( QKeySequence::Copy );
    window.addAction( hostCopy );
    QObject::connect( hostCopy, &QAction::triggered, [ &hostCopies ] { ++hostCopies; } );

    auto* widget = new FooterDisplayWidget;
    window.setCentralWidget( widget );
    widget->updateValues( { { "VIN", "ABC123", "ABC123", 0 } } );
    window.show();
    window.activateWindow();
    REQUIRE( QTest::qWaitForWindowActive( &window ) );
    QApplication::clipboard()->setText( "before" );

    GIVEN( "a value with the keyboard focus" )
    {
        auto* item = items( *widget ).value( 0 );
        REQUIRE( item != nullptr );
        item->setFocus();
        REQUIRE( item->hasFocus() );

        WHEN( "the copy shortcut is pressed" )
        {
            const auto copy = QKeySequence( QKeySequence::Copy )[ 0 ];
            QTest::keyClick( item, copy.key(), copy.keyboardModifiers() );

            THEN( "the value copies it, not the window's Copy action" )
            {
                REQUIRE( clipboardText() == "ABC123" );
                REQUIRE( hostCopies == 0 );
            }
        }

        WHEN( "Return is pressed" )
        {
            QTest::keyClick( item, Qt::Key_Return );

            THEN( "the value is copied" )
            {
                REQUIRE( clipboardText() == "ABC123" );
            }
        }
    }

    GIVEN( "no value has the keyboard focus" )
    {
        window.setFocus();

        WHEN( "the copy shortcut is pressed" )
        {
            const auto copy = QKeySequence( QKeySequence::Copy )[ 0 ];
            QTest::keyClick( &window, copy.key(), copy.keyboardModifiers() );

            THEN( "the window's Copy action gets it" )
            {
                REQUIRE( hostCopies == 1 );
            }
        }
    }
}

SCENARIO( "Footer values stay in place while new keys appear", "[footerdisplaywidget]" )
{
    FooterDisplayWidget widget;
    widget.updateValues( { { "VIN", "ABC123", "ABC123", 1 } } );
    QPointer<FooterValueItem> vin = items( widget ).value( 0 );
    REQUIRE( vin );
    vin->setFocus();
    REQUIRE( widget.focusWidget() == vin );

    WHEN( "a key appears before it" )
    {
        widget.updateValues(
            { { "Mode", "Production", "0x04", 0 }, { "VIN", "ABC123", "ABC123", 1 } } );

        THEN( "the value keeps its item and the keyboard focus" )
        {
            REQUIRE( vin );
            REQUIRE( widget.focusWidget() == vin );
            const auto shown = items( widget );
            REQUIRE( shown.contains( vin ) );
            REQUIRE( widget.values().size() == 2 );
            REQUIRE( widget.values()[ 0 ].key == "Mode" );
            REQUIRE( widget.values()[ 1 ].key == "VIN" );
        }

        THEN( "Tab goes through the values in the order they are shown" )
        {
            FooterValueItem* mode = nullptr;
            for ( auto* item : items( widget ) ) {
                if ( item->value().key == "Mode" ) {
                    mode = item;
                }
            }
            REQUIRE( mode != nullptr );
            auto* next = mode->nextInFocusChain();
            while ( next && !qobject_cast<FooterValueItem*>( next ) ) {
                next = next->nextInFocusChain();
            }
            REQUIRE( next == vin );
        }

        AND_WHEN( "it disappears again" )
        {
            widget.updateValues( { { "VIN", "ABC123", "ABC123", 1 } } );

            THEN( "the value still keeps its item" )
            {
                REQUIRE( vin );
                REQUIRE( items( widget ).size() == 1 );
                REQUIRE( labelText( widget ).count( "|" ) == 0 );
            }
        }
    }
}

SCENARIO( "A key with an ampersand is shown as it is", "[footerdisplaywidget]" )
{
    FooterDisplayWidget widget;
    widget.updateValues( { { "R&D", "on", "on", 0 } } );

    THEN( "the key's ampersand is no mnemonic" )
    {
        QLabel* key = nullptr;
        for ( auto* label : widget.findChildren<QLabel*>() ) {
            if ( label->text().contains( "R&amp;D" ) ) {
                key = label;
            }
        }
        REQUIRE( key != nullptr );
        REQUIRE( key->buddy() == nullptr );
        auto* accessible = QAccessible::queryAccessibleInterface( key );
        REQUIRE( accessible != nullptr );
        REQUIRE( accessible->text( QAccessible::Name ).contains( "R&D" ) );
        REQUIRE( items( widget ).value( 0 )->accessibleName() == "R&D: on" );
    }
}

SCENARIO( "Removing a copied value leaves other tooltips alone", "[footerdisplaywidget][copy]" )
{
    FooterDisplayWidget widget;
    QWidget other;
    other.show();
    widget.updateValues( { { "VIN", "ABC123", "ABC123", 0 } } );
    widget.show();
    click( items( widget ).value( 0 ) );
    REQUIRE( QToolTip::text().contains( "Copied" ) );

    GIVEN( "another widget's tooltip shown after the confirmation" )
    {
        // Qt hides any tooltip on a change of focus, as when the focused
        // value of a shown footer goes away; this is about what the item owns.
        widget.hide();
        QToolTip::showText( other.mapToGlobal( QPoint( 1, 1 ) ), "Other tip", &other );
        REQUIRE( QToolTip::text() == "Other tip" );

        WHEN( "the copied value goes away" )
        {
            widget.clearValues();

            THEN( "the other tooltip stays" )
            {
                REQUIRE( QToolTip::isVisible() );
                REQUIRE( QToolTip::text() == "Other tip" );
            }
        }
    }

    GIVEN( "the confirmation still shown" )
    {
        widget.hide();

        WHEN( "the copied value goes away" )
        {
            widget.clearValues();

            THEN( "the confirmation expires by itself, as it is not tied to the item" )
            {
                REQUIRE( QToolTip::isVisible() );
                REQUIRE( QToolTip::text() == "Copied" );
            }
        }
    }
}
