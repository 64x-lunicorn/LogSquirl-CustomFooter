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
 * @file ruletemplate_mapping_test.cpp
 * @brief Templates with value mappings: Manufacturer and Model year from
 *        the VIN.
 *
 * The sample VINs are those of the VIN template's tests: the example VIN
 * 1M8GDM9AXKP042788 of the Wikipedia article (WMI 1M8 is not mapped) and
 * VINs of the same shape with the WMIs WBA and WVW.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footereditor.h"
#include "footerscanner.h"
#include "ruletemplate.h"
#include "ruletemplatedialog.h"
#include "templatesamples.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>

#include <optional>

using namespace custom_footer;

namespace {

/// Mappings as `raw=display` strings, so they compare and print.
QStringList pairsOf( const QList<ValueMapping>& mappings )
{
    QStringList pairs;
    for ( const auto& mapping : mappings ) {
        pairs.append( mapping.pattern + "=" + mapping.displayValue );
    }
    return pairs;
}

/// The display value of @p raw in @p mappings; nothing if it is unmapped.
std::optional<QString> displayOf( const QList<ValueMapping>& mappings, const QString& raw )
{
    for ( const auto& mapping : mappings ) {
        if ( mapping.pattern == raw ) {
            return mapping.displayValue;
        }
    }
    return std::nullopt;
}

/// Adds the templates named @p names in @p editor through From template….
void addFromTemplates( FooterEditor& editor, const QStringList& names )
{
    auto* button = editor.findChild<QToolButton*>( "templateButton" );
    REQUIRE( button );
    button->click();
    auto* dialog = editor.findChild<RuleTemplateDialog*>( "ruleTemplateDialog" );
    REQUIRE( dialog );
    auto* list = dialog->findChild<QTreeWidget*>( "templateList" );
    REQUIRE( list );
    for ( const auto& name : names ) {
        const auto items = list->findItems( name, Qt::MatchExactly | Qt::MatchRecursive, 0 );
        REQUIRE( items.size() == 1 );
        items.first()->setCheckState( 0, Qt::Checked );
    }
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    REQUIRE( buttons );
    REQUIRE( buttons->button( QDialogButtonBox::Ok )->isEnabled() );
    buttons->button( QDialogButtonBox::Ok )->click();
    REQUIRE( !dialog->isVisible() );
}

/// The rows of the editor's mapping table as `raw=display` strings.
QStringList mappingTableOf( FooterEditor& editor )
{
    auto* table = editor.findChild<QTableWidget*>( "mappingTable" );
    REQUIRE( table );
    QStringList pairs;
    for ( int row = 0; row < table->rowCount(); ++row ) {
        pairs.append( table->item( row, 0 )->text() + "=" + table->item( row, 1 )->text() );
    }
    return pairs;
}

} // namespace

SCENARIO( "Manufacturer and Model year follow the VIN in the Automotive group",
          "[ruletemplate][mapping]" )
{
    THEN( "after VIN, IMEI and ICCID, keyed as named and advanced" )
    {
        REQUIRE( namesOf( templatesIn( "Automotive" ) )
                 == QStringList{ "VIN", "IMEI", "ICCID", "Manufacturer", "Model year" } );
        for ( const auto* name : { "Manufacturer", "Model year" } ) {
            INFO( name );
            const auto& t = templateNamed( name );
            REQUIRE( t.key == name );
            REQUIRE( !t.isSimple() );
            REQUIRE( t.entry().linePattern == t.linePattern );
        }
    }

    THEN( "the Model year description names the cycle its mappings cover" )
    {
        REQUIRE( templateNamed( "Model year" ).description.contains( "2010 to 2039" ) );
    }
}

