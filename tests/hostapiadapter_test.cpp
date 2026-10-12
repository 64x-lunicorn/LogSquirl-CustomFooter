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
 * @file hostapiadapter_test.cpp
 * @brief BDD tests for the adapter from the host's C function table to
 *        HostFunctions, with a fake table that records the calls and answers
 *        the Regex Lab when the test says so.
 */

#include <catch2/catch.hpp>

#include "hostapiadapter.h"
#include "hostfunctions.h"
#include "logsquirl_plugin_api.h"

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>

using namespace custom_footer;

namespace {

/// A Regex Lab the fake host opened, until the test answers it.
struct OpenLab {
    QByteArray pattern;
    int flags = 0;
    LogSquirlRegexLabCallbackFn callback = nullptr;
    void* userData = nullptr;
};

/// What the fake host was asked, and what it answers.
struct FakeTable {
    QStringList calls;
    QStringList errors; ///< The messages logged as errors.
    QList<OpenLab> labs;
    int openResult = 0;
    int goToResult = LOGSQUIRL_LOG_LINES_OK;
    QList<std::uint64_t> wentTo;
    int selectionResult = LOGSQUIRL_LOG_LINES_OK;
    QByteArray selection;
    size_t selectionLines = 0;
};

FakeTable& table()
{
    static FakeTable instance;
    return instance;
}

/// A whole 26.11 table: the base functions the adapter uses, and the three
/// added later.
LogSquirlHostApi fullApi()
{
    LogSquirlHostApi api{};
    api.api_version = LOGSQUIRL_PLUGIN_API_VERSION;
    api.log_message = []( void*, int level, const char* message ) {
        table().calls.append( "log_message" );
        if ( level == LOGSQUIRL_LOG_ERROR ) {
            table().errors.append( QString::fromUtf8( message ) );
        }
    };
    api.open_regex_lab = []( void*, const char* pattern, int flags,
                             LogSquirlRegexLabCallbackFn callback, void* userData ) {
        table().calls.append( "open_regex_lab" );
        if ( table().openResult == 0 ) {
            table().labs.append( { pattern, flags, callback, userData } );
        }
        return table().openResult;
    };
    api.go_to_log_line = []( void*, uint64_t lineNumber ) {
        table().calls.append( "go_to_log_line" );
        table().wentTo.append( lineNumber );
        return table().goToResult;
    };
    api.get_selected_log_lines = []( void*, const char** text, size_t* length, size_t* lineCount ) {
        table().calls.append( "get_selected_log_lines" );
        const bool ok = table().selectionResult >= 0;
        *text = ok ? table().selection.constData() : nullptr;
        if ( length ) {
            *length = ok ? static_cast<size_t>( table().selection.size() ) : 0;
        }
        if ( lineCount ) {
            *lineCount = ok ? table().selectionLines : 0;
        }
        return table().selectionResult;
    };
    return api;
}

/// Answer the fake host's Regex Lab @p index as the host does, with Apply.
void apply( int index, const char* pattern, int flags )
{
    const auto lab = table().labs.at( index );
    lab.callback( lab.userData, LOGSQUIRL_REGEX_LAB_APPLIED, pattern, flags );
}

/// Answer the fake host's Regex Lab @p index as the host does, with Cancel.
void cancel( int index )
{
    const auto lab = table().labs.at( index );
    lab.callback( lab.userData, LOGSQUIRL_REGEX_LAB_CANCELLED, nullptr, 0 );
}

/// Records the answers a Regex Lab's done function got.
struct Answers {
    QList<std::optional<RegexLabPattern>> got;

    RegexLabDone done()
    {
        return [ this ]( const std::optional<RegexLabPattern>& applied ) { got.append( applied ); };
    }
};

/// Sets a flag when destroyed, to see that a done function was released.
struct ReleaseFlag {
    bool* released;
    explicit ReleaseFlag( bool* flag )
        : released( flag )
    {
    }
    ~ReleaseFlag()
    {
        *released = true;
    }
};

} // namespace

