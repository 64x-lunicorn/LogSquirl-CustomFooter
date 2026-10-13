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
 * @file ruletemplate_ci_test.cpp
 * @brief The CI rule templates: what each one finds in build logs of GitHub
 * Actions, GitLab and Jenkins.
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

/// The value the template named @p templateName extracts from @p line
/// through the scanner, as the footer would show it; nothing if it does
/// not match.
std::optional<QString> extract( const QString& templateName, const QString& line )
{
    const auto entry = templateNamed( templateName ).entry();
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

// The start of the "Set up job" step of a GitHub Actions job log, as
// downloaded (GitHub-hosted runner 2.337.0, ubuntu-24.04 image, 2026-10).
const char* const setUpJob = "\xEF\xBB\xBF"
                             "2026-10-13T06:10:46.8342817Z Current runner version: '2.337.0'\n"
                             "2026-10-13T06:10:46.8362427Z ##[group]Runner Image Provisioner\n"
                             "2026-10-13T06:10:46.8363117Z Hosted Compute Agent\n"
                             "2026-10-13T06:10:46.8363472Z Version: 20261002.596\n"
                             "2026-10-13T06:10:46.8363841Z Commit: "
                             "c3d12f3313a95f25162a1503be624f3b0617f8c0\n"
                             "2026-10-13T06:10:46.8364512Z Build Date: 2026-10-02T22:28:27Z\n"
                             "2026-10-13T06:10:46.8366104Z ##[endgroup]\n"
                             "2026-10-13T06:10:46.8366943Z ##[group]Operating System\n"
                             "2026-10-13T06:10:46.8367308Z Ubuntu\n"
                             "2026-10-13T06:10:46.8367690Z 24.04.5\n"
                             "2026-10-13T06:10:46.8368017Z LTS\n"
                             "2026-10-13T06:10:46.8368330Z ##[endgroup]\n"
                             "2026-10-13T06:10:46.8368699Z ##[group]Runner Image\n"
                             "2026-10-13T06:10:46.8369098Z Image: ubuntu-24.04\n"
                             "2026-10-13T06:10:46.8369499Z Version: 20261004.327.1\n"
                             "2026-10-13T06:10:46.8371256Z Image Release: "
                             "https://github.com/actions/runner-images/releases/tag/"
                             "ubuntu24%2F20261004.327\n"
                             "2026-10-13T06:10:46.8371849Z ##[endgroup]";

} // namespace

SCENARIO( "The CI group holds its templates", "[ruletemplate]" )
{
    THEN( "runner image, its version, commit, GitLab Runner version and started by, in order" )
    {
        REQUIRE( namesIn( "CI" )
                 == QStringList{ "Runner image", "Runner image version", "Commit",
                                 "GitLab Runner version", "Started by" } );
    }

    THEN( "each is keyed by its name" )
    {
        for ( const auto& name : namesIn( "CI" ) ) {
            INFO( name.toStdString() );
            REQUIRE( templateNamed( name ).key == name );
        }
    }

    THEN( "all but the runner image version are simple rules" )
    {
        REQUIRE( templateNamed( "Runner image" ).simple
                 == SimpleRule{ "Image:", ValueEnd::Whitespace, {} } );
        REQUIRE( templateNamed( "Commit" ).simple
                 == SimpleRule{ "HEAD is now at", ValueEnd::Whitespace, {} } );
        REQUIRE( templateNamed( "GitLab Runner version" ).simple
                 == SimpleRule{ "Running with gitlab-runner", ValueEnd::Whitespace, {} } );
        REQUIRE( templateNamed( "Started by" ).simple
                 == SimpleRule{ "Started by user", ValueEnd::EndOfLine, {} } );
        for ( const auto* name :
              { "Runner image", "Commit", "GitLab Runner version", "Started by" } ) {
            INFO( name );
            const auto& t = templateNamed( name );
            REQUIRE( t.isSimple() );
            REQUIRE( simpleRuleOf( t.entry() ) == t.simple );
        }
    }

    THEN( "the runner image version needs a shape and is advanced" )
    {
        const auto& t = templateNamed( "Runner image version" );
        REQUIRE( !t.isSimple() );
        REQUIRE( !simpleRuleOf( t.entry() ).has_value() );
    }
}

SCENARIO( "The runner image template finds the GitHub Actions runner image", "[ruletemplate]" )
{
    THEN( "the image of the Runner Image section of Set up job" )
    {
        requireFinds( "Runner image", { { setUpJob, "ubuntu-24.04" } } );
    }

    THEN( "the image of other GitHub-hosted runners, with or without the timestamp" )
    {
        // Runner 2.337.0, macOS and Windows images, 2026-10.
        requireFinds(
            "Runner image",
            { { "2026-10-13T06:10:59.9181000Z Image: macos-15-arm64", "macos-15-arm64" },
              { "2026-10-13T06:10:56.3682869Z Image: windows-2025-vs2026", "windows-2025-vs2026" },
              { "Image: ubuntu-22.04", "ubuntu-22.04" } } );
    }

    THEN( "not the image release, another spelling, or no value" )
    {
        requireFindsNothing(
            "Runner image",
            { "2026-10-13T06:10:46.8371256Z Image Release: "
              "https://github.com/actions/runner-images/releases/tag/ubuntu24%2F20261004.327",
              "2026-10-13T06:10:46.8368699Z ##[group]Runner Image", "image: ubuntu-24.04",
              "Image:" } );
    }
}

