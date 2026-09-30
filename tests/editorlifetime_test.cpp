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
 * @file editorlifetime_test.cpp
 * @brief BDD tests for the rule editor's dialogs, which never block, and for
 *        deleting the editor in the middle of things.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footereditor.h"
#include "rulelistmodel.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>

using namespace custom_footer;

namespace {

template <typename Widget>
Widget* child( QObject& parent, const char* name )
{
    auto* widget = parent.findChild<Widget*>( name );
    REQUIRE( widget );
    return widget;
}

FooterEntry rule( const QString& key )
{
    return { key, key + "=(\\S+)", "", true, { ValueMapping{ "raw", "shown" } } };
}

} // namespace

SCENARIO( "The rule editor's import and export never block", "[footereditor][lifetime]" )
{
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    auto* editor = new FooterEditor( { rule( "A" ) } );
    editor->show();
    auto* model = editor->findChild<RuleListModel*>();
    REQUIRE( model );

    GIVEN( "the import dialog, opened from the Import button" )
    {
        child<QToolButton>( *editor, "importButton" )->click();
        QPointer<QFileDialog> dialog = child<QFileDialog>( *editor, "importDialog" );

        THEN( "it is open, and the click has returned" )
        {
            REQUIRE( dialog->isVisible() );
        }

        WHEN( "a rules file is chosen" )
        {
            const auto path = dir.path() + "/rules.json";
            REQUIRE( FooterConfig::exportToJson( path, { rule( "B" ), rule( "C" ) } ) );
            dialog->selectFile( path );
            static_cast<QDialog*>( dialog.data() )->accept(); // QFileDialog::accept(), as OK does

            THEN( "its rules are appended" )
            {
                REQUIRE( model->rowCount() == 3 );
                REQUIRE( model->entry( 2 ).key == "C" );
            }
        }

        WHEN( "a file that is no rules file is chosen" )
        {
            const auto path = dir.path() + "/broken.json";
            {
                QFile file( path );
                REQUIRE( file.open( QIODevice::WriteOnly ) );
                file.write( "not json" );
            }
            dialog->selectFile( path );
            static_cast<QDialog*>( dialog.data() )->accept(); // QFileDialog::accept(), as OK does

            THEN( "the error is shown without blocking, and no rule is added" )
            {
                REQUIRE( child<QMessageBox>( *editor, "importErrorBox" )->isVisible() );
                REQUIRE( model->rowCount() == 1 );
            }
        }

        WHEN( "the editor is deleted while the dialog is open" )
        {
            delete editor;
            editor = nullptr;

            THEN( "the dialog is gone with it" )
            {
                REQUIRE_FALSE( dialog );
            }
        }
    }

    GIVEN( "the export dialog, opened from the Export button" )
    {
        child<QToolButton>( *editor, "exportButton" )->click();
        auto* dialog = child<QFileDialog>( *editor, "exportDialog" );
        REQUIRE( dialog->isVisible() );

        WHEN( "a file is chosen" )
        {
            const auto path = dir.path() + "/out.json";
            dialog->selectFile( path );
            static_cast<QDialog*>( dialog )->accept(); // QFileDialog::accept(), as OK does

            THEN( "the rules are written to it" )
            {
                const auto exported = FooterConfig::importFromJson( path );
                REQUIRE( exported.size() == 1 );
                REQUIRE( exported[ 0 ].key == "A" );
            }
        }
    }

    delete editor;
}

SCENARIO( "The rule editor can be deleted while a mapping is being edited",
          "[footereditor][lifetime]" )
{
    GIVEN( "a shown editor with a focused mapping cell being edited" )
    {
        auto* editor = new FooterEditor( { rule( "A" ), rule( "B" ) } );
        editor->show();
        editor->activateWindow();
        auto* mappings = child<QTableWidget>( *editor, "mappingTable" );
        mappings->setCurrentCell( 0, 1 );
        mappings->editItem( mappings->item( 0, 1 ) );
        auto* cellEditor = mappings->viewport()->findChild<QLineEdit*>();
        REQUIRE( cellEditor );
        cellEditor->setFocus();
        cellEditor->setText( "typed" );

        auto* model = editor->findChild<RuleListModel*>();
        REQUIRE( model );
        int changes = 0;
        QList<FooterEntry> seen;
        QObject::connect( model, &RuleListModel::dataChanged, [ &changes, &seen, model ] {
            ++changes;
            seen = model->entries();
        } );

        WHEN( "it is deleted" )
        {
            delete editor;

            THEN( "the edit was stored once, while the editor was whole" )
            {
                REQUIRE( changes == 1 );
                REQUIRE( seen.size() == 2 );
                REQUIRE( seen[ 0 ].mappings[ 0 ].displayValue == "typed" );
            }
        }
    }
}