SCENARIO( "The host API adapter offers what the table's size reports", "[hostapiadapter]" )
{
    table() = FakeTable();
    const auto api = fullApi();
    int handle = 0;

    WHEN( "the host passes a 26.11 table" )
    {
        HostApiAdapter adapter( &api, &handle, sizeof( LogSquirlHostApi ) );
        const auto& host = adapter.functions();

        THEN( "all three host functions are set, and logging too" )
        {
            REQUIRE( host.log );
            REQUIRE( host.openRegexLab );
            REQUIRE( host.goToLogLine );
            REQUIRE( host.selectedLogLines );
        }
    }

    WHEN( "the host passes a table that ends after Go to line" )
    {
        HostApiAdapter adapter( &api, &handle,
                                offsetof( LogSquirlHostApi, get_selected_log_lines ) );
        const auto& host = adapter.functions();

        THEN( "the selected Log Lines are not set" )
        {
            REQUIRE( host.openRegexLab );
            REQUIRE( host.goToLogLine );
            REQUIRE_FALSE( host.selectedLogLines );
        }
    }

    WHEN( "the host passes the base table, in a block of exactly its size" )
    {
        // Any read past the base table is a read past the block, which
        // AddressSanitizer reports.
        auto* block = static_cast<unsigned char*>( std::malloc( LOGSQUIRL_HOST_API_BASE_SIZE ) );
        REQUIRE( block );
        std::memcpy( block, &api, LOGSQUIRL_HOST_API_BASE_SIZE );
        const auto* base = reinterpret_cast<const LogSquirlHostApi*>( block );
        {
            HostApiAdapter adapter( base, &handle, LOGSQUIRL_HOST_API_BASE_SIZE );
            const auto& host = adapter.functions();

            THEN( "none of the three is set, and logging still works" )
            {
                REQUIRE_FALSE( host.openRegexLab );
                REQUIRE_FALSE( host.goToLogLine );
                REQUIRE_FALSE( host.selectedLogLines );
                host.logMessage( LOGSQUIRL_LOG_INFO, "hello" );
                REQUIRE( table().calls == QStringList{ "log_message" } );
            }
        }
        std::free( block );
    }
}

