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

#include "rulecheck.h"
#include "footerscanner.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace custom_footer {

namespace {

/// The problem of @p pattern: that it is too long, as @p tooLong says, or
/// else why it does not compile, which @p error tells only then.
template <typename Error>
QString patternProblem( const QString& pattern, const QString& tooLong, const QString& invalid,
                        Error error )
{
    if ( pattern.size() > RuleCheck::kMaxPatternLength ) {
        return tooLong.arg( RuleCheck::kMaxPatternLength );
    }
    const QString why = error();
    return why.isEmpty() ? QString() : RuleCheck::tr( "%1: %2" ).arg( invalid, why );
}

/// The problems of @p rule, whose line and value patterns do not compile
/// for the reasons @p linePatternError and @p valuePatternError give, or do
/// where these are empty. They are asked only of patterns not too long.
template <typename LineError, typename ValueError>
RuleProblems problemsWith( const RuleCheck::Rule& rule, LineError linePatternError,
                           ValueError valuePatternError )
{
    const auto& entry = rule.entry;
    RuleProblems problems;
    if ( RuleCheck::isUsed( entry ) && entry.key.trimmed().isEmpty() ) {
        problems.key = RuleCheck::tr( "a rule with a line pattern needs a key" );
    }
    problems.linePattern = patternProblem(
        entry.linePattern, RuleCheck::tr( "line pattern longer than %1 characters" ),
        RuleCheck::tr( "invalid line pattern" ), linePatternError );
    problems.valuePattern = patternProblem(
        entry.valuePattern, RuleCheck::tr( "value pattern longer than %1 characters" ),
        RuleCheck::tr( "invalid value pattern" ), valuePatternError );
    if ( rule.unfinishedSimpleRule ) {
        problems.endCharacter = endCharacterProblem( *rule.unfinishedSimpleRule );
    }
    return problems;
}

} // namespace

QStringList RuleProblems::all() const
{
    QStringList problems;
    for ( const auto* problem : { &key, &linePattern, &valuePattern, &endCharacter } ) {
        if ( !problem->isEmpty() ) {
            problems.append( *problem );
        }
    }
    return problems;
}

QString RuleProblems::preventingMatches() const
{
    for ( const auto* problem : { &linePattern, &valuePattern, &endCharacter } ) {
        if ( !problem->isEmpty() ) {
            return *problem;
        }
    }
    return {};
}

RuleCheck::RuleCheck( PatternCompiler compile )
    : compile_( compile ? std::move( compile ) : &FooterScanner::patternError )
{
}

bool RuleCheck::isUsed( const FooterEntry& entry )
{
    return entry.enabled && !entry.linePattern.isEmpty();
}

RuleProblems RuleCheck::problemsOf( const Rule& rule, const QRegularExpression& linePattern,
                                    const QRegularExpression& valuePattern )
{
    const auto errorOf = []( const QRegularExpression& regex ) {
        return [ &regex ] { return regex.isValid() ? QString() : regex.errorString(); };
    };
    return problemsWith( rule, errorOf( linePattern ), errorOf( valuePattern ) );
}

QStringList RuleCheck::listing( int row, const RuleProblems& problems )
{
    QStringList lines;
    for ( const auto& problem : problems.all() ) {
        lines.append( tr( "Rule %1: %2" ).arg( row + 1 ).arg( problem ) );
    }
    return lines;
}

void RuleCheck::setRules( const QList<Rule>& rules )
{
    rows_.clear();
    insertRules( 0, rules );
}

void RuleCheck::setRule( int row, const Rule& rule )
{
    if ( row >= 0 && row < rows_.size() ) {
        rows_[ row ] = check( rule );
    }
}

void RuleCheck::insertRules( int row, const QList<Rule>& rules )
{
    if ( row < 0 || row > rows_.size() ) {
        return;
    }
    for ( int i = 0; i < rules.size(); ++i ) {
        rows_.insert( row + i, check( rules.at( i ) ) );
    }
}

void RuleCheck::removeRules( int row, int count )
{
    if ( row < 0 || count <= 0 || row + count > rows_.size() ) {
        return;
    }
    rows_.remove( row, count );
}

bool RuleCheck::canMoveRules( int first, int count, int destination ) const
{
    const int size = this->count();
    return count > 0 && first >= 0 && first + count <= size && destination >= 0
           && destination <= size && ( destination < first || destination > first + count );
}

bool RuleCheck::moveRules( int first, int count, int destination )
{
    if ( !canMoveRules( first, count, destination ) ) {
        return false;
    }
    const auto moved = rows_.mid( first, count );
    rows_.remove( first, count );
    const int insertAt = destination > first ? destination - count : destination;
    for ( int i = 0; i < count; ++i ) {
        rows_.insert( insertAt + i, moved.at( i ) );
    }
    return true;
}

RuleProblems RuleCheck::problems( int row ) const
{
    return row >= 0 && row < rows_.size() ? rows_.at( row ).problems : RuleProblems();
}

QStringList RuleCheck::listing() const
{
    QStringList lines;
    for ( int row = 0; row < rows_.size(); ++row ) {
        const auto& problems = rows_.at( row ).problems;
        if ( !problems.isEmpty() ) {
            lines += listing( row, problems );
        }
    }
    return lines;
}

bool RuleCheck::canSave() const
{
    return std::all_of( rows_.begin(), rows_.end(),
                        []( const Row& row ) { return row.problems.isEmpty(); } );
}

QStringList RuleCheck::keysInUse() const
{
    QStringList keys;
    QSet<QString> seen;
    for ( const auto& row : rows_ ) {
        const auto& key = row.rule.entry.key;
        if ( row.used && !seen.contains( key ) ) {
            seen.insert( key );
            keys.append( key );
        }
    }
    return keys;
}

RuleCheck::Row RuleCheck::check( const Rule& rule )
{
    Row row;
    row.rule = rule;
    const auto errorOf = [ this ]( const QString& pattern ) {
        return [ this, &pattern ] { return patternError( pattern ); };
    };
    row.problems = problemsWith( rule, errorOf( rule.entry.linePattern ),
                                 errorOf( rule.entry.valuePattern ) );
    row.used = isUsed( rule.entry ) && row.problems.isEmpty();
    return row;
}

QString RuleCheck::patternError( const QString& pattern )
{
    auto known = patternErrors_.constFind( pattern );
    if ( known == patternErrors_.constEnd() ) {
        known = patternErrors_.insert( pattern, compile_( pattern ) );
    }
    return known.value();
}

} // namespace custom_footer
