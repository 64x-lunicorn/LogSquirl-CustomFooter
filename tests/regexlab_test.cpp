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

#include "eventloopwait.h"
#include "footereditor.h"
#include "hostfunctions.h"
#include "rulelistmodel.h"
#include "rulepreviewer.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>

#include <memory>
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
        , advanced( child<QCheckBox>( editor, "advancedCheck" ) )
        , removeRule( child<QToolButton>( editor, "removeRuleButton" ) )
        , moveDown( child<QToolButton>( editor, "moveDownButton" ) )
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
    QCheckBox* advanced;
    QToolButton* removeRule;
    QToolButton* moveDown;
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

SCENARIO( "FooterEditor's preview follows a pattern applied in the Regex Lab",
          "[footereditor][regexlab]" )
{
    QTemporaryDir logDir;
    REQUIRE( logDir.isValid() );
    const auto path = logDir.path() + "/vin.log";
    {
        QFile file( path );
        REQUIRE( file.open( QIODevice::WriteOnly ) );
        file.write( "VIN=ABC123\nvin: def456\n" );
    }
    FakeRegexLab lab;
    FooterEditor editor( { advancedRule( "VIN", "VIN=(\\w+)" ) } );
    editor.setRegexLab( lab.function() );
    editor.setActiveFile( path );
    Ui ui( editor );
    auto* previewer = child<RulePreviewer>( editor, "rulePreviewer" );
    auto* value = child<QLabel>( editor, "previewValue" );
    previewer->setEditDelay( 0 );
    const auto settle = [ previewer ] {
        previewer->flush();
        REQUIRE( processUntil( [ previewer ] { return !previewer->isBusy(); } ) );
    };
    settle();
    REQUIRE( value->text().contains( "ABC123" ) );

    WHEN( "a pattern that ignores case is applied" )
    {
        ui.testLine->click();
        REQUIRE( lab.apply( "vin: (\\w+)", false ) );
        settle();

        THEN( "the preview shows what it finds" )
        {
            REQUIRE( value->text().contains( "def456" ) );
        }
    }
}

SCENARIO( "FooterEditor maps a rule's (?i) to the Regex Lab's Match case",
          "[footereditor][regexlab]" )
{
    FakeRegexLab lab;

    GIVEN( "a rule whose line pattern starts with (?i)" )
    {
        FooterEditor editor( { advancedRule( "VIN", "(?i)vin=(\\w+)" ) } );
        editor.setRegexLab( lab.function() );
        Ui ui( editor );

        WHEN( "Test… is clicked" )
        {
            ui.testLine->click();

            THEN( "the Lab gets the pattern without it, and Match case off" )
            {
                REQUIRE( lab.pattern.pattern == "vin=(\\w+)" );
                REQUIRE_FALSE( lab.pattern.matchesCase );
            }

            AND_WHEN( "it is applied with Match case still off" )
            {
                REQUIRE( lab.apply( "vin: (\\w+)", false ) );

                THEN( "the rule's pattern has (?i) in front again" )
                {
                    REQUIRE( editor.entries().at( 0 ).linePattern == "(?i)vin: (\\w+)" );
                }
            }

            AND_WHEN( "it is applied with Match case turned on" )
            {
                REQUIRE( lab.apply( "vin=(\\w+)", true ) );

                THEN( "the rule's pattern matches case" )
                {
                    REQUIRE( editor.entries().at( 0 ).linePattern == "vin=(\\w+)" );
                }
            }

            AND_WHEN( "it is applied without Match case, with (?i) already in the pattern" )
            {
                REQUIRE( lab.apply( "(?i)vin=(\\w+)", false ) );

                THEN( "(?i) is not added twice" )
                {
                    REQUIRE( editor.entries().at( 0 ).linePattern == "(?i)vin=(\\w+)" );
                }
            }
        }
    }

    GIVEN( "a rule matching case, tested in the Lab" )
    {
        FooterEditor editor( { advancedRule( "VIN", "VIN=(\\w+)" ) } );
        editor.setRegexLab( lab.function() );
        Ui ui( editor );
        ui.testLine->click();

        WHEN( "the pattern is applied with Match case turned off" )
        {
            REQUIRE( lab.apply( "VIN=(\\w+)", false ) );

            THEN( "the rule's pattern gets (?i) in front" )
            {
                REQUIRE( editor.entries().at( 0 ).linePattern == "(?i)VIN=(\\w+)" );
            }
        }

        WHEN( "an empty pattern is applied with Match case turned off" )
        {
            REQUIRE( lab.apply( "", false ) );

            THEN( "the pattern stays empty" )
            {
                REQUIRE( editor.entries().at( 0 ).linePattern.isEmpty() );
            }
        }
    }

    GIVEN( "a rule starting with PCRE start-of-pattern items, tested in the Lab" )
    {
        FooterEditor editor( { advancedRule( "VIN", "(*CRLF)(*UTF)VIN=(\\w+)" ) } );
        editor.setRegexLab( lab.function() );
        Ui ui( editor );
        ui.testLine->click();
        REQUIRE( lab.pattern.pattern == "(*CRLF)(*UTF)VIN=(\\w+)" );
        REQUIRE( lab.pattern.matchesCase );

        WHEN( "it is applied with Match case turned off" )
        {
            REQUIRE( lab.apply( "(*CRLF)(*UTF)VIN=(\\w+)", false ) );

            THEN( "(?i) goes after those items, which must come first, and the pattern is valid" )
            {
                REQUIRE( editor.entries().at( 0 ).linePattern == "(*CRLF)(*UTF)(?i)VIN=(\\w+)" );
                REQUIRE( ui.linePatternProblem->isHidden() );
            }

            AND_WHEN( "it is tested again" )
            {
                ui.testLine->click();

                THEN( "the Lab gets it without (?i), and Match case off" )
                {
                    REQUIRE( lab.pattern.pattern == "(*CRLF)(*UTF)VIN=(\\w+)" );
                    REQUIRE_FALSE( lab.pattern.matchesCase );
                }
            }
        }
    }

    GIVEN( "a rule whose pattern is just (?i)" )
    {
        FooterEditor editor( { advancedRule( "VIN", "(?i)" ) } );
        editor.setRegexLab( lab.function() );
        Ui ui( editor );

        WHEN( "it is tested and applied unchanged" )
        {
            ui.testLine->click();
            REQUIRE( lab.pattern.pattern == "(?i)" );
            REQUIRE( lab.apply( lab.pattern.pattern, lab.pattern.matchesCase ) );

            THEN( "it is kept as it is, not emptied" )
            {
                REQUIRE( editor.entries().at( 0 ).linePattern == "(?i)" );
            }
        }
    }
}