SCENARIO( "The adapter opens the host's Regex Lab and hands its answer over once",
          "[hostapiadapter]" )
{
    table() = FakeTable();
    const auto api = fullApi();
    int handle = 0;
    auto adapter = std::make_unique<HostApiAdapter>( &api, &handle, sizeof( LogSquirlHostApi ) );
    const auto host = adapter->functions();
    Answers answers;
    auto editor = std::make_unique<QObject>();

    GIVEN( "a Lab opened for a pattern that matches case" )
    {
        REQUIRE( host.openRegexLab( { "VIN:\\s+(\\S+)", true }, editor.get(), answers.done() ) );

        THEN( "the host got the pattern, the flag, and a callback" )
        {
            REQUIRE( table().labs.size() == 1 );
            REQUIRE( table().labs.at( 0 ).pattern == "VIN:\\s+(\\S+)" );
            REQUIRE( table().labs.at( 0 ).flags == LOGSQUIRL_REGEX_LAB_MATCH_CASE );
            REQUIRE( table().labs.at( 0 ).callback );
            REQUIRE( answers.got.isEmpty() );
        }

        WHEN( "the user applies a changed pattern that ignores case" )
        {
            apply( 0, "vin=(\\w+)", 0 );

            THEN( "done gets that pattern" )
            {
                REQUIRE( answers.got.size() == 1 );
                REQUIRE( answers.got.at( 0 ) );
                REQUIRE( answers.got.at( 0 )->pattern == "vin=(\\w+)" );
                REQUIRE_FALSE( answers.got.at( 0 )->matchesCase );
            }

            AND_WHEN( "the host answers once more, against its contract" )
            {
                cancel( 0 );

                THEN( "done is not called again" )
                {
                    REQUIRE( answers.got.size() == 1 );
                }
            }
        }

        WHEN( "the user applies a pattern that matches case" )
        {
            apply( 0, "VIN", LOGSQUIRL_REGEX_LAB_MATCH_CASE );

            THEN( "done says it matches case" )
            {
                REQUIRE( answers.got.size() == 1 );
                REQUIRE( answers.got.at( 0 )->matchesCase );
            }
        }

        WHEN( "the user cancels or closes the Lab" )
        {
            cancel( 0 );

            THEN( "done gets nothing" )
            {
                REQUIRE( answers.got.size() == 1 );
                REQUIRE_FALSE( answers.got.at( 0 ) );
            }
        }

        WHEN( "the editor that asked is gone before the answer comes" )
        {
            editor.reset();
            apply( 0, "VIN", 0 );

            THEN( "done is not called" )
            {
                REQUIRE( answers.got.isEmpty() );
            }
        }

        WHEN( "the plugin is shut down, and the host answers all the same" )
        {
            adapter.reset();
            apply( 0, "VIN", 0 );
            cancel( 0 );

            THEN( "nothing happens" )
            {
                REQUIRE( answers.got.isEmpty() );
            }
        }
    }

    GIVEN( "two Labs open at once" )
    {
        Answers second;
        REQUIRE( host.openRegexLab( { "first", false }, editor.get(), answers.done() ) );
        REQUIRE( host.openRegexLab( { "second", false }, editor.get(), second.done() ) );
        REQUIRE( table().labs.at( 0 ).flags == 0 );

        WHEN( "the second is applied and the first cancelled" )
        {
            apply( 1, "two", 0 );
            cancel( 0 );

            THEN( "each done gets its own answer" )
            {
                REQUIRE( answers.got.size() == 1 );
                REQUIRE_FALSE( answers.got.at( 0 ) );
                REQUIRE( second.got.size() == 1 );
                REQUIRE( second.got.at( 0 )->pattern == "two" );
            }
        }
    }

    GIVEN( "a done function that holds something" )
    {
        bool released = false;
        auto flag = std::make_shared<ReleaseFlag>( &released );
        REQUIRE( host.openRegexLab( { "VIN", true }, editor.get(),
                                    [ flag ]( const std::optional<RegexLabPattern>& ) {} ) );
        flag.reset();
        REQUIRE_FALSE( released );

        WHEN( "the answer comes" )
        {
            cancel( 0 );

            THEN( "done is released" )
            {
                REQUIRE( released );
            }
        }

        WHEN( "the editor is gone and then the answer comes" )
        {
            editor.reset();
            cancel( 0 );

            THEN( "done is released" )
            {
                REQUIRE( released );
            }
        }

        WHEN( "the plugin is shut down before an answer came" )
        {
            adapter.reset();

            THEN( "done is released" )
            {
                REQUIRE( released );
            }
        }
    }

    GIVEN( "a done function that throws" )
    {
        REQUIRE( host.openRegexLab( { "VIN", true }, editor.get(),
                                    []( const std::optional<RegexLabPattern>& ) {
                                        throw std::runtime_error( "no thanks" );
                                    } ) );

        WHEN( "the answer comes" )
        {
            cancel( 0 );

            THEN( "the exception is logged and does not reach the host" )
            {
                REQUIRE( table().errors
                         == QStringList{ "Custom Footer: the Regex Lab's answer failed: no "
                                         "thanks" } );
            }
        }
    }

    WHEN( "the host does not open the Lab" )
    {
        table().openResult = -1;
        bool released = false;
        auto flag = std::make_shared<ReleaseFlag>( &released );
        const bool opened = host.openRegexLab(
            { "VIN", true }, editor.get(), [ flag ]( const std::optional<RegexLabPattern>& ) {} );
        flag.reset();

        THEN( "it says so, and done is released at once" )
        {
            REQUIRE_FALSE( opened );
            REQUIRE( released );
        }
    }

    WHEN( "a Lab is asked for without a context" )
    {
        const bool opened = host.openRegexLab( { "VIN", true }, nullptr, answers.done() );

        THEN( "it is not opened, and the host is not asked" )
        {
            REQUIRE_FALSE( opened );
            REQUIRE( table().calls.isEmpty() );
        }
    }
}

