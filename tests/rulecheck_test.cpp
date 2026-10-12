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
 * @file rulecheck_test.cpp
 * @brief The rule check: which rules are usable, why not, and how the
 *        scanner agrees with it. No widgets.
 */

#include <catch2/catch.hpp>

#include "footerscanner.h"
#include "rulecheck.h"

#include <QStringList>

#include <optional>
#include <vector>

using namespace custom_footer;

namespace {

FooterEntry rule( const QString& key, const QString& linePattern,
                  const QString& valuePattern = QString(), bool enabled = true )
{
    return { key, linePattern, valuePattern, enabled, {} };
}

/// A simple rule ending at a character that is not given yet.
SimpleRule withoutEndCharacter()
{
    SimpleRule simple;
    simple.textBefore = "user=";
    simple.valueEnd = ValueEnd::Character;
    return simple;
}

const QString kNeedsKey = "a rule with a line pattern needs a key";

QString invalidLine( const QString& pattern )
{
    return "invalid line pattern: " + FooterScanner::patternError( pattern );
}

QString invalidValue( const QString& pattern )
{
    return "invalid value pattern: " + FooterScanner::patternError( pattern );
}

const QString kNoEndCharacter = endCharacterProblem( withoutEndCharacter() );

struct Case {
    const char* name;
    QList<RuleCheck::Rule> rules;
    QList<RuleProblems> problems; ///< Per row.
    QStringList listing;
    QStringList keysInUse;
    QStringList preventingMatches; ///< Per row.
};

std::vector<Case> cases()
{
    return {
        { "valid rules, two of them sharing a key",
          { rule( "A", "a=(\\S+)" ), rule( "B", "b=(\\S+)", "(\\d+)" ), rule( "A", "alt (\\S+)" ) },
          { {}, {}, {} },
          {},
          { "A", "B" },
          { "", "", "" } },
        { "an enabled rule with a line pattern but no key",
          { rule( "", "a=(\\S+)" ), rule( "  ", "b=(\\S+)" ) },
          { { kNeedsKey, "", "", "" }, { kNeedsKey, "", "", "" } },
          { "Rule 1: " + kNeedsKey, "Rule 2: " + kNeedsKey },
          {},
          { "", "" } },
        { "rules without a key that need none",
          { rule( "", "a=(\\S+)", "", false ), rule( "", "" ), rule( "K", "" ) },
          { {}, {}, {} },
          {},
          {},
          { "", "", "" } },
        { "invalid patterns, also of a disabled rule",
          { rule( "A", "[unclosed" ), rule( "B", "b", "(" ), rule( "C", "(", "", false ) },
          { { "", invalidLine( "[unclosed" ), "", "" },
            { "", "", invalidValue( "(" ), "" },
            { "", invalidLine( "(" ), "", "" } },
          { "Rule 1: " + invalidLine( "[unclosed" ), "Rule 2: " + invalidValue( "(" ),
            "Rule 3: " + invalidLine( "(" ) },
          {},
          { invalidLine( "[unclosed" ), invalidValue( "(" ), invalidLine( "(" ) } },
        { "a rule with every pattern problem",
          { rule( "", "(", "[" ) },
          { { kNeedsKey, invalidLine( "(" ), invalidValue( "[" ), "" } },
          { "Rule 1: " + kNeedsKey, "Rule 1: " + invalidLine( "(" ),
            "Rule 1: " + invalidValue( "[" ) },
          {},
          { invalidLine( "(" ) } },
        { "an unfinished simple rule among valid ones",
          { rule( "A", "a=(\\S+)" ), { rule( "User", "" ), withoutEndCharacter() } },
          { {}, { "", "", "", kNoEndCharacter } },
          { "Rule 2: " + kNoEndCharacter },
          { "A" },
          { "", kNoEndCharacter } },
        { "an unfinished simple rule with an invalid value pattern",
          { { rule( "User", "", "(" ), withoutEndCharacter() } },
          { { "", "", invalidValue( "(" ), kNoEndCharacter } },
          { "Rule 1: " + invalidValue( "(" ), "Rule 1: " + kNoEndCharacter },
          {},
          { invalidValue( "(" ) } },
        { "no rules", {}, {}, {}, {}, {} },
    };
}

/// Compiles like the scanner and counts how often it does.
struct CountingCompiler {
    int* count;