SCENARIO( "Manufacturer and Model year bring their mappings", "[ruletemplate][mapping]" )
{
    THEN( "Manufacturer maps common WMIs to the manufacturer, each WMI once" )
    {
        const auto& mappings = templateNamed( "Manufacturer" ).mappings;
        REQUIRE( displayOf( mappings, "WVW" ) == QString( "Volkswagen" ) );
        REQUIRE( displayOf( mappings, "WBA" ) == QString( "BMW" ) );
        REQUIRE( displayOf( mappings, "WAU" ) == QString( "Audi" ) );
        REQUIRE( displayOf( mappings, "WP0" ) == QString( "Porsche" ) );
        REQUIRE( displayOf( mappings, "W1K" ) == QString( "Mercedes-Benz" ) );
        REQUIRE( displayOf( mappings, "1FA" ) == QString( "Ford" ) );
        REQUIRE( displayOf( mappings, "1G1" ) == QString( "Chevrolet" ) );
        REQUIRE( displayOf( mappings, "JTD" ) == QString( "Toyota" ) );
        REQUIRE( displayOf( mappings, "JHM" ) == QString( "Honda" ) );
        REQUIRE( displayOf( mappings, "JN1" ) == QString( "Nissan" ) );
        REQUIRE( displayOf( mappings, "KMH" ) == QString( "Hyundai" ) );
        REQUIRE( displayOf( mappings, "KNA" ) == QString( "Kia" ) );
        REQUIRE( displayOf( mappings, "5YJ" ) == QString( "Tesla" ) );
        REQUIRE( displayOf( mappings, "YV1" ) == QString( "Volvo" ) );
        REQUIRE( displayOf( mappings, "TMB" ) == QString( "Škoda" ) );
        REQUIRE( displayOf( mappings, "VF7" ) == QString( "Citroën" ) );
        REQUIRE( displayOf( mappings, "1M8" ) == std::nullopt );

        QSet<QString> wmis;
        for ( const auto& mapping : mappings ) {
            INFO( mapping.pattern.toStdString() );
            REQUIRE(
                QRegularExpression( "^[A-HJ-NPR-Z0-9]{3}$" ).match( mapping.pattern ).hasMatch() );
            REQUIRE( !mapping.displayValue.isEmpty() );
            REQUIRE( !wmis.contains( mapping.pattern ) );
            wmis.insert( mapping.pattern );
        }
    }

    THEN( "Model year maps the 30 codes to the years 2010 to 2039" )
    {
        const auto& mappings = templateNamed( "Model year" ).mappings;
        REQUIRE( mappings.size() == 30 );
        REQUIRE( pairsOf( mappings.mid( 0, 3 ) ) == QStringList{ "A=2010", "B=2011", "C=2012" } );
        REQUIRE( displayOf( mappings, "H" ) == QString( "2017" ) );
        REQUIRE( displayOf( mappings, "J" ) == QString( "2018" ) );
        REQUIRE( displayOf( mappings, "P" ) == QString( "2023" ) );
        REQUIRE( displayOf( mappings, "R" ) == QString( "2024" ) );
        REQUIRE( displayOf( mappings, "T" ) == QString( "2026" ) );
        REQUIRE( displayOf( mappings, "V" ) == QString( "2027" ) );
        REQUIRE( displayOf( mappings, "Y" ) == QString( "2030" ) );
        REQUIRE( displayOf( mappings, "1" ) == QString( "2031" ) );
        REQUIRE( displayOf( mappings, "9" ) == QString( "2039" ) );
        for ( const auto* unused : { "I", "O", "Q", "U", "Z", "0" } ) {
            INFO( unused );
            REQUIRE( displayOf( mappings, unused ) == std::nullopt );
        }
    }

    THEN( "the new rule has the template's mappings" )
    {
        for ( const auto* name : { "Manufacturer", "Model year" } ) {
            INFO( name );
            const auto& t = templateNamed( name );
            REQUIRE( !t.mappings.isEmpty() );
            REQUIRE( pairsOf( t.entry().mappings ) == pairsOf( t.mappings ) );
        }
    }

    THEN( "every other template brings none, and its rule has none" )
    {
        for ( const auto& t : ruleTemplates() ) {
            if ( t.name == "Manufacturer" || t.name == "Model year" ) {
                continue;
            }
            INFO( t.name.toStdString() );
            REQUIRE( t.mappings.isEmpty() );
            REQUIRE( t.entry( "user" ).mappings.isEmpty() );
        }
    }
}

