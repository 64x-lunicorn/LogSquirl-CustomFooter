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
 * @file templatesamples.h
 * @brief What the rule template tests share: a template by name or group,
 *        and the value a template takes from a sample line through the
 *        scanner, as the footer would show it.
 */

#pragma once

#include "footerentry.h"
#include "footerscanner.h"
#include "ruletemplate.h"

#include <catch2/catch.hpp>

#include <QFile>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <algorithm>
#include <initializer_list>
#include <optional>

namespace custom_footer {

/// The template named @p name, in any group; the dialog lists templates by
/// name, so no two have the same.
inline const RuleTemplate& templateNamed( const QString& name )
{
    const auto& templates = ruleTemplates();
    const auto it = std::find_if( templates.begin(), templates.end(),
                                  [ & ]( const RuleTemplate& t ) { return t.name == name; } );
    REQUIRE( it != templates.end() );
    return *it;
}

/// The templates of @p group, in list order.
inline QList<RuleTemplate> templatesIn( const QString& group )
{
    QList<RuleTemplate> inGroup;
    for ( const auto& t : ruleTemplates() ) {
        if ( t.group == group ) {
            inGroup.append( t );
        }
    }
    return inGroup;
}

/// The names of @p templates, in their order.
inline QStringList namesOf( const QList<RuleTemplate>& templates )
{
    QStringList names;
    for ( const auto& t : templates ) {
        names.append( t.name );
    }
    return names;
}

/// The value @p entry extracts from @p line, between a first and a last
/// line, through the scanner, as the footer would show it, mappings
/// applied; nothing if it does not match.
inline std::optional<QString> extract( const FooterEntry& entry, const QString& line )
{
    REQUIRE( FooterScanner::patternError( entry.linePattern ).isEmpty() );
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto path = dir.path() + "/sample.log";
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    file.write( "first line\n" );
    file.write( line.toUtf8() );
    file.write( "\nlast line\n" );
    file.close();

    const auto values = FooterScanner::scan( path, { entry } );
    if ( !values.contains( entry.key ) ) {
        return std::nullopt;
    }
    return values.value( entry.key );
}

/// The value the template named @p templateName extracts from @p line, as
/// extract() of its rule.
inline std::optional<QString> extract( const QString& templateName, const QString& line )
{
    return extract( templateNamed( templateName ).entry(), line );
}

/// A line a rule finds, with the value it takes from it.
struct Sample {
    const char* line;
    const char* value;
};

/// Requires @p entry to take each sample's value from its line.
inline void requireFinds( const FooterEntry& entry, std::initializer_list<Sample> samples )
{
    for ( const auto& sample : samples ) {
        INFO( sample.line );
        REQUIRE( extract( entry, sample.line ) == QString( sample.value ) );
    }
}

/// Requires the template named @p templateName to take each sample's value
/// from its line.
inline void requireFinds( const QString& templateName, std::initializer_list<Sample> samples )
{
    requireFinds( templateNamed( templateName ).entry(), samples );
}

/// Requires @p entry to take no value from any of @p lines.
inline void requireFindsNothing( const FooterEntry& entry,
                                 std::initializer_list<const char*> lines )
{
    for ( const auto* line : lines ) {
        INFO( line );
        REQUIRE( extract( entry, line ) == std::nullopt );
    }
}

/// Requires the template named @p templateName to take no value from any of
/// @p lines.
inline void requireFindsNothing( const QString& templateName,
                                 std::initializer_list<const char*> lines )
{
    requireFindsNothing( templateNamed( templateName ).entry(), lines );
}

} // namespace custom_footer