SCENARIO( "FooterEditor tests a simple rule's generated pattern", "[footereditor][regexlab]" )
{
    FakeRegexLab lab;
    FooterEditor editor( { advancedRule( "VIN", "VIN:\\s*(\\S+)" ) } );
    editor.setRegexLab( lab.function() );
    Ui ui( editor );
    REQUIRE_FALSE( ui.advanced->isChecked() );

    THEN( "Test… is offered for the generated line pattern, not for the value pattern" )
    {
        REQUIRE( ui.testLine->isEnabled() );
        REQUIRE_FALSE( ui.testValue->isEnabled() );
        REQUIRE( ui.testValue->toolTip().contains( "Advanced" ) );
    }

    WHEN( "the generated pattern is tested and applied unchanged" )
    {
        ui.testLine->click();
        REQUIRE( lab.pattern.pattern == "VIN:\\s*(\\S+)" );
        REQUIRE( lab.apply( "VIN:\\s*(\\S+)" ) );

        THEN( "the rule stays in simple mode" )
        {
            REQUIRE_FALSE( ui.advanced->isChecked() );
            REQUIRE( ui.linePattern->isReadOnly() );
        }
    }

    WHEN( "the simple fields are edited while the Lab is open, and it is applied unchanged" )
    {
        ui.testLine->click();
        child<QLineEdit>( editor, "textBeforeEdit" )->setText( "Serial:" );
        REQUIRE( lab.apply( "VIN:\\s*(\\S+)" ) );

        THEN( "nothing changes: the rule stays simple, with the edited text" )
        {
            REQUIRE_FALSE( ui.advanced->isChecked() );
            REQUIRE( editor.entries().at( 0 ).linePattern == "Serial:\\s*(\\S+)" );
        }
    }

    WHEN( "a changed pattern is applied" )
    {
        ui.testLine->click();
        REQUIRE( lab.apply( "VIN:\\s*([A-Z0-9]{17})" ) );

        THEN( "the rule switches to advanced mode with that pattern" )
        {
            REQUIRE( ui.advanced->isChecked() );
            REQUIRE_FALSE( ui.linePattern->isReadOnly() );
            REQUIRE( ui.linePattern->text() == "VIN:\\s*([A-Z0-9]{17})" );
            REQUIRE( editor.entries().at( 0 ).linePattern == "VIN:\\s*([A-Z0-9]{17})" );
            REQUIRE( ui.testValue->isEnabled() );
        }
    }
}

