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
 * @file ruletemplate_automotive_test.cpp
 * @brief The Automotive rule templates: VIN, IMEI and ICCID.
 *
 * The sample lines have the form the named tools write, with example
 * values instead of real identifiers: the example VIN 1M8GDM9AXKP042788
 * and the example IMEI 490154203237518 of their Wikipedia articles, VINs
 * of the same shape, and an ICCID of the usual 20 digits.
 */

#include <catch2/catch.hpp>

#include "footereditor.h"
#include "ruletemplate.h"
#include "ruletemplatedialog.h"
#include "simplerule.h"
#include "templatesamples.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>

using namespace custom_footer;

namespace {

/// Adds the template named @p name in @p editor through From template….
void addFromTemplate( FooterEditor& editor, const QString& name )
{
    auto* button = editor.findChild<QToolButton*>( "templateButton" );
    REQUIRE( button );
    button->click();
    auto* dialog = editor.findChild<RuleTemplateDialog*>( "ruleTemplateDialog" );
    REQUIRE( dialog );
    auto* list = dialog->findChild<QTreeWidget*>( "templateList" );
    REQUIRE( list );
    const auto items = list->findItems( name, Qt::MatchExactly | Qt::MatchRecursive, 0 );
    REQUIRE( items.size() == 1 );
    items.first()->setCheckState( 0, Qt::Checked );
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    REQUIRE( buttons );
    REQUIRE( buttons->button( QDialogButtonBox::Ok )->isEnabled() );
    buttons->button( QDialogButtonBox::Ok )->click();
    REQUIRE( !dialog->isVisible() );
}

} // namespace

SCENARIO( "The Automotive group starts with VIN, IMEI and ICCID", "[ruletemplate][automotive]" )
{
    THEN( "in this order, keyed as named" )
    {
        // The templates with mappings after them are tested in
        // ruletemplate_mapping_test.cpp.
        REQUIRE( namesOf( templatesIn( "Automotive" ) ).mid( 0, 3 )
                 == QStringList{ "VIN", "IMEI", "ICCID" } );
        for ( const auto* name : { "VIN", "IMEI", "ICCID" } ) {
            INFO( name );
            REQUIRE( templateNamed( name ).key == name );
            REQUIRE( templateNamed( name ).group == "Automotive" );
        }
    }

    THEN( "the VIN needs a shape and is advanced, IMEI and ICCID are simple rules" )
    {
        REQUIRE( !templateNamed( "VIN" ).isSimple() );
        REQUIRE( !simpleRuleOf( templateNamed( "VIN" ).entry() ).has_value() );
        REQUIRE( templateNamed( "IMEI" ).simple
                 == SimpleRule{ "IMEI:", ValueEnd::Whitespace, {} } );
        REQUIRE( templateNamed( "ICCID" ).simple
                 == SimpleRule{ "ICCID:", ValueEnd::Whitespace, {} } );
        for ( const auto* name : { "IMEI", "ICCID" } ) {
            INFO( name );
            const auto entry = templateNamed( name ).entry();
            REQUIRE( simpleRuleOf( entry ).has_value() );
            REQUIRE( entry.linePattern == simpleLinePattern( *simpleRuleOf( entry ) ) );
        }
    }
}

SCENARIO( "Automotive templates open in their mode in the editor",
          "[ruletemplate][automotive][footereditor]" )
{
    GIVEN( "an editor without rules" )
    {
        FooterEditor editor( {} );
        editor.show();
        auto* advanced = editor.findChild<QCheckBox*>( "advancedCheck" );
        auto* textBefore = editor.findChild<QLineEdit*>( "textBeforeEdit" );
        REQUIRE( advanced );
        REQUIRE( textBefore );

        for ( const auto* name : { "IMEI", "ICCID" } ) {
            WHEN( std::string( "adding the " ) + name + " template" )
            {
                addFromTemplate( editor, name );

                THEN( "its rule opens in simple mode with the text to adjust" )
                {
                    REQUIRE( editor.entries().size() == 1 );
                    REQUIRE( !advanced->isChecked() );
                    REQUIRE( textBefore->isVisibleTo( &editor ) );
                    REQUIRE( textBefore->text() == QString( name ) + ":" );
                }
            }
        }

        WHEN( "adding the VIN template" )
        {
            addFromTemplate( editor, "VIN" );

            THEN( "its rule opens in advanced mode" )
            {
                REQUIRE( editor.entries().size() == 1 );
                REQUIRE( advanced->isChecked() );
            }
        }
    }
}

