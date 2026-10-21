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
 * @file plaintext_test.cpp
 * @brief BDD tests for text from a rule or a file: shown as it is, never as
 *        rich text.
 */

#include <catch2/catch.hpp>

#include "footerdisplaywidget.h"
#include "footereditor.h"
#include "rulelistmodel.h"
#include "rulepreviewview.h"
#include "ruletemplatedialog.h"
#include "shownastooltip.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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

/// A valid regex that, shown as rich text, loads a file and is bold.
const QString kHtml = QStringLiteral( "<img src=\"file:///dev/zero\"><b>VIN</b> &amp; (\\S+)" );

} // namespace

SCENARIO( "The rule list's tooltip shows the line pattern as it is", "[plaintext][rulelist]" )
{
    GIVEN( "a rule whose line pattern looks like HTML" )
    {
        RuleListModel model;
        model.setEntries( { { "VIN", kHtml, "", true, {} } } );
        REQUIRE( model.problems( 0 ).isEmpty() );

        WHEN( "its line pattern is hovered" )
        {
            const auto shown = shownAsToolTip( model.index( 0, RuleListModel::LinePatternColumn )
                                                   .data( Qt::ToolTipRole )
                                                   .toString() );

            THEN( "the pattern is shown literally, and nothing is loaded" )
            {
                REQUIRE( shown.text == kHtml );
                REQUIRE( shown.resources.isEmpty() );
            }
        }
    }

    GIVEN( "a rule with such a line pattern and a problem" )
    {
        RuleListModel model;
        model.setEntries( { { "", kHtml, "", true, {} } } );
        const auto problems = model.problems( 0 ).all();
        REQUIRE_FALSE( problems.isEmpty() );

        THEN( "both are shown literally, each on a line of its own" )
        {
            const auto shown = shownAsToolTip( model.index( 0, RuleListModel::LinePatternColumn )
                                                   .data( Qt::ToolTipRole )
                                                   .toString() );
            REQUIRE( shown.text == ( QStringList{ kHtml } + problems ).join( '\n' ) );
            REQUIRE( shown.resources.isEmpty() );
        }
    }

    GIVEN( "a pattern with runs of spaces" )
    {
        RuleListModel model;
        model.setEntries( { { "VIN", "VIN:  \t(\\S+)", "", true, {} } } );

        THEN( "they are shown as they are" )
        {
            REQUIRE( shownAsToolTip( model.index( 0, RuleListModel::LinePatternColumn )
                                         .data( Qt::ToolTipRole )
                                         .toString() )
                         .text
                     == "VIN:  \t(\\S+)" );
        }
    }
}

SCENARIO( "The preview's tooltip shows the file path as it is", "[plaintext][preview]" )
{
    GIVEN( "a preview of a file whose path looks like HTML" )
    {
        FooterScanner::Preview preview;
        preview.status = FooterScanner::Preview::Status::Unreadable;
        preview.filePath = "/logs/" + kHtml + ".log";
        RulePreviewView view;
        view.showPreview( preview );

        THEN( "the path is shown literally, and nothing is loaded" )
        {
            const auto shown = shownAsToolTip( view.toolTip() );
            REQUIRE( shown.text == QDir::toNativeSeparators( preview.filePath ) );
            REQUIRE( shown.resources.isEmpty() );
        }
    }
}