SCENARIO( "FooterEditor offers Test… only when the Regex Lab can open", "[footereditor][regexlab]" )
{
    GIVEN( "an editor on a host without the Regex Lab" )
    {
        FooterEditor editor( { advancedRule( "VIN", "VIN=(\\w+)", "=(\\w+)$" ) } );
        Ui ui( editor );

        THEN( "both Test… buttons are disabled, saying what they need" )
        {
            REQUIRE_FALSE( ui.testLine->isEnabled() );
            REQUIRE_FALSE( ui.testValue->isEnabled() );
            REQUIRE( ui.testLine->toolTip() == "Needs LogSquirl 26.11 or later" );
            REQUIRE( ui.testValue->toolTip() == "Needs LogSquirl 26.11 or later" );
        }

        WHEN( "the line pattern is typed" )
        {
            ui.linePattern->setText( "VIN: (\\w+)" );

            THEN( "the rule is edited as before" )
            {
                REQUIRE( editor.entries().at( 0 ).linePattern == "VIN: (\\w+)" );
                REQUIRE( ui.buttons->button( QDialogButtonBox::Ok )->isEnabled() );
            }
        }

        WHEN( "another rule is shown" )
        {
            editor.appendEntries( { advancedRule( "Mode", "Mode=(\\w+)" ) } );
            ui.select( 1 );

            THEN( "Test… stays disabled" )
            {
                REQUIRE_FALSE( ui.testLine->isEnabled() );
                REQUIRE_FALSE( ui.testValue->isEnabled() );
            }
        }
    }

    GIVEN( "an editor with the Regex Lab open for a pattern" )
    {
        FakeRegexLab lab;
        FooterEditor editor( { advancedRule( "VIN", "VIN=(\\w+)", "=(\\w+)$" ) } );
        editor.setRegexLab( lab.function() );
        Ui ui( editor );
        ui.testLine->click();

        THEN( "Test… is disabled for both patterns until the Lab answers" )
        {
            REQUIRE_FALSE( ui.testLine->isEnabled() );
            REQUIRE_FALSE( ui.testValue->isEnabled() );
            REQUIRE( ui.testLine->toolTip().contains( "The Regex Lab is open" ) );
        }

        WHEN( "another rule is selected" )
        {
            editor.appendEntries( { advancedRule( "Mode", "Mode=(\\w+)" ) } );
            ui.select( 1 );

            THEN( "Test… stays disabled" )
            {
                REQUIRE_FALSE( ui.testLine->isEnabled() );
                REQUIRE( lab.opened == 1 );
            }
        }
    }

    GIVEN( "a Regex Lab that does not open" )
    {
        FakeRegexLab lab;
        lab.opens = false;
        FooterEditor editor( { advancedRule( "VIN", "VIN=(\\w+)" ) } );
        editor.setRegexLab( lab.function() );
        Ui ui( editor );

        WHEN( "Test… is clicked" )
        {
            ui.testLine->click();

            THEN( "nothing changes, and Test… is offered again" )
            {
                REQUIRE( lab.opened == 1 );
                REQUIRE( ui.testLine->isEnabled() );
                REQUIRE( editor.entries().at( 0 ).linePattern == "VIN=(\\w+)" );
            }
        }
    }
}

