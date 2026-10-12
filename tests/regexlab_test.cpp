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
 * @file regexlab_test.cpp
 * @brief BDD tests for Test… in the rule editor, which opens a rule's
 *        pattern in LogSquirl's Regex Lab, with a fake Regex Lab.
 */

#include <catch2/catch.hpp>

#include "footereditor.h"
#include "hostfunctions.h"
#include "rulelistmodel.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTableView>
#include <QToolButton>

#include <optional>

using namespace custom_footer;

namespace {

/**
 * A Regex Lab as the host offers it: it records what it was opened with
 * and keeps the answer function, which the test calls as the user's Apply
 * or Cancel. Like the host, it never answers once the context is gone.
 */
struct FakeRegexLab {
    int opened = 0;
    RegexLabPattern pattern;
    QPointer<const QObject> context;
    RegexLabDone done;
    /// What opening returns: false as if the Lab could not open.
    bool opens = true;

    OpenRegexLab function()
    {
        return
            [ this ]( const RegexLabPattern& asked, const QObject* askedBy, RegexLabDone answer ) {
                ++opened;
                pattern = asked;
                context = askedBy;
                if ( !opens ) {
                    return false;
                }
                done = std::move( answer );
                return true;
            };
    }

    bool isOpen() const
    {
        return static_cast<bool>( done );
    }

    /// The user's answer; returns whether it reached the plugin.
    bool answer( const std::optional<RegexLabPattern>& applied )
    {
        REQUIRE( done );
        auto answerOnce = std::move( done );
        done = nullptr;
        if ( !context ) {
            return false;
        }
        answerOnce( applied );
        return true;
    }

    bool apply( const QString& pattern, bool matchesCase = true )
    {
        return answer( RegexLabPattern{ pattern, matchesCase } );
    }

    bool cancel()
    {
        return answer( std::nullopt );
    }
};

template <typename Widget>
Widget* child( QObject& parent, const char* name )
{
    auto* widget = parent.findChild<Widget*>( name );
    REQUIRE( widget );
    return widget;
}

/// The editor's widgets the tests use, by object name.
struct Ui {
    explicit Ui( FooterEditor& editor )
        : list( child<QTableView>( editor, "ruleList" ) )
        , testLine( child<QToolButton>( editor, "testLinePatternButton" ) )
        , testValue( child<QToolButton>( editor, "testValuePatternButton" ) )
        , linePattern( child<QLineEdit>( editor, "linePatternEdit" ) )
        , valuePattern( child<QLineEdit>( editor, "valuePatternEdit" ) )
        , linePatternProblem( child<QLabel>( editor, "linePatternProblem" ) )
        , buttons( editor.findChild<QDialogButtonBox*>() )
    {
        REQUIRE( buttons );
    }

    void select( int row ) const
    {
        list->setCurrentIndex( list->model()->index( row, RuleListModel::KeyColumn ) );
    }

    QTableView* list;
    QToolButton* testLine;
    QToolButton* testValue;
    QLineEdit* linePattern;
    QLineEdit* valuePattern;
    QLabel* linePatternProblem;
    QDialogButtonBox* buttons;
};

FooterEntry advancedRule( const QString& key, const QString& linePattern,
                          const QString& valuePattern = {} )
{
    return { key, linePattern, valuePattern, true, {} };
}

} // namespace

SCENARIO( "FooterEditor tests a rule's pattern in the Regex Lab", "[footereditor][regexlab]" )
{
    FakeRegexLab lab;

    GIVEN( "an editor with an advanced rule and the host's Regex Lab" )
    {
        FooterEditor editor( { advancedRule( "VIN", "VIN=(\\w+)", "=(\\w+)$" ) } );
        editor.setRegexLab( lab.function() );
        Ui ui( editor );

        THEN( "Test… is offered for the line pattern and the value pattern" )
        {
            REQUIRE( ui.testLine->isEnabled() );
            REQUIRE( ui.testValue->isEnabled() );
            REQUIRE( ui.testLine->text() == "Test…" );
        }

        WHEN( "Test… of the line pattern is clicked" )
        {
            ui.testLine->click();

            THEN( "the Regex Lab opens with the line pattern, matching case, for the editor" )
            {
                REQUIRE( lab.opened == 1 );
                REQUIRE( lab.pattern.pattern == "VIN=(\\w+)" );
                REQUIRE( lab.pattern.matchesCase );
                REQUIRE( lab.context == &editor );
            }

            AND_WHEN( "the user applies a changed pattern" )
            {
                REQUIRE( lab.apply( "VIN: (\\w+)" ) );

                THEN( "the rule has it, as if it had been typed" )
                {
                    REQUIRE( ui.linePattern->text() == "VIN: (\\w+)" );
                    REQUIRE( editor.entries().at( 0 ).linePattern == "VIN: (\\w+)" );
                    REQUIRE( editor.entries().at( 0 ).valuePattern == "=(\\w+)$" );
                    REQUIRE( ui.list->model()
                                 ->index( 0, RuleListModel::LinePatternColumn )
                                 .data()
                                 .toString()
                             == "VIN: (\\w+)" );
                }
            }

            AND_WHEN( "the user applies an invalid pattern" )
            {
                REQUIRE( lab.apply( "VIN=(" ) );

                THEN( "it is marked, and OK and Apply are blocked, as for a typed one" )
                {
                    REQUIRE( editor.entries().at( 0 ).linePattern == "VIN=(" );
                    REQUIRE_FALSE( ui.linePatternProblem->isHidden() );
                    REQUIRE_FALSE( ui.buttons->button( QDialogButtonBox::Ok )->isEnabled() );
                    REQUIRE_FALSE( ui.buttons->button( QDialogButtonBox::Apply )->isEnabled() );
                }
            }

            AND_WHEN( "the user cancels the Lab, or closes it" )
            {
                REQUIRE( lab.cancel() );

                THEN( "the rule is unchanged, and Test… is offered again" )
                {
                    REQUIRE( editor.entries().at( 0 ).linePattern == "VIN=(\\w+)" );
                    REQUIRE( ui.linePattern->text() == "VIN=(\\w+)" );
                    REQUIRE( ui.testLine->isEnabled() );
                    REQUIRE( ui.testValue->isEnabled() );
                }
            }
        }

        WHEN( "Test… of the value pattern is clicked and a pattern applied" )
        {
            ui.testValue->click();
            REQUIRE( lab.pattern.pattern == "=(\\w+)$" );
            REQUIRE( lab.apply( "=(\\d+)$" ) );

            THEN( "the value pattern has it, and the line pattern is unchanged" )
            {
                REQUIRE( ui.valuePattern->text() == "=(\\d+)$" );
                REQUIRE( editor.entries().at( 0 ).valuePattern == "=(\\d+)$" );
                REQUIRE( editor.entries().at( 0 ).linePattern == "VIN=(\\w+)" );
            }
        }
    }
}