    QString operator()( const QString& pattern ) const
    {
        ++*count;
        return FooterScanner::patternError( pattern );
    }
};

QList<RuleCheck::Rule> manyRules( int count )
{
    QList<RuleCheck::Rule> rules;
    for ( int i = 0; i < count; ++i ) {
        const auto key = QString( "Key%1" ).arg( i );
        rules.append( rule( key, QString( "%1:\\s+(\\S+)" ).arg( key ),
                            QString( "(?<=%1:)\\s*(\\w+)" ).arg( key ) ) );
    }
    return rules;
}

} // namespace

SCENARIO( "RuleCheck lists why rules cannot be used", "[rulecheck]" )
{
    for ( const auto& c : cases() ) {
        GIVEN( c.name )
        {
            RuleCheck check;
            check.setRules( c.rules );

            THEN( "each rule has its problems, listed by rule number in one order" )
            {
                REQUIRE( check.count() == c.rules.size() );
                for ( int row = 0; row < c.rules.size(); ++row ) {
                    INFO( "row " << row );
                    REQUIRE( check.problems( row ) == c.problems.at( row ) );
                    REQUIRE( check.problems( row ).preventingMatches()
                             == c.preventingMatches.at( row ) );
                    REQUIRE( RuleCheck::problemsOf( c.rules.at( row ) ) == c.problems.at( row ) );
                }
                REQUIRE( check.listing() == c.listing );
                REQUIRE( check.canSave() == c.listing.isEmpty() );
                REQUIRE( check.keysInUse() == c.keysInUse );
            }

            THEN( "the scanner reports exactly the problems of the rules it would use" )
            {
                QList<FooterEntry> entries;
                QStringList reported;
                for ( int row = 0; row < c.rules.size(); ++row ) {
                    const auto& entry = c.rules.at( row ).entry;
                    entries.append( entry );
                    if ( RuleCheck::isUsed( entry ) ) {
                        reported += RuleCheck::listing( row, RuleCheck::problemsOf( entry ) );
                    }
                }
                REQUIRE( FooterScanner( entries ).problems() == reported );
            }
        }
    }
}