SCENARIO( "FooterEditor writes the Regex Lab's answer into the rule it was opened for",
          "[footereditor][regexlab]" )
{
    FakeRegexLab lab;
    auto editor = std::make_unique<FooterEditor>( QList<FooterEntry>{
        advancedRule( "VIN", "VIN=(\\w+)" ), advancedRule( "Mode", "Mode=(\\w+)" ),
        advancedRule( "Build", "Build=(\\w+)" ) } );
    editor->setRegexLab( lab.function() );
    Ui ui( *editor );
    const auto patterns = [ &editor ] {
        QStringList all;
        for ( const auto& entry : editor->entries() ) {
            all.append( entry.linePattern );
        }
        return all;
    };

    GIVEN( "the Lab open for the second rule's line pattern" )
    {
        ui.select( 1 );
        ui.testLine->click();
        REQUIRE( lab.pattern.pattern == "Mode=(\\w+)" );

        WHEN( "another rule is selected before the pattern is applied" )
        {
            ui.select( 0 );
            REQUIRE( lab.apply( "Mode: (\\w+)" ) );

            THEN( "the second rule gets it, and the shown rule is unchanged" )
            {
                REQUIRE( patterns()
                         == QStringList{ "VIN=(\\w+)", "Mode: (\\w+)", "Build=(\\w+)" } );
                REQUIRE( ui.linePattern->text() == "VIN=(\\w+)" );
            }

            AND_WHEN( "the second rule is selected again" )
            {
                ui.select( 1 );

                THEN( "the panel shows the applied pattern" )
                {
                    REQUIRE( ui.linePattern->text() == "Mode: (\\w+)" );
                }
            }
        }

        WHEN( "another rule is selected and an invalid pattern applied" )
        {
            ui.select( 0 );
            REQUIRE( lab.apply( "Mode=(" ) );

            THEN( "the second rule is marked in the list, and OK is blocked" )
            {
                REQUIRE( patterns()[ 1 ] == "Mode=(" );
                REQUIRE( ui.list->model()
                             ->index( 1, RuleListModel::KeyColumn )
                             .data( Qt::BackgroundRole )
                             .isValid() );
                REQUIRE_FALSE( ui.buttons->button( QDialogButtonBox::Ok )->isEnabled() );
            }
        }

        WHEN( "the rule is moved down before the pattern is applied" )
        {
            ui.moveDown->click();
            REQUIRE( patterns()[ 2 ] == "Mode=(\\w+)" );
            REQUIRE( lab.apply( "Mode: (\\w+)" ) );

            THEN( "the moved rule gets it, in its new place" )
            {
                REQUIRE( patterns()
                         == QStringList{ "VIN=(\\w+)", "Build=(\\w+)", "Mode: (\\w+)" } );
            }
        }

        WHEN( "the rule above it is removed before the pattern is applied" )
        {
            ui.select( 0 );
            ui.removeRule->click();
            REQUIRE( patterns() == QStringList{ "Mode=(\\w+)", "Build=(\\w+)" } );
            REQUIRE( lab.apply( "Mode: (\\w+)" ) );

            THEN( "the rule it was opened for gets it, in its new place" )
            {
                REQUIRE( patterns() == QStringList{ "Mode: (\\w+)", "Build=(\\w+)" } );
            }
        }

        WHEN( "the pattern field is edited and the pattern applied unchanged" )
        {
            ui.linePattern->setText( "Mode := (\\w+)" );
            REQUIRE( lab.apply( "Mode=(\\w+)" ) );

            THEN( "nothing changes: the edit is kept" )
            {
                REQUIRE( patterns()[ 1 ] == "Mode := (\\w+)" );
            }
        }

        WHEN( "the rule is removed before the pattern is applied" )
        {
            ui.removeRule->click();
            REQUIRE( patterns() == QStringList{ "VIN=(\\w+)", "Build=(\\w+)" } );
            REQUIRE( lab.apply( "Mode: (\\w+)" ) );

            THEN( "the answer is dropped: no other rule gets it, and Test… is offered again" )
            {
                REQUIRE( patterns() == QStringList{ "VIN=(\\w+)", "Build=(\\w+)" } );
                REQUIRE( ui.testLine->isEnabled() );
            }
        }

        WHEN( "the pattern field is edited before the pattern is applied" )
        {
            ui.linePattern->setText( "Mode := (\\w+)" );
            REQUIRE( lab.apply( "Mode: (\\w+)" ) );

            THEN( "the applied pattern wins, as the user's last word" )
            {
                REQUIRE( patterns()[ 1 ] == "Mode: (\\w+)" );
            }
        }

        WHEN( "the editor is closed before the pattern is applied" )
        {
            editor->reject();
            REQUIRE( lab.apply( "Mode: (\\w+)" ) );

            THEN( "the answer is dropped" )
            {
                REQUIRE( patterns() == QStringList{ "VIN=(\\w+)", "Mode=(\\w+)", "Build=(\\w+)" } );
            }
        }

        WHEN( "the editor is destroyed before the Lab answers" )
        {
            editor.reset();

            THEN( "the host never answers, as the editor was the Lab's context" )
            {
                REQUIRE_FALSE( lab.context );
                REQUIRE_FALSE( lab.apply( "Mode: (\\w+)" ) );
            }
        }
    }
}