SCENARIO( "The footer shows the mapped manufacturer and model year of a VIN",
          "[ruletemplate][mapping]" )
{
    THEN( "Manufacturer shows the manufacturer, or an unmapped WMI as it is" )
    {
        // bimmer_connected 0.17.4, `bimmerconnected status`
        requireFinds( "Manufacturer", { { "VIN: WBA3A5C51CF256985", "BMW" } } );
        // bimmer_connected 0.17.4, the error of `bimmerconnected status --vin`
        requireFinds( "Manufacturer", { { "Error: Could not find vehicle for VIN WBA123. "
                                          "Valid VINs are: ['WBA3A5C51CF256985']",
                                          "BMW" } } );
        requireFinds( "Manufacturer",
                      { { "{\"vin\": \"WVWZZZ1JZXW000001\", \"mileage\": 1200}", "Volkswagen" },
                        { "1M8GDM9AXKP042788", "1M8" },
                        { "upload vin_1M8GDM9AXKP042788.log", "1M8" } } );
    }

    THEN( "Model year shows the year in the 2010 to 2039 cycle" )
    {
        // bimmer_connected 0.17.4, `bimmerconnected status`
        requireFinds( "Model year", { { "VIN: WBA3A5C51CF256985", "2012" } } );
        requireFinds( "Model year",
                      { { "{\"vin\": \"WVWZZZ1JZXW000001\", \"mileage\": 1200}", "2029" },
                        { "vehicle=1M8GDM9AXKP042788;state=parked", "2019" } } );
    }

    THEN( "the parts of the first VIN in the file" )
    {
        requireFinds( "Manufacturer",
                      { { "VIN: 1M8GDM9AXKP042788\nVIN: WVWZZZ1JZXW000001", "1M8" } } );
        requireFinds( "Model year",
                      { { "VIN: WVWZZZ1JZXW000001\nVIN: WBA3A5C51CF256985", "2029" } } );
    }

    THEN( "nothing where the VIN template finds no VIN" )
    {
        for ( const auto* name : { "Manufacturer", "Model year" } ) {
            INFO( name );
            requireFindsNothing( name, { "VIN: WBA3A5C51IF256985", "VIN: WBA3A5C51OF256985",
                                         "VIN: WBA3A5C51QF256985", "id 12345678901234567",
                                         "VIN: ABCDEFGHJKLMNPRST", "vin: wba3a5c51cf256985",
                                         "VIN: WBA3A5C51CF25698", "VIN: WBA3A5C51CF2569850",
                                         "XWBA3A5C51CF256985", "WBA3A5C51CF256985Z",
                                         "manufacturer: WBA", "model year: C", "Wba123 BMW" } );
        }
    }
}

SCENARIO( "A rule added from a template with mappings keeps them",
          "[ruletemplate][mapping][footereditor][footerconfig]" )
{
    GIVEN( "an editor without rules" )
    {
        FooterEditor editor( {} );
        editor.show();

        WHEN( "adding the Manufacturer template" )
        {
            addFromTemplates( editor, { "Manufacturer" } );

            THEN( "the rule and the mapping table hold its mappings" )
            {
                const auto expected = pairsOf( templateNamed( "Manufacturer" ).mappings );
                REQUIRE( editor.entries().size() == 1 );
                REQUIRE( pairsOf( editor.entries().first().mappings ) == expected );
                REQUIRE( mappingTableOf( editor ) == expected );
            }
        }

        WHEN( "adding Manufacturer and Model year, saving and loading the rules" )
        {
            addFromTemplates( editor, { "Manufacturer", "Model year" } );
            QTemporaryDir dir;
            REQUIRE( dir.isValid() );
            REQUIRE( FooterConfig::saveEntries( dir.path(), editor.entries() ) );
            const auto loaded = FooterConfig::loadEntries( dir.path() );

            THEN( "the loaded rules have the mappings, also in a new editor" )
            {
                REQUIRE( loaded.size() == 2 );
                REQUIRE( loaded.at( 0 ).key == "Manufacturer" );
                REQUIRE( loaded.at( 1 ).key == "Model year" );
                REQUIRE( pairsOf( loaded.at( 0 ).mappings )
                         == pairsOf( templateNamed( "Manufacturer" ).mappings ) );
                REQUIRE( pairsOf( loaded.at( 1 ).mappings )
                         == pairsOf( templateNamed( "Model year" ).mappings ) );

                FooterEditor reopened( loaded );
                reopened.show();
                REQUIRE( pairsOf( reopened.entries().at( 0 ).mappings )
                         == pairsOf( loaded.at( 0 ).mappings ) );
                REQUIRE( pairsOf( reopened.entries().at( 1 ).mappings )
                         == pairsOf( loaded.at( 1 ).mappings ) );
            }

            THEN( "the loaded rules show the mapped values for a VIN" )
            {
                const auto path = dir.path() + "/sample.log";
                QFile file( path );
                REQUIRE( file.open( QIODevice::WriteOnly ) );
                // bimmer_connected 0.17.4, `bimmerconnected status`
                file.write( "VIN: WBA3A5C51CF256985\n" );
                file.close();
                const auto values = FooterScanner::scan( path, loaded );
                REQUIRE( values.value( "Manufacturer" ) == "BMW" );
                REQUIRE( values.value( "Model year" ) == "2012" );
            }
        }
    }
}