SCENARIO( "RuleCheck checks only what changed, compiling each pattern once", "[rulecheck]" )
{
    GIVEN( "a check of 200 rules with 400 distinct patterns" )
    {
        int compilations = 0;
        RuleCheck check( CountingCompiler{ &compilations } );
        const auto rules = manyRules( 200 );
        check.setRules( rules );
        REQUIRE( compilations == 400 );
        REQUIRE( check.canSave() );

        WHEN( "a key is typed into one rule a character at a time" )
        {
            auto edited = rules.at( 100 );
            for ( const auto& key : { "", "N", "Ne", "New" } ) {
                edited.entry.key = key;
                check.setRule( 100, edited );
            }

            THEN( "no pattern is compiled again" )
            {
                REQUIRE( compilations == 400 );
                REQUIRE( check.canSave() );
                REQUIRE( check.keysInUse().contains( "New" ) );
                REQUIRE_FALSE( check.keysInUse().contains( "Key100" ) );
            }
        }

        WHEN( "a pattern of one rule becomes invalid" )
        {
            auto edited = rules.at( 150 );
            edited.entry.valuePattern = "(";
            check.setRule( 150, edited );

            THEN( "only that pattern is compiled, and only that rule has a problem" )
            {
                REQUIRE( compilations == 401 );
                REQUIRE( check.problems( 150 ).valuePattern == invalidValue( "(" ) );
                REQUIRE( check.problems( 149 ).isEmpty() );
                REQUIRE( check.problems( 151 ).isEmpty() );
                REQUIRE( check.listing() == QStringList{ "Rule 151: " + invalidValue( "(" ) } );
                REQUIRE_FALSE( check.canSave() );
                REQUIRE_FALSE( check.keysInUse().contains( "Key150" ) );
            }

            AND_WHEN( "another rule gets the same invalid pattern" )
            {
                auto other = rules.at( 3 );
                other.entry.valuePattern = "(";
                check.setRule( 3, other );

                THEN( "its error is remembered, not compiled again" )
                {
                    REQUIRE( compilations == 401 );
                    REQUIRE( check.listing().size() == 2 );
                }
            }

            AND_WHEN( "rules are moved past it and removed above it" )
            {
                check.moveRules( 150, 1, 0 );
                check.moveRules( 10, 5, 200 );
                check.removeRules( 1, 2 );

                THEN( "nothing is compiled, and the problem is renumbered" )
                {
                    REQUIRE( compilations == 401 );
                    REQUIRE( check.count() == 198 );
                    REQUIRE( check.listing() == QStringList{ "Rule 1: " + invalidValue( "(" ) } );
                }
            }

            AND_WHEN( "it is fixed again" )
            {
                check.setRule( 150, rules.at( 150 ) );

                THEN( "nothing is compiled, and the rules can be saved" )
                {
                    REQUIRE( compilations == 401 );
                    REQUIRE( check.canSave() );
                    REQUIRE( check.listing().isEmpty() );
                }
            }
        }

        WHEN( "rules are inserted" )
        {
            check.insertRules( 0, { rule( "", "new (\\S+)" ), rules.at( 7 ).entry } );

            THEN( "only the patterns not seen before are compiled, and rules are renumbered" )
            {
                // The new rule's line pattern, and its empty value pattern.
                REQUIRE( compilations == 402 );
                REQUIRE( check.count() == 202 );
                REQUIRE( check.listing() == QStringList{ "Rule 1: " + kNeedsKey } );
                REQUIRE( check.problems( 2 ).isEmpty() );
            }
        }
    }
}

SCENARIO( "RuleCheck moves rules as a model moves rows", "[rulecheck]" )
{
    GIVEN( "four rules, the second and the fourth with a problem" )
    {
        RuleCheck check;
        check.setRules( { rule( "A", "a" ), rule( "", "b" ), rule( "C", "c" ), rule( "D", "(" ) } );
        REQUIRE( check.listing().size() == 2 );

        WHEN( "the second rule is moved to the end" )
        {
            check.moveRules( 1, 1, 4 );

            THEN( "its problem follows it" )
            {
                REQUIRE( check.problems( 3 ).key == kNeedsKey );
                REQUIRE( check.problems( 2 ).linePattern == invalidLine( "(" ) );
                REQUIRE(
                    check.listing()
                    == QStringList{ "Rule 3: " + invalidLine( "(" ), "Rule 4: " + kNeedsKey } );
                REQUIRE( check.keysInUse() == QStringList{ "A", "C" } );
            }
        }

        WHEN( "the last two rules are moved to the start" )
        {
            check.moveRules( 2, 2, 0 );

            THEN( "they keep their order and their problems" )
            {
                REQUIRE( check.problems( 0 ).isEmpty() );
                REQUIRE( check.problems( 1 ).linePattern == invalidLine( "(" ) );
                REQUIRE( check.problems( 3 ).key == kNeedsKey );
                REQUIRE( check.keysInUse() == QStringList{ "C", "A" } );
            }
        }

        WHEN( "a row out of range is asked for" )
        {
            THEN( "it has no problems" )
            {
                REQUIRE( check.problems( -1 ).isEmpty() );
                REQUIRE( check.problems( 4 ).isEmpty() );
            }
        }
    }
}
