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
 * @file ruletemplate_testrun_test.cpp
 * @brief The Test run template group: what each pytest template finds.
 *
 * The sample lines are pytest output quoted in issues of pytest-dev/pytest;
 * the comment next to each names the pytest version it comes from.
 */

#include <catch2/catch.hpp>

#include "footerscanner.h"
#include "ruletemplate.h"
#include "simplerule.h"

#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <optional>

using namespace custom_footer;

namespace {

const RuleTemplate& templateNamed( const QString& name )
{
    const auto& templates = ruleTemplates();
    const auto it = std::find_if( templates.begin(), templates.end(),
                                  [ & ]( const RuleTemplate& t ) { return t.name == name; } );
    REQUIRE( it != templates.end() );
    return *it;
}

/// The values @p entries extract from a file of @p lines, through the
/// scanner, as the footer would show them.
QMap<QString, QString> scanLines( const QList<FooterEntry>& entries, const QStringList& lines )
{
    for ( const auto& entry : entries ) {
        REQUIRE( FooterScanner::patternError( entry.linePattern ).isEmpty() );
    }
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto path = dir.path() + "/sample.log";
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    for ( const auto& line : lines ) {
        file.write( line.toUtf8() );
        file.write( "\n" );
    }
    file.close();
    return FooterScanner::scan( path, entries );
}

/// The value the template named @p templateName extracts from @p line;
/// nothing if it does not match.
std::optional<QString> extract( const QString& templateName, const QString& line )
{
    const auto entry = templateNamed( templateName ).entry();
    const auto values = scanLines( { entry }, { "first line", line, "last line" } );
    if ( !values.contains( entry.key ) ) {
        return std::nullopt;
    }
    return values.value( entry.key );
}

/// Lines a template finds, with the value it takes from each.
struct Sample {
    const char* line;
    const char* value;
};

void requireFinds( const QString& templateName, std::initializer_list<Sample> samples )
{
    for ( const auto& sample : samples ) {
        INFO( sample.line );
        REQUIRE( extract( templateName, sample.line ) == QString( sample.value ) );
    }
}

void requireFindsNothing( const QString& templateName, std::initializer_list<const char*> lines )
{
    for ( const auto* line : lines ) {
        INFO( line );
        REQUIRE( extract( templateName, line ) == std::nullopt );
    }
}

/// The names of the templates of @p group, in list order.
QStringList namesIn( const QString& group )
{
    QStringList names;
    for ( const auto& t : ruleTemplates() ) {
        if ( t.group == group ) {
            names.append( t.name );
        }
    }
    return names;
}

} // namespace

SCENARIO( "The Test run group holds the pytest templates", "[ruletemplate]" )
{
    THEN( "Python version, pytest version and Test result, in this order" )
    {
        REQUIRE( namesIn( "Test run" )
                 == QStringList{ "Python version", "pytest version", "Test result" } );
    }

    THEN( "each has its key" )
    {
        REQUIRE( templateNamed( "Python version" ).key == "Python" );
        REQUIRE( templateNamed( "pytest version" ).key == "pytest" );
        REQUIRE( templateNamed( "Test result" ).key == "Test result" );
    }

    THEN( "the versions follow a fixed text and are simple rules" )
    {
        for ( const auto* name : { "Python version", "pytest version" } ) {
            INFO( name );
            const auto& t = templateNamed( name );
            REQUIRE( t.isSimple() );
            const auto entry = t.entry();
            REQUIRE( simpleRuleOf( entry ).has_value() );
            REQUIRE( entry.linePattern == simpleLinePattern( *simpleRuleOf( entry ) ) );
        }
    }

    THEN( "the test result needs the shape of the summary and is advanced" )
    {
        const auto& t = templateNamed( "Test result" );
        REQUIRE( !t.isSimple() );
        REQUIRE( !simpleRuleOf( t.entry() ).has_value() );
    }

    THEN( "the test result says it is meant for one run per file" )
    {
        REQUIRE( templateNamed( "Test result" ).description.contains( "one run per file" ) );
    }
}

SCENARIO( "The Python version template finds the version in the pytest header", "[ruletemplate]" )
{
    THEN( "the version after -- Python, up to the comma" )
    {
        requireFinds(
            "Python version",
            { // pytest 9.0.3
              { "platform linux -- Python 3.14.3, pytest-9.0.3, pluggy-1.6.0", "3.14.3" },
              // pytest 8.3.3
              { "platform win32 -- Python 3.11.4, pytest-8.3.3, pluggy-1.5.0", "3.11.4" },
              // pytest 9.1.1, with -v
              { "platform darwin -- Python 3.14.4, pytest-9.1.1, pluggy-1.6.0 -- "
                "[...]/.venv/bin/python3.14",
                "3.14.4" },
              // pytest 4.6.9
              { "platform linux -- Python 3.8.5, pytest-4.6.9, py-1.8.1, pluggy-0.13.0", "3.8.5" },
              // pytest 7.2.0.dev12+g22756c28e on PyPy: the PyPy version is part
              // of it
              { "platform linux -- Python 3.9.10[pypy-7.3.8-final], "
                "pytest-7.2.0.dev12+g22756c28e, pluggy-1.0.0",
                "3.9.10[pypy-7.3.8-final]" } } );
    }

    THEN( "not without -- before it, in lower case, or in the path pytest -v adds" )
    {
        requireFindsNothing(
            "Python version",
            { "Python 3.12.3", "platform linux -- python 3.12.3, pytest-8.3.2", "-- Python",
              // the end of a pytest -v platform line: the interpreter's path
              "pluggy-1.6.0 -- /usr/bin/python3", "rootdir: /home/sweet/project" } );
    }
}