SCENARIO( "The import and export error dialogs show the file path as plain text",
          "[plaintext][footereditor]" )
{
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    auto* editor = new FooterEditor( { { "VIN", "VIN=(\\S+)", "", true, {} } } );
    editor->show();

    GIVEN( "rules exported into a folder, looking like HTML, that does not exist" )
    {
        child<QToolButton>( *editor, "exportButton" )->click();
        auto* dialog = child<QFileDialog>( *editor, "exportDialog" );
        const auto path = dir.path() + "/<b>gone &amp; <img src=x>/rules.json";
        dialog->selectFile( path );
        static_cast<QDialog*>( dialog )->accept(); // QFileDialog::accept(), as OK does

        THEN( "the error shows the path as plain text" )
        {
            auto* box = child<QMessageBox>( *editor, "exportErrorBox" );
            REQUIRE( box->textFormat() == Qt::PlainText );
            REQUIRE( box->text().contains( QFileInfo( path ).absoluteFilePath() ) );
        }
    }

    GIVEN( "rules imported from a file, named like HTML, that is gone" )
    {
        const auto path = dir.path() + "/<b>rules</b> &amp; <img src=x>.json";
        REQUIRE( QMetaObject::invokeMethod( editor, "importFrom", Q_ARG( QString, path ) ) );

        THEN( "the error shows the path as plain text" )
        {
            auto* box = child<QMessageBox>( *editor, "importErrorBox" );
            REQUIRE( box->textFormat() == Qt::PlainText );
            REQUIRE( box->text().contains( path ) );
        }
    }

    delete editor;
}

SCENARIO( "The problem labels show their text as plain text", "[plaintext][footereditor]" )
{
    GIVEN( "the editor with a rule whose line pattern does not compile" )
    {
        FooterEditor editor( { { "VIN", "<b>VIN</b>=(", "", true, {} } } );
        editor.show();
        auto* field = child<QLineEdit>( editor, "linePatternEdit" );
        auto* fieldProblem = child<QLabel>( editor, "linePatternProblem" );
        auto* listProblems = child<QLabel>( editor, "problemLabel" );
        REQUIRE_FALSE( fieldProblem->text().isEmpty() );
        REQUIRE_FALSE( listProblems->text().isEmpty() );

        THEN( "the labels below the field and the list show plain text" )
        {
            REQUIRE( fieldProblem->textFormat() == Qt::PlainText );
            REQUIRE( listProblems->textFormat() == Qt::PlainText );
            for ( const char* name : { "keyProblem", "valuePatternProblem" } ) {
                REQUIRE( child<QLabel>( editor, name )->textFormat() == Qt::PlainText );
            }
        }

        THEN( "the field's tooltip shows the problem literally" )
        {
            const auto shown = shownAsToolTip( field->toolTip() );
            REQUIRE( shown.text == fieldProblem->text() );
            REQUIRE( shown.resources.isEmpty() );
        }
    }
}

SCENARIO( "The template dialog's notes show the keys as plain text", "[plaintext][ruletemplate]" )
{
    GIVEN( "the template dialog, opened over a rule whose key looks like HTML" )
    {
        FooterEditor editor( { { "<b>VIN</b>", "VIN=(\\S+)", "", true, {} } } );
        editor.show();
        child<QToolButton>( editor, "templateButton" )->click();
        auto* dialog = child<RuleTemplateDialog>( editor, "ruleTemplateDialog" );

        THEN( "its notes on the keys show plain text" )
        {
            REQUIRE( child<QLabel>( *dialog, "templateKeyNote" )->textFormat() == Qt::PlainText );
            REQUIRE( child<QLabel>( *dialog, "templateKeyHint" )->textFormat() == Qt::PlainText );
        }
    }
}

SCENARIO( "The footer's tooltips keep escaping the key and the values", "[plaintext][footer]" )
{
    GIVEN( "a mapped value whose key, value and raw value look like HTML" )
    {
        FooterDisplayWidget widget;
        widget.updateValues(
            { { "<b>Key</b>", "<img src=\"file:///dev/zero\"> &amp;", "<i>raw</i> &lt;", 0 } } );
        const auto items = widget.findChildren<FooterValueItem*>();
        REQUIRE( items.size() == 1 );

        THEN( "its tooltip shows them literally, and loads nothing" )
        {
            const auto shown = shownAsToolTip( items[ 0 ]->toolTip() );
            REQUIRE( shown.text.contains( "<b>Key</b>: <img src=\"file:///dev/zero\"> &amp;" ) );
            REQUIRE( shown.text.contains( "Raw value: <i>raw</i> &lt;" ) );
            REQUIRE( shown.resources.isEmpty() );
        }
    }
}