SCENARIO( "The VIN template finds a VIN by its shape", "[ruletemplate][automotive]" )
{
    THEN( "with or without text before it" )
    {
        // bimmer_connected 0.17.4, `bimmerconnected status`
        requireFinds( "VIN", { { "VIN: WBA3A5C51CF256985", "WBA3A5C51CF256985" } } );
        // bimmer_connected 0.17.4, the error of `bimmerconnected status --vin`
        requireFinds( "VIN", { { "Error: Could not find vehicle for VIN WBA123. "
                                 "Valid VINs are: ['WBA3A5C51CF256985']",
                                 "WBA3A5C51CF256985" } } );
        requireFinds(
            "VIN", { { "1M8GDM9AXKP042788", "1M8GDM9AXKP042788" },
                     { "{\"vin\": \"WVWZZZ1JZXW000001\", \"mileage\": 1200}", "WVWZZZ1JZXW000001" },
                     { "vehicle=1M8GDM9AXKP042788;state=parked", "1M8GDM9AXKP042788" },
                     { "upload vin_1M8GDM9AXKP042788.log", "1M8GDM9AXKP042788" } } );
    }

    THEN( "the first VIN in the file, as it holds for the whole file" )
    {
        requireFinds(
            "VIN", { { "VIN: WVWZZZ1JZXW000001\nVIN: 1M8GDM9AXKP042788", "WVWZZZ1JZXW000001" } } );
    }

    THEN( "not with I, O or Q, all digits, all letters, lower case, or another length" )
    {
        requireFindsNothing( "VIN", { "VIN: 1M8GDM9AIKP042788", "VIN: 1M8GDM9AOKP042788",
                                      "VIN: 1M8GDM9AQKP042788", "id 12345678901234567",
                                      "VIN: ABCDEFGHJKLMNPRST", "vin: 1m8gdm9axkp042788",
                                      "VIN: 1M8GDM9AXKP04278", "VIN: 1M8GDM9AXKP0427880" } );
    }

    THEN( "not as part of a longer word" )
    {
        requireFindsNothing( "VIN", { "X1M8GDM9AXKP042788", "1M8GDM9AXKP042788Z",
                                      "a1M8GDM9AXKP042788", "1M8GDM9AXKP042788b",
                                      "sha 0A1B2C3D4E5F6A7B8C9D0E1F2A3B4C5D6E7F8A9B" } );
    }
}

SCENARIO( "The IMEI template finds the number after IMEI:", "[ruletemplate][automotive]" )
{
    THEN( "the word after IMEI:" )
    {
        // TinyGSM 0.12.0, AllFunctions example: DBG( "IMEI:", imei )
        requireFinds( "IMEI", { { "[5123] IMEI: 490154203237518", "490154203237518" } } );
        // nRF Connect SDK v2.5.0, an application's LOG_INF of modem_info
        requireFinds( "IMEI", { { "[00:00:05.329,772] <inf> lte_info: "
                                  "ICCID:89014103211118510720, IMSI:310150123456789, "
                                  "IMEI:490154203237518",
                                  "490154203237518" } } );
    }

    THEN( "not a number without the text, another spelling, the IMEISV, or no value" )
    {
        requireFindsNothing( "IMEI",
                             {
                                 // ModemManager 1.22, `mmcli -m 0`
                                 "           |         equipment id: 490154203237518",
                                 // 3GPP TS 27.007 AT+CGSN, plain and with =1 (nRF91 firmware 1.3)
                                 "490154203237518",
                                 "+CGSN: \"490154203237518\"",
                                 "imei: 490154203237518",
                                 "IMEI 490154203237518",
                                 "IMEISV: 4901542032375181",
                                 "IMEI:",
                             } );
    }
}

SCENARIO( "The ICCID template finds the number after ICCID:", "[ruletemplate][automotive]" )
{
    THEN( "the word after ICCID:, also ending a longer word" )
    {
        // nRF91 modem firmware 1.3.5, AT%XICCID response
        requireFinds( "ICCID",
                      { { "%XICCID: 89014103211118510720", "89014103211118510720" },
                        { "SIM ICCID: 8901410321111851072F ready", "8901410321111851072F" } } );
    }

    THEN( "not a number without the text, another spelling, or no value" )
    {
        requireFindsNothing( "ICCID",
                             {
                                 // TinyGSM 0.12.0, AllFunctions example: DBG( "CCID:", ccid )
                                 "[5120] CCID: 89014103211118510720",
                                 // ModemManager 1.22, `mmcli -i 0`
                                 "           |              iccid: 89014103211118510720",
                                 // Quectel EC25, AT+QCCID response
                                 "+QCCID: 89014103211118510720",
                                 "89014103211118510720",
                                 "ICCID 89014103211118510720",
                                 "ICCID:",
                             } );
    }
}
