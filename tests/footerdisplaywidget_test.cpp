/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file footerdisplaywidget_test.cpp
 * @brief BDD tests for the FooterDisplayWidget.
 */

#include <catch2/catch.hpp>

#include "footerdisplaywidget.h"

#include <QLabel>

using namespace costume_footer;

SCENARIO( "FooterDisplayWidget shows key-value pairs", "[footerdisplaywidget]" )
{
    FooterDisplayWidget widget;

    GIVEN( "an empty widget" )
    {
        THEN( "it is visible and has a child label" )
        {
            auto* label = widget.findChild<QLabel*>();
            REQUIRE( label != nullptr );
            REQUIRE( label->text().isEmpty() );
        }
    }

    GIVEN( "a list of key-value pairs" )
    {
        QList<QPair<QString, QString>> values;
        values.append( qMakePair( QString( "VIN" ), QString( "WVWZZZ1JZWW123456" ) ) );
        values.append( qMakePair( QString( "Status" ), QString( "OK" ) ) );

        WHEN( "updateValues is called" )
        {
            widget.updateValues( values );

            THEN( "the label contains both entries with HTML formatting" )
            {
                auto* label = widget.findChild<QLabel*>();
                REQUIRE( label != nullptr );
                REQUIRE( label->text().contains( "VIN" ) );
                REQUIRE( label->text().contains( "WVWZZZ1JZWW123456" ) );
                REQUIRE( label->text().contains( "Status" ) );
                REQUIRE( label->text().contains( "OK" ) );
                REQUIRE( label->text().contains( "|" ) );
            }
        }

        WHEN( "updateValues is called then clearValues is called" )
        {
            widget.updateValues( values );
            widget.clearValues();

            THEN( "the label text is empty" )
            {
                auto* label = widget.findChild<QLabel*>();
                REQUIRE( label != nullptr );
                REQUIRE( label->text().isEmpty() );
            }
        }
    }

    GIVEN( "an empty value list" )
    {
        WHEN( "updateValues is called with an empty list" )
        {
            widget.updateValues( {} );

            THEN( "the label is cleared" )
            {
                auto* label = widget.findChild<QLabel*>();
                REQUIRE( label != nullptr );
                REQUIRE( label->text().isEmpty() );
            }
        }
    }

    GIVEN( "values containing HTML special characters" )
    {
        QList<QPair<QString, QString>> values;
        values.append( qMakePair( QString( "Tag" ),
                                  QString( "<script>alert('xss')</script>" ) ) );

        WHEN( "updateValues is called" )
        {
            widget.updateValues( values );

            THEN( "the HTML is escaped and not injected" )
            {
                auto* label = widget.findChild<QLabel*>();
                REQUIRE( label != nullptr );
                REQUIRE_FALSE( label->text().contains( "<script>" ) );
                REQUIRE( label->text().contains( "&lt;script&gt;" ) );
            }
        }
    }
}