SCENARIO( "The pytest version template finds the version in the pytest header", "[ruletemplate]" )
{
    THEN( "the version after , pytest-, up to the comma or the end of the line" )
    {
        requireFinds( "pytest version",
                      { // pytest 9.0.3
                        { "platform linux -- Python 3.14.3, pytest-9.0.3, pluggy-1.6.0", "9.0.3" },
                        // pytest 3.2.3, with -v
                        { "platform win32 -- Python 3.4.3, pytest-3.2.3, py-1.4.34, pluggy-0.4.0 "
                          "-- c:\\program files (x86)\\python34\\python.exe",
                          "3.2.3" },
                        // a development build of pytest 9.2.0
                        { "platform linux -- Python 3.12.3, pytest-9.2.0.dev335+g99ab2accc, "
                          "pluggy-1.6.0",
                          "9.2.0.dev335+g99ab2accc" },
                        // pytest 5.2.2, the line as quoted, without plugins after it
                        { "platform darwin -- Python 3.7.3, pytest-5.2.2", "5.2.2" } } );
    }

    THEN( "not in the plugin list, a temporary path, or the cache directory" )
    {
        requireFindsNothing( "pytest version",
                             { "plugins: cov-4.1.0, xdist-3.5.0",
                               "tmp_path = PosixPath('/tmp/pytest-of-sweet/pytest-3/test0')",
                               "cachedir: .pytest_cache", "pytest-8.3.2" } );
    }
}

SCENARIO( "The test result template finds the summary at the end of a pytest run",
          "[ruletemplate]" )
{
    THEN( "the counts and the time, without the = around them" )
    {
        requireFinds(
            "Test result",
            { // pytest 9.0.3
              { "=== 2 failed, 1 passed in 0.04s ===", "2 failed, 1 passed in 0.04s" },
              // pytest 7.1.3
              { "============================== 2 passed in 0.01s "
                "===============================",
                "2 passed in 0.01s" },
              // pytest 7.4.4
              { "================================================ 1 failed, 1 warning in 0.02s "
                "================================================",
                "1 failed, 1 warning in 0.02s" },
              // pytest 7.2.0.dev12+g22756c28e, a run longer than a minute
              { "====== 6 failed, 3039 passed, 89 skipped, 9 xfailed in 336.42s (0:05:36) =======",
                "6 failed, 3039 passed, 89 skipped, 9 xfailed in 336.42s (0:05:36)" },
              // pytest 8.3.2
              { "===================== no tests ran in 0.00s ======================",
                "no tests ran in 0.00s" },
              // pytest 9.0.3
              { "=== 1 deselected in 0.00s ===", "1 deselected in 0.00s" },
              // pytest 7.4.0, with spaces after it as quoted
              { "=== 1 passed in 0.00s ===  ", "1 passed in 0.00s" },
              // pytest 3.6.1, which wrote seconds in full
              { "========================== 2 failed, 4 passed in 0.03 seconds "
                "===========================",
                "2 failed, 4 passed in 0.03 seconds" },
              // pytest -q, without the = (pytest 8.x with pytest-subtests)
              { "3 failed, 1 passed, 5 subtests passed in 0.03s",
                "3 failed, 1 passed, 5 subtests passed in 0.03s" },
              // pytest -q, pytest 4.x
              { "1 passed in 0.01 seconds", "1 passed in 0.01 seconds" } } );
    }

    THEN( "not the other separators, the interrupt line, or a unittest summary" )
    {
        requireFindsNothing(
            "Test result",
            { // pytest 8.3.2
              "============================= test session starts ==============================",
              "=================================== FAILURES ===================================",
              "=========================== short test summary info ============================",
              "FAILED test_sample.py::test_answer - assert 4 == 5",
              "!!!!!!!!!!!!!!!!!!!! Interrupted: 1 error during collection !!!!!!!!!!!!!!!!!!!!",
              "collected 120 items",
              "test_sample.py F                                                     [100%]",
              // unittest, Python 3.12
              "Ran 120 tests in 12.300s",
              // a log line that mentions a result in prose
              "INFO all 3 passed in 2s, deploying" } );
    }
}

SCENARIO( "The Test run templates read a whole pytest log", "[ruletemplate]" )
{
    GIVEN( "a failed pytest run, its temporary path naming pytest- after the header" )
    {
        // pytest 9.0.3, as quoted in pytest-dev/pytest, with a tmp_path line
        // added to the failure
        const QStringList log{
            "============================= test session starts ==============================",
            "platform linux -- Python 3.14.2, pytest-9.0.3, pluggy-1.6.0",
            "rootdir: /home/sweet/project",
            "collected 3 items",
            "",
            "test_sample.py FF.                                                   [100%]",
            "",
            "=================================== FAILURES ===================================",
            "tmp_path = PosixPath('/tmp/pytest-of-sweet/pytest-3/test_answer0')",
            "=========================== short test summary info ============================",
            "FAILED test_sample.py::test_answer - assert 4 == 5",
            "=== 2 failed, 1 passed in 0.04s ===",
        };

        THEN( "the footer shows the versions from the header and the summary" )
        {
            QList<FooterEntry> entries;
            for ( const auto* name : { "Python version", "pytest version", "Test result" } ) {
                entries.append( templateNamed( name ).entry() );
            }
            const auto values = scanLines( entries, log );
            REQUIRE( values.value( "Python" ) == "3.14.2" );
            REQUIRE( values.value( "pytest" ) == "9.0.3" );
            REQUIRE( values.value( "Test result" ) == "2 failed, 1 passed in 0.04s" );
        }
    }
}