SCENARIO( "The adapter goes to a Log Line of the host", "[hostapiadapter]" )
{
    table() = FakeTable();
    const auto api = fullApi();
    int handle = 0;
    HostApiAdapter adapter( &api, &handle, sizeof( LogSquirlHostApi ) );
    const auto& host = adapter.functions();

    WHEN( "the host shows the line" )
    {
        THEN( "it succeeds, with the line number passed as it is, from 1" )
        {
            REQUIRE( host.goToLogLine( 42 ) );
            REQUIRE( table().wentTo == QList<std::uint64_t>{ 42 } );
        }
    }

    WHEN( "the host has no such line, or no Log File" )
    {
        THEN( "it fails" )
        {
            table().goToResult = LOGSQUIRL_LOG_LINES_OUT_OF_RANGE;
            REQUIRE_FALSE( host.goToLogLine( 1000000 ) );
            table().goToResult = LOGSQUIRL_LOG_LINES_NO_LOG_FILE;
            REQUIRE_FALSE( host.goToLogLine( 1 ) );
        }
    }
}

SCENARIO( "The adapter reads the Log Lines selected in the host", "[hostapiadapter]" )
{
    table() = FakeTable();
    const auto api = fullApi();
    int handle = 0;
    HostApiAdapter adapter( &api, &handle, sizeof( LogSquirlHostApi ) );
    const auto& host = adapter.functions();

    WHEN( "two Log Lines are selected" )
    {
        table().selection = "VIN: W0L000051T2123456\nGear: P \xc3\xa4";
        table().selectionLines = 2;
        const auto selected = host.selectedLogLines();

        THEN( "both are copied, as UTF-8, and nothing is truncated" )
        {
            REQUIRE( selected );
            REQUIRE( selected->lines
                     == QStringList{ "VIN: W0L000051T2123456", QString::fromUtf8( "Gear: P ä" ) } );
            REQUIRE_FALSE( selected->truncated );
        }

        THEN( "the copy stays when the host's buffer changes" )
        {
            table().selection.fill( 'x' );
            REQUIRE( selected->lines.at( 0 ) == "VIN: W0L000051T2123456" );
        }
    }

    WHEN( "one empty Log Line is selected" )
    {
        table().selection = "";
        table().selectionLines = 1;
        const auto selected = host.selectedLogLines();

        THEN( "it is one empty line" )
        {
            REQUIRE( selected );
            REQUIRE( selected->lines == QStringList{ "" } );
        }
    }

    WHEN( "more is selected than the host hands over" )
    {
        table().selection = "first";
        table().selectionLines = 1;
        table().selectionResult = LOGSQUIRL_LOG_LINES_TRUNCATED;
        const auto selected = host.selectedLogLines();

        THEN( "the first lines come, marked as truncated" )
        {
            REQUIRE( selected );
            REQUIRE( selected->lines == QStringList{ "first" } );
            REQUIRE( selected->truncated );
        }
    }

    WHEN( "nothing is selected, or no Log File is open" )
    {
        THEN( "there is nothing" )
        {
            table().selectionResult = LOGSQUIRL_LOG_LINES_NO_SELECTION;
            REQUIRE_FALSE( host.selectedLogLines() );
            table().selectionResult = LOGSQUIRL_LOG_LINES_NO_LOG_FILE;
            REQUIRE_FALSE( host.selectedLogLines() );
            REQUIRE( table().calls
                     == QStringList{ "get_selected_log_lines", "get_selected_log_lines" } );
        }
    }
}