SCENARIO( "The runner image version template finds the image's version", "[ruletemplate]" )
{
    THEN( "in Set up job, the image's version, not the provisioner's before it" )
    {
        requireFinds( "Runner image version", { { setUpJob, "20261004.327.1" } } );
    }

    THEN( "the version of other images, with or without the timestamp" )
    {
        // Runner 2.337.0, macOS and Windows images, 2026-10.
        requireFinds(
            "Runner image version",
            { { "2026-10-13T06:10:59.9181420Z Version: 20260907.0337.1", "20260907.0337.1" },
              { "2026-10-13T06:10:56.3683424Z Version: 20260925.250.1", "20260925.250.1" },
              { "Version: 20240101.1.0", "20240101.1.0" } } );
    }

    THEN( "not the provisioner's version, other versions, or another spelling" )
    {
        requireFindsNothing( "Runner image version",
                             { // Runner Image Provisioner, runner 2.337.0.
                               "2026-10-13T06:10:46.8363472Z Version: 20261002.596",
                               // Current runner version, runner 2.337.0.
                               "2026-10-13T06:10:46.8342817Z Current runner version: '2.337.0'",
                               // MSVC environment on windows-2025-vs2026, 2026-10.
                               "2026-10-13T06:12:25.8145829Z   UCRTVersion: 10.0.26100.0",
                               "Version: 8.0.100", "version: 20261004.327.1",
                               "Version: 20261004.327.1.5", "Version:" } );
    }
}

SCENARIO( "The commit template finds the commit after HEAD is now at", "[ruletemplate]" )
{
    THEN( "the abbreviated commit of a pull request's checkout" )
    {
        // actions/checkout (3d3c42e), git 2.55.0, ubuntu-24.04, 2026-10.
        requireFinds( "Commit", { { "2026-10-13T05:49:48.9902563Z HEAD is now at aa5a02d Merge "
                                    "d350627657e6a0217d1440d03d2458ff985c2437 into "
                                    "209be7109b130286819e8fe57ad0c2394ff93dd9",
                                    "aa5a02d" },
                                  { "HEAD is now at aa5a02d Merge", "aa5a02d" } } );
    }

    THEN( "of several checkouts, the first" )
    {
        requireFinds( "Commit", { { "HEAD is now at aa5a02d Merge into main\n"
                                    "Previous HEAD position was aa5a02d Merge into main\n"
                                    "HEAD is now at 1a90b04 Merge pull request #43",
                                    "aa5a02d" } } );
    }

    THEN( "not the previous HEAD, GitLab's checkout, another spelling, or no value" )
    {
        requireFindsNothing( "Commit", { // git 2.55.0
                                         "Previous HEAD position was aa5a02d Merge into main",
                                         // GitLab Runner 16.2.0
                                         "Checking out 7226fc70 as detached HEAD (ref is main)...",
                                         "head is now at aa5a02d", "HEAD is now at" } );
    }
}

SCENARIO( "The GitLab Runner version template finds the runner's version", "[ruletemplate]" )
{
    THEN( "the version on the first line of a job log" )
    {
        // GitLab Runner 16.2.0, from the GitLab 17.5 tutorial "Create,
        // register, and run your own project runner".
        requireFinds( "GitLab Runner version",
                      { { "Running with gitlab-runner 16.2.0 (782e15da)\n"
                          "  on my-project-runner TOKEN, system ID: SYSTEM ID\n"
                          "Preparing the \"shell\" executor",
                          "16.2.0" } } );
    }

    THEN( "not the old multi-runner, the runner's machine, or no value" )
    {
        requireFindsNothing( "GitLab Runner version",
                             { // GitLab CI Multi Runner 1.4.1
                               "Running with gitlab-ci-multi-runner 1.4.1 (fae8f18)",
                               // GitLab Runner 16.2.0
                               "Running on MACHINE-NAME...", "Running with GitLab Runner 16.2.0",
                               "Running with gitlab-runner" } );
    }
}

SCENARIO( "The started by template finds the Jenkins user who started the build", "[ruletemplate]" )
{
    THEN( "the user's name up to the end of the line, spaces included" )
    {
        // Jenkins 2.x core, Cause.UserIdCause: "Started by user {0}".
        requireFinds( "Started by", { { "Started by user admin\n"
                                        "Running as SYSTEM\n"
                                        "Building in workspace /var/jenkins_home/workspace/demo",
                                        "admin" },
                                      { "Started by user Jane Doe", "Jane Doe" },
                                      { "Started by user Jane Doe  ", "Jane Doe" } } );
    }

    THEN( "the user of the original cause of a build started upstream" )
    {
        // Jenkins 2.x core, Cause.UpstreamCause with its CausedBy.
        requireFinds( "Started by", { { "Started by upstream project \"build\" build number 12\n"
                                        "originally caused by:\n"
                                        " Started by user Jane Doe",
                                        "Jane Doe" } } );
    }

    THEN( "not for builds started otherwise, another spelling, or no value" )
    {
        requireFindsNothing( "Started by",
                             { // Jenkins 2.x core and its timer and SCM triggers.
                               "Started by timer", "Started by an SCM change",
                               "Started by upstream project \"build\" build number 12",
                               "Started by remote host 10.0.0.5", "started by user Jane Doe",
                               "Started by user", "Started by user   " } );
    }
}
