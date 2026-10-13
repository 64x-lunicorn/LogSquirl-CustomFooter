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
 * @file ruletemplate_test.cpp
 * @brief Rule templates: what each one finds, and adding one in the editor.
 */

#include <catch2/catch.hpp>

#include "footereditor.h"
#include "footerscanner.h"
#include "rulelistmodel.h"
#include "ruletemplate.h"
#include "ruletemplatedialog.h"
#include "simplerule.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>

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

/// The value @p entry extracts from @p line through the scanner, as the
/// footer would show it; nothing if it does not match.
std::optional<QString> extract( const FooterEntry& entry, const QString& line )
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

std::optional<QString> extract( const QString& templateName, const QString& line )
{
    return extract( templateNamed( templateName ).entry(), line );
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

/// Test access to the editor and its template dialog, by object name.
struct TemplateUi {
    template <typename Widget>
    static Widget* find( QObject& parent, const char* name )
    {
        auto* widget = parent.findChild<Widget*>( name );
        REQUIRE( widget );
        return widget;
    }

    explicit TemplateUi( FooterEditor& editor )
        : editor( &editor )
        , list( find<QTableView>( editor, "ruleList" ) )
        , model( qobject_cast<RuleListModel*>( list->model() ) )
        , fromTemplate( find<QToolButton>( editor, "templateButton" ) )
        , key( find<QLineEdit>( editor, "keyEdit" ) )
        , advanced( find<QCheckBox>( editor, "advancedCheck" ) )
        , textBefore( find<QLineEdit>( editor, "textBeforeEdit" ) )
        , linePattern( find<QLineEdit>( editor, "linePatternEdit" ) )
        , mappings( find<QTableWidget>( editor, "mappingTable" ) )
    {
        REQUIRE( model );
    }

    /// Click "From template…" and return the dialog it opened.
    RuleTemplateDialog* open()
    {
        fromTemplate->click();
        dialog = editor->findChild<RuleTemplateDialog*>( "ruleTemplateDialog" );
        REQUIRE( dialog );
        REQUIRE( dialog->isVisible() );
        templates = find<QTreeWidget>( *dialog, "templateList" );
        templateKey = find<QLineEdit>( *dialog, "templateKeyEdit" );
        note = find<QLabel>( *dialog, "templateKeyNote" );
        keyHint = find<QLabel>( *dialog, "templateKeyHint" );
        buttons = dialog->findChild<QDialogButtonBox*>();
        REQUIRE( buttons );
        return dialog;
    }

    /// The item of the template named @p name, under its group.
    QTreeWidgetItem* templateItem( const QString& name ) const
    {
        const auto items = templates->findItems( name, Qt::MatchExactly | Qt::MatchRecursive, 0 );
        REQUIRE( items.size() == 1 );
        REQUIRE( items.first()->parent() );
        return items.first();
    }

    QTreeWidgetItem* groupItem( const QString& name ) const
    {
        const auto items = templates->findItems( name, Qt::MatchExactly, 0 );
        REQUIRE( items.size() == 1 );
        return items.first();
    }

    void check( const QString& name, bool checked = true ) const
    {
        templateItem( name )->setCheckState( 0, checked ? Qt::Checked : Qt::Unchecked );
    }

    void checkGroup( const QString& name ) const
    {
        groupItem( name )->setCheckState( 0, Qt::Checked );
    }

    /// The names of the checked templates, in list order.
    QStringList checkedTemplates() const
    {
        QStringList names;
        for ( int g = 0; g < templates->topLevelItemCount(); ++g ) {
            const auto* group = templates->topLevelItem( g );
            for ( int i = 0; i < group->childCount(); ++i ) {
                if ( group->child( i )->checkState( 0 ) == Qt::Checked ) {
                    names.append( group->child( i )->text( 0 ) );
                }
            }
        }
        return names;
    }

    QPushButton* ok() const
    {
        return buttons->button( QDialogButtonBox::Ok );
    }

    QPushButton* cancel() const
    {
        return buttons->button( QDialogButtonBox::Cancel );
    }

    /// Check a template and click OK.
    void add( const QString& name, const QString& askedKey = QString() )
    {
        open();
        check( name );
        if ( !askedKey.isEmpty() ) {
            templateKey->setText( askedKey );
        }
        REQUIRE( ok()->isEnabled() );
        ok()->click();
        REQUIRE( !dialog->isVisible() );
    }

    int currentRow() const
    {
        return list->currentIndex().isValid() ? list->currentIndex().row() : -1;
    }

    bool inSimpleMode() const
    {
        return !advanced->isChecked() && textBefore->isVisibleTo( editor )
               && linePattern->isReadOnly();
    }

    bool inAdvancedMode() const
    {
        return advanced->isChecked() && !textBefore->isVisibleTo( editor )
               && !linePattern->isReadOnly();
    }

    FooterEditor* editor;
    QTableView* list;
    RuleListModel* model;
    QToolButton* fromTemplate;
    QLineEdit* key;
    QCheckBox* advanced;
    QLineEdit* textBefore;
    QLineEdit* linePattern;
    QTableWidget* mappings;

    RuleTemplateDialog* dialog = nullptr;
    QTreeWidget* templates = nullptr;
    QLineEdit* templateKey = nullptr;
    QLabel* note = nullptr;
    QLabel* keyHint = nullptr;
    QDialogButtonBox* buttons = nullptr;
};

QList<FooterEntry> twoRules()
{
    return { { "VIN", "VIN:\\s*(\\S+)", "", true, { ValueMapping{ "WVW", "VW" } } },
             { "Mode", "mode=(\\w+)", "=(\\w+)", false, {} } };
}

bool sameEntry( const FooterEntry& a, const FooterEntry& b )
{
    if ( a.key != b.key || a.linePattern != b.linePattern || a.valuePattern != b.valuePattern
         || a.enabled != b.enabled || a.mappings.size() != b.mappings.size() ) {
        return false;
    }
    for ( int i = 0; i < a.mappings.size(); ++i ) {
        if ( a.mappings[ i ].pattern != b.mappings[ i ].pattern
             || a.mappings[ i ].displayValue != b.mappings[ i ].displayValue ) {
            return false;
        }
    }
    return true;
}

/// The templates of @p group, in list order.
QList<RuleTemplate> templatesIn( const QString& group )
{
    QList<RuleTemplate> inGroup;
    for ( const auto& t : ruleTemplates() ) {
        if ( t.group == group ) {
            inGroup.append( t );
        }
    }
    return inGroup;
}

} // namespace

SCENARIO( "Rule templates are named and described", "[ruletemplate]" )
{
    GIVEN( "the templates" )
    {
        const auto& templates = ruleTemplates();

        THEN( "each has a name and a one-line description" )
        {
            for ( const auto& t : templates ) {
                INFO( t.name.toStdString() );
                REQUIRE( !t.name.isEmpty() );
                REQUIRE( !t.description.isEmpty() );
                REQUIRE( !t.description.contains( '\n' ) );
            }
        }

        THEN( "General and Network hold their templates in this order" )
        {
            const auto names = []( const QString& group ) {
                QStringList inGroup;
                for ( const auto& t : templatesIn( group ) ) {
                    inGroup.append( t.name );
                }
                return inGroup;
            };
            REQUIRE( names( "General" )
                     == QStringList{ "Version", "Build number", "Serial number", "Environment",
                                     "User", "Log start", "key=value" } );
            REQUIRE( names( "Network" )
                     == QStringList{ "IPv4 address", "MAC address", "IPv6 address" } );
        }

        THEN( "each is in a group, the groups in order, a group's templates together" )
        {
            QStringList groups;
            for ( const auto& t : templates ) {
                INFO( t.name.toStdString() );
                REQUIRE( !t.group.isEmpty() );
                if ( groups.isEmpty() || groups.last() != t.group ) {
                    REQUIRE( !groups.contains( t.group ) );
                    groups.append( t.group );
                }
            }
            REQUIRE( groups
                     == QStringList{ "General", "Network", "Automotive", "Android",
                                     "Linux and boot", "iOS crash log", "Java and Spring Boot",
                                     "Database", "CI", "Test run" } );
        }

        THEN( "the IPv4 address is under Network, the others of the issue under General" )
        {
            for ( const auto* name : { "Version", "Build number", "Serial number", "Environment",
                                       "User", "Log start", "key=value" } ) {
                INFO( name );
                REQUIRE( templateNamed( name ).group == "General" );
            }
            REQUIRE( templateNamed( "IPv4 address" ).group == "Network" );
        }

        THEN( "a simple template whose text ends in a word does not match inside a longer word" )
        {
            // `Linux versions 1` is no kernel version `s` (#56).
            for ( const auto& t : templates ) {
                const auto text = t.simple.textBefore.trimmed();
                if ( !t.isSimple() || text.isEmpty() || !text.back().isLetterOrNumber() ) {
                    continue;
                }
                INFO( t.name.toStdString() );
                REQUIRE( extract( t.entry(), text + "s 1" ) == std::nullopt );
                REQUIRE( extract( t.entry(), text + " 1" ) == "1" );
            }
        }

        THEN( "every template makes an enabled rule with its mappings whose pattern compiles" )
        {
            for ( const auto& t : templates ) {
                INFO( t.name.toStdString() );
                const auto entry = t.entry( "k" );
                REQUIRE( !entry.key.isEmpty() );
                REQUIRE( !entry.linePattern.isEmpty() );
                REQUIRE( entry.valuePattern.isEmpty() );
                REQUIRE( entry.enabled );
                REQUIRE( entry.mappings.size() == t.mappings.size() );
                REQUIRE( FooterScanner::patternError( entry.linePattern ).isEmpty() );
            }
        }
    }
}

SCENARIO( "Rule templates are simple where the value fits the simple form", "[ruletemplate]" )
{
    GIVEN( "each template" )
    {
        THEN( "build number, serial number, IP address, environment and user are simple rules" )
        {
            for ( const auto* name :
                  { "Build number", "Serial number", "IPv4 address", "Environment", "User" } ) {
                INFO( name );
                const auto& t = templateNamed( name );
                REQUIRE( t.isSimple() );
                const auto entry = t.entry( "user" );
                REQUIRE( simpleRuleOf( entry ).has_value() );
                REQUIRE( entry.linePattern == simpleLinePattern( *simpleRuleOf( entry ) ) );
            }
        }

        THEN( "version, log start and key=value need a shape and are advanced" )
        {
            for ( const auto* name : { "Version", "Log start", "key=value" } ) {
                INFO( name );
                const auto& t = templateNamed( name );
                REQUIRE( !t.isSimple() );
                REQUIRE( !simpleRuleOf( t.entry( "user" ) ).has_value() );
            }
        }
    }
}

SCENARIO( "The version template finds version numbers", "[ruletemplate]" )
{
    THEN( "after version or ver, in any case, with an optional : or = and v" )
    {
        requireFinds( "Version", { { "version 1.2.3", "1.2.3" },
                                   { "Version: v2.0.1-rc1", "2.0.1-rc1" },
                                   { "ver=1.2", "1.2" },
                                   { "App VERSION 4.10 started", "4.10" },
                                   { "firmware version: 3", "3" },
                                   { "version 1.2.3+build.5 ready", "1.2.3+build.5" },
                                   { "(version 1.0.0).", "1.0.0" } } );
    }

    THEN( "a v directly before a dotted number, e.g. v1.2.3" )
    {
        requireFinds( "Version", { { "starting v1.2.3", "1.2.3" },
                                   { "tool-v10.4 loaded", "10.4" },
                                   { "V2.0", "2.0" } } );
    }

    THEN( "after _ or at the start of a camel-case word, e.g. appVersion" )
    {
        requireFinds( "Version", { { "app_version=1.2.3", "1.2.3" },
                                   { "appVersion: 2.1.0", "2.1.0" },
                                   { "fw_ver=3.1", "3.1" } } );
    }

    THEN( "with quotes, > or . between the word and the number, without the quotes" )
    {
        requireFinds( "Version", { { "{\"version\": \"1.2.3\"}", "1.2.3" },
                                   { "version=\"1.2.3\"", "1.2.3" },
                                   { "<version>1.2.3</version>", "1.2.3" },
                                   { "version '1.2.3'", "1.2.3" },
                                   { "Ver. 2.1", "2.1" } } );
    }

    THEN( "a pre-release or build suffix with a letter is kept whole" )
    {
        requireFinds( "Version", { { "version 1.2.3-rc1", "1.2.3-rc1" },
                                   { "version 1.2.3+build.5", "1.2.3+build.5" },
                                   { "version 2.0.0-beta.2", "2.0.0-beta.2" },
                                   { "version 1.0-alpha", "1.0-alpha" } } );
    }

    THEN( "not the XML declaration's version, but an app version on a later line" )
    {
        requireFindsNothing( "Version", { "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" } );
        requireFinds( "Version",
                      { { "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<app>\n<version>1.2.3</version>",
                          "1.2.3" },
                        { "<?xml version=\"1.0\"?>\n{\"version\": \"4.5.6\"}", "4.5.6" } } );
    }

    THEN( "a date after the version is not part of it" )
    {
        requireFinds( "Version", { { "version 1.2.3-2024-01-15T10:30:00", "1.2.3" } } );
    }

    THEN( "not in other words, without a number, a bare number, or a date" )
    {
        requireFindsNothing( "Version", { "conversion 1.2", "server=1.2", "server 1.2.3",
                                          "vertical 3.4", "version unknown", "versions 1.2",
                                          "v1 of the API", "rev1.2.3", "address 10.0.0.1", "1.2.3",
                                          "SERVER 1.2", "version: 2024-01-15 10:30" } );
    }
}

SCENARIO( "The build number template finds the value after Build:", "[ruletemplate]" )
{
    THEN( "the word after Build:, with or without a space" )
    {
        requireFinds( "Build number", { { "Build: 1234", "1234" },
                                        { "App Build:20240115.3 (release)", "20240115.3" },
                                        { "Build:   #512 done", "#512" } } );
    }

    THEN( "not without the colon, in lower case, or without a value" )
    {
        requireFindsNothing( "Build number",
                             { "Build 1234", "build: 1234", "Build:", "Build:   ", "rebuilding" } );
    }
}

SCENARIO( "The serial number template finds the value after Serial number:", "[ruletemplate]" )
{
    THEN( "the word after Serial number:" )
    {
        requireFinds( "Serial number", { { "Serial number: SN-00042", "SN-00042" },
                                         { "Device Serial number:ABC123 ok", "ABC123" } } );
    }

    THEN( "not for other spellings or without a value" )
    {
        requireFindsNothing( "Serial number",
                             { "serial number: 1", "Serial: 1", "Serial number:", "S/N: 42" } );
    }
}

SCENARIO( "The IP address template finds the address after IP:", "[ruletemplate]" )
{
    GIVEN( "the template" )
    {
        const auto& t = templateNamed( "IPv4 address" );

        THEN( "its rule is the simple rule for the word after IP:, keyed IP address" )
        {
            REQUIRE( t.key == "IP address" );
            REQUIRE( t.simple == SimpleRule{ "IP:", ValueEnd::Whitespace, {} } );
        }
    }

    THEN( "the word after IP:, with or without a space" )
    {
        requireFinds( "IPv4 address", { { "IP: 192.168.1.20", "192.168.1.20" },
                                        { "Device IP:10.0.0.5 up", "10.0.0.5" },
                                        { "eth0 IP:   172.16.0.2", "172.16.0.2" } } );
    }

    THEN( "the address after the text, not the first address in the line" )
    {
        requireFinds( "IPv4 address",
                      { { "request from 10.0.0.7 to server IP: 192.168.1.20", "192.168.1.20" },
                        { "GET / 10.0.0.1 200\nIP: 192.168.1.20", "192.168.1.20" } } );
    }

    THEN( "not an address without the text, another spelling, or no value" )
    {
        requireFindsNothing( "IPv4 address",
                             { "connected to 192.168.1.20:8080", "10.0.0.1", "ip: 10.0.0.1",
                               "IP 10.0.0.1", "IP address 10.0.0.1", "IP:", "IP:   " } );
    }
}

SCENARIO( "The environment template finds the value after env:", "[ruletemplate]" )
{
    THEN( "the word after env:" )
    {
        requireFinds( "Environment", { { "env: prod", "prod" },
                                       { "Starting service env:staging region=eu", "staging" },
                                       { "deploy env:   dev-2", "dev-2" } } );
    }

    THEN( "not without the colon, another spelling, or no value" )
    {
        requireFindsNothing( "Environment", { "env prod", "ENV: prod", "Env: prod",
                                              "environment: prod", "env=prod", "env:" } );
    }
}

SCENARIO( "The user template finds the value after user:", "[ruletemplate]" )
{
    THEN( "the word after user:" )
    {
        requireFinds( "User", { { "user: alice", "alice" },
                                { "Session opened user:bob@example.com", "bob@example.com" },
                                { "login user:   svc_backup ok", "svc_backup" } } );
    }

    THEN( "not without the colon, another spelling, or no value" )
    {
        requireFindsNothing(
            "User", { "user alice", "User: alice", "user=alice", "username alice", "user:" } );
    }
}

SCENARIO( "The log start template finds the first ISO 8601 date and time", "[ruletemplate]" )
{
    GIVEN( "the template" )
    {
        const auto& t = templateNamed( "Log start" );

        THEN( "it is keyed Log start and says it shows when the log starts" )
        {
            REQUIRE( t.key == "Log start" );
            REQUIRE( t.description.contains( "log starts" ) );
        }

        THEN( "its pattern is the one of the former timestamp template" )
        {
            REQUIRE( t.entry().linePattern
                     == "(?<!\\d)(\\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\\d|3[01])"
                        "[Tt ](?:[01]\\d|2[0-3]):[0-5]\\d"
                        "(?>(?::(?:[0-5]\\d|60)(?:[.,]\\d+)?)?"
                        "(?:[Zz]|[+-](?:[01]\\d|2[0-3])(?::?[0-5]\\d)?)?))(?!\\d|:\\d)" );
        }
    }

    THEN( "of several lines with a timestamp, the first, where the log starts" )
    {
        requireFinds( "Log start", { { "2024-01-15T10:30:00Z start\n"
                                       "2024-01-15T10:31:12Z running\n"
                                       "2024-01-15T11:02:45Z stop",
                                       "2024-01-15T10:30:00Z" } } );
    }

    THEN( "date, T or a space, time, optional seconds, fraction and offset" )
    {
        requireFinds( "Log start",
                      { { "2024-01-15T10:30:00Z start", "2024-01-15T10:30:00Z" },
                        { "[2024-01-15 10:30:00.123] INFO", "2024-01-15 10:30:00.123" },
                        { "at 2024-12-31T23:59:59,5+01:00.", "2024-12-31T23:59:59,5+01:00" },
                        { "ts=2024-02-29T08:15-0500 x", "2024-02-29T08:15-0500" },
                        { "2024-06-01T00:00:00", "2024-06-01T00:00:00" } } );
    }

    THEN( "an offset of hours only, and lower-case t and z" )
    {
        requireFinds( "Log start", { { "2024-01-15T10:30:00+01 up", "2024-01-15T10:30:00+01" },
                                     { "2024-01-15 10:30:00-05", "2024-01-15 10:30:00-05" },
                                     { "2024-01-15t10:30:00z", "2024-01-15t10:30:00z" } } );
    }

    THEN( "a complete timestamp followed by a colon" )
    {
        requireFinds( "Log start",
                      { { "2024-01-15 10:30:00: Server started", "2024-01-15 10:30:00" },
                        { "2024-01-15T10:30:00Z: started", "2024-01-15T10:30:00Z" },
                        { "2024-01-15T10:30:00.123: x", "2024-01-15T10:30:00.123" },
                        { "2024-01-15T10:30:00+01:00: x", "2024-01-15T10:30:00+01:00" } } );
    }

    THEN( "not a date alone, another format, or values out of range" )
    {
        requireFindsNothing( "Log start",
                             { "2024-01-15", "2024/01/15 10:30:00", "15.01.2024 10:30",
                               "2024-13-01T10:00:00", "2024-01-32T10:00", "2024-01-15T24:00",
                               "2024-01-15T10:60", "12024-01-15T10:30:00", "10:30:00",
                               "2024-01-15T10:30:5", "2024-01-15T10:30:00.5:1",
                               "2024-01-15T10:30:00+01:0", "2024-01-15T10:305" } );
    }
}

SCENARIO( "The key=value template finds the value after the given key", "[ruletemplate]" )
{
    GIVEN( "the key user" )
    {
        const auto entry = templateNamed( "key=value" ).entry( "  user " );

        THEN( "the rule is named after the trimmed key and looks for user= as a whole key" )
        {
            REQUIRE( entry.key == "user" );
            REQUIRE( entry.linePattern == "(?<![\\w.-])-{0,2}user=(\\S+)" );
        }

        THEN( "the value directly after = ends at whitespace" )
        {
            for ( const Sample& sample :
                  std::initializer_list<Sample>{ { "user=alice", "alice" },
                                                 { "level=info user=alice msg=hi", "alice" },
                                                 { "a=1 user=x;y=2", "x;y=2" },
                                                 { "superuser=root user=alice", "alice" },
                                                 { "user= msg=hi user=bob", "bob" },
                                                 { "Starting with --user=alice", "alice" },
                                                 { "--user=alice", "alice" },
                                                 { "-user=alice", "alice" },
                                                 { "a=1 user=alice", "alice" } } ) {
                INFO( sample.line );
                REQUIRE( extract( entry, sample.line ) == QString( sample.value ) );
            }
        }

        THEN( "not for other keys, another separator, or without a value" )
        {
            for ( const auto* line :
                  { "users: alice", "user: alice", "User=alice", "user=", "user= msg=hi",
                    "superuser=root", "a.user=x", "my-user=x", "my-user=bob", "x.user=bob",
                    "my--user=bob", "---user=bob" } ) {
                INFO( line );
                REQUIRE( extract( entry, line ) == std::nullopt );
            }
        }
    }

    GIVEN( "a key with regex metacharacters" )
    {
        const auto entry = templateNamed( "key=value" ).entry( "a.b[0]" );

        THEN( "it is matched literally" )
        {
            REQUIRE( extract( entry, "a.b[0]=7" ) == QString( "7" ) );
            REQUIRE( extract( entry, "axb0=7" ) == std::nullopt );
        }
    }

    GIVEN( "a key given with its =" )
    {
        const auto entry = templateNamed( "key=value" ).entry( "user=" );

        THEN( "one = is dropped: the rule looks for user=, not user==" )
        {
            REQUIRE( entry.key == "user" );
            REQUIRE( extract( entry, "user=alice" ) == QString( "alice" ) );
        }
    }

    GIVEN( "no key" )
    {
        THEN( "the template makes no rule" )
        {
            REQUIRE( templateNamed( "key=value" ).asksForKey() );
            REQUIRE( templateNamed( "key=value" ).entry( "  " ).linePattern.isEmpty() );
            REQUIRE( templateNamed( "key=value" ).entry( " = " ).linePattern.isEmpty() );
        }
    }
}

SCENARIO( "From template… adds a rule and selects it", "[ruletemplate][footereditor]" )
{
    GIVEN( "an editor with two rules, the second selected" )
    {
        const auto rules = twoRules();
        FooterEditor editor( rules );
        TemplateUi ui( editor );
        editor.show();
        ui.list->setCurrentIndex( ui.model->index( 1, RuleListModel::KeyColumn ) );

        WHEN( "opening the template dialog" )
        {
            ui.open();

            THEN( "it lists every template with its name and description under its group" )
            {
                const auto& templates = ruleTemplates();
                int i = 0;
                for ( int g = 0; g < ui.templates->topLevelItemCount(); ++g ) {
                    const auto* group = ui.templates->topLevelItem( g );
                    REQUIRE( group->childCount() > 0 );
                    for ( int c = 0; c < group->childCount(); ++c, ++i ) {
                        REQUIRE( i < templates.size() );
                        REQUIRE( group->text( 0 ) == templates[ i ].group );
                        REQUIRE( group->child( c )->text( 0 ) == templates[ i ].name );
                        REQUIRE( group->child( c )->text( 1 ) == templates[ i ].description );
                    }
                }
                REQUIRE( i == templates.size() );
                REQUIRE( ui.templates->topLevelItem( 0 )->text( 0 ) == "General" );
                REQUIRE( ui.templates->topLevelItem( 1 )->text( 0 ) == "Network" );
            }

            THEN( "nothing is checked, so OK is disabled and no key is asked for" )
            {
                REQUIRE( ui.checkedTemplates().isEmpty() );
                REQUIRE( ui.groupItem( "General" )->checkState( 0 ) == Qt::Unchecked );
                REQUIRE( !ui.ok()->isEnabled() );
                REQUIRE( !ui.templateKey->isEnabled() );
                REQUIRE( ui.note->isHidden() );
            }
        }

        WHEN( "adding the version template" )
        {
            ui.add( "Version" );

            THEN( "a new rule is appended and selected, in advanced mode" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 3 );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( sameEntry( entries[ 2 ], templateNamed( "Version" ).entry() ) );
                REQUIRE( ui.key->text() == "Version" );
                REQUIRE( ui.linePattern->text() == entries[ 2 ].linePattern );
                REQUIRE( ui.inAdvancedMode() );
            }

            THEN( "the existing rules are unchanged" )
            {
                const auto entries = editor.entries();
                REQUIRE( sameEntry( entries[ 0 ], rules[ 0 ] ) );
                REQUIRE( sameEntry( entries[ 1 ], rules[ 1 ] ) );
            }
        }

        WHEN( "adding the environment template" )
        {
            ui.add( "Environment" );

            THEN( "the new rule is selected in simple mode with the text env:" )
            {
                REQUIRE( editor.entries()[ 2 ].key == "Environment" );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( ui.textBefore->text() == "env:" );
            }
        }

        WHEN( "adding the user template" )
        {
            ui.add( "User" );

            THEN( "the new rule is selected in simple mode with the text user:" )
            {
                REQUIRE( editor.entries()[ 2 ].key == "User" );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( ui.textBefore->text() == "user:" );
            }
        }

        WHEN( "adding the build number template" )
        {
            ui.add( "Build number" );

            THEN( "the new rule is selected in simple mode" )
            {
                REQUIRE( editor.entries().size() == 3 );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( ui.textBefore->text() == "Build:" );
            }
        }

        WHEN( "adding every template in turn" )
        {
            for ( const auto& t : ruleTemplates() ) {
                ui.add( t.name, t.asksForKey() ? QStringLiteral( "user" ) : QString() );
            }

            THEN( "each is a new rule after the others, and the old ones are unchanged" )
            {
                const auto entries = editor.entries();
                const auto& templates = ruleTemplates();
                REQUIRE( entries.size() == 2 + templates.size() );
                REQUIRE( sameEntry( entries[ 0 ], rules[ 0 ] ) );
                REQUIRE( sameEntry( entries[ 1 ], rules[ 1 ] ) );
                for ( int i = 0; i < templates.size(); ++i ) {
                    REQUIRE( sameEntry( entries[ 2 + i ], templates[ i ].entry( "user" ) ) );
                }
                REQUIRE( ui.currentRow() == entries.size() - 1 );
            }
        }

        WHEN( "cancelling the dialog" )
        {
            ui.open();
            ui.check( "IPv4 address" );
            ui.cancel()->click();

            THEN( "nothing is added and the selection stays" )
            {
                REQUIRE( !ui.dialog->isVisible() );
                REQUIRE( editor.entries().size() == 2 );
                REQUIRE( ui.currentRow() == 1 );
            }
        }
    }

    GIVEN( "an editor without rules" )
    {
        FooterEditor editor( {} );
        TemplateUi ui( editor );
        editor.show();

        WHEN( "adding a template" )
        {
            ui.add( "IPv4 address" );

            THEN( "it is the only rule, selected in simple mode with the text IP:" )
            {
                REQUIRE( editor.entries().size() == 1 );
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.key->text() == "IP address" );
                REQUIRE( ui.inSimpleMode() );
                REQUIRE( ui.textBefore->text() == "IP:" );
            }
        }
    }
}

SCENARIO( "From template… adds every checked template", "[ruletemplate][footereditor]" )
{
    GIVEN( "the template dialog of an editor with two rules, the second selected" )
    {
        const auto rules = twoRules();
        FooterEditor editor( rules );
        TemplateUi ui( editor );
        editor.show();
        ui.list->setCurrentIndex( ui.model->index( 1, RuleListModel::KeyColumn ) );
        ui.open();

        WHEN( "checking log start, then the version, and clicking OK" )
        {
            ui.check( "Log start" );
            ui.check( "Version" );
            REQUIRE( ui.ok()->isEnabled() );
            ui.ok()->click();

            THEN( "both are added after the existing rules in list order, the first selected" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 4 );
                REQUIRE( sameEntry( entries[ 2 ], templateNamed( "Version" ).entry() ) );
                REQUIRE( sameEntry( entries[ 3 ], templateNamed( "Log start" ).entry() ) );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( ui.key->text() == "Version" );
            }

            THEN( "the existing rules are unchanged" )
            {
                const auto entries = editor.entries();
                REQUIRE( sameEntry( entries[ 0 ], rules[ 0 ] ) );
                REQUIRE( sameEntry( entries[ 1 ], rules[ 1 ] ) );
            }
        }

        WHEN( "checking the Network group" )
        {
            ui.checkGroup( "Network" );

            THEN( "its templates are checked, and OK adds them" )
            {
                QStringList names;
                for ( const auto& t : templatesIn( "Network" ) ) {
                    names.append( t.name );
                }
                REQUIRE( ui.checkedTemplates() == names );
                REQUIRE( ui.ok()->isEnabled() );
                ui.ok()->click();
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 2 + names.size() );
                REQUIRE( sameEntry( entries[ 2 ], templateNamed( "IPv4 address" ).entry() ) );
                REQUIRE( ui.currentRow() == 2 );
            }
        }

        WHEN( "checking the General group" )
        {
            ui.checkGroup( "General" );

            THEN( "all its templates are checked, and OK waits for the key of key=value" )
            {
                REQUIRE( ui.checkedTemplates().size() == templatesIn( "General" ).size() );
                REQUIRE( ui.templateKey->isEnabled() );
                REQUIRE( !ui.ok()->isEnabled() );
            }

            AND_WHEN( "giving the key user and clicking OK" )
            {
                ui.templateKey->setText( "user" );
                REQUIRE( ui.ok()->isEnabled() );
                ui.ok()->click();

                THEN( "there is one new rule per template in list order, the first selected" )
                {
                    const auto entries = editor.entries();
                    const auto general = templatesIn( "General" );
                    REQUIRE( entries.size() == 2 + general.size() );
                    REQUIRE( sameEntry( entries[ 0 ], rules[ 0 ] ) );
                    REQUIRE( sameEntry( entries[ 1 ], rules[ 1 ] ) );
                    for ( int i = 0; i < general.size(); ++i ) {
                        REQUIRE( sameEntry( entries[ 2 + i ], general[ i ].entry( "user" ) ) );
                    }
                    REQUIRE( ui.currentRow() == 2 );
                }
            }

            AND_WHEN( "unchecking key=value" )
            {
                ui.check( "key=value", false );

                THEN( "the group is partly checked, no key is asked, and OK adds the others" )
                {
                    REQUIRE( ui.groupItem( "General" )->checkState( 0 ) == Qt::PartiallyChecked );
                    REQUIRE( !ui.templateKey->isEnabled() );
                    REQUIRE( ui.ok()->isEnabled() );
                    ui.ok()->click();
                    REQUIRE( editor.entries().size() == 2 + templatesIn( "General" ).size() - 1 );
                }
            }
        }

        WHEN( "checking a template and unchecking it again" )
        {
            ui.check( "Version" );
            ui.check( "Version", false );

            THEN( "OK is disabled again" )
            {
                REQUIRE( !ui.ok()->isEnabled() );
            }
        }

        WHEN( "double-clicking a template" )
        {
            Q_EMIT ui.templates->itemDoubleClicked( ui.templateItem( "Version" ), 0 );

            THEN( "it is checked, and double-clicking it again unchecks it" )
            {
                REQUIRE( ui.checkedTemplates() == QStringList{ "Version" } );
                REQUIRE( ui.dialog->isVisible() );
                Q_EMIT ui.templates->itemDoubleClicked( ui.templateItem( "Version" ), 0 );
                REQUIRE( ui.checkedTemplates().isEmpty() );
            }
        }

        WHEN( "checking templates, cancelling and opening the dialog again" )
        {
            ui.check( "Version" );
            ui.checkGroup( "Network" );
            ui.cancel()->click();
            ui.open();

            THEN( "nothing is checked" )
            {
                REQUIRE( ui.checkedTemplates().isEmpty() );
                REQUIRE( ui.groupItem( "Network" )->checkState( 0 ) == Qt::Unchecked );
                REQUIRE( !ui.ok()->isEnabled() );
            }
        }
    }
}

SCENARIO( "The key=value template asks for the key", "[ruletemplate][footereditor]" )
{
    GIVEN( "the template dialog with key=value checked" )
    {
        FooterEditor editor( twoRules() );
        TemplateUi ui( editor );
        editor.show();
        ui.open();
        ui.check( "key=value" );

        THEN( "the key field is enabled, and OK waits for a key" )
        {
            REQUIRE( ui.templateKey->isEnabled() );
            REQUIRE( !ui.ok()->isEnabled() );
            ui.templateKey->setText( "   " );
            REQUIRE( !ui.ok()->isEnabled() );
            ui.templateKey->setText( "user" );
            REQUIRE( ui.ok()->isEnabled() );
        }

        WHEN( "giving a key and clicking OK" )
        {
            ui.templateKey->setText( "user" );
            ui.ok()->click();

            THEN( "the new rule for user= is named user, selected in advanced mode" )
            {
                const auto entries = editor.entries();
                REQUIRE( entries.size() == 3 );
                REQUIRE( entries[ 2 ].key == "user" );
                REQUIRE( ui.currentRow() == 2 );
                REQUIRE( ui.inAdvancedMode() );
                REQUIRE( ui.linePattern->text() == entries[ 2 ].linePattern );
            }
        }

        WHEN( "giving the key with its =" )
        {
            ui.templateKey->setText( "user=" );

            THEN( "a hint says the = is added anyway, and OK is enabled" )
            {
                REQUIRE( !ui.keyHint->isHidden() );
                REQUIRE( ui.keyHint->text().contains( "user=" ) );
                REQUIRE( ui.ok()->isEnabled() );
            }

            AND_WHEN( "clicking OK" )
            {
                ui.ok()->click();

                THEN( "the rule is for user=, named user" )
                {
                    REQUIRE( editor.entries()[ 2 ].key == "user" );
                    REQUIRE( sameEntry( editor.entries()[ 2 ],
                                        templateNamed( "key=value" ).entry( "user" ) ) );
                }
            }

            AND_WHEN( "removing the =" )
            {
                ui.templateKey->setText( "user" );

                THEN( "the hint goes away" )
                {
                    REQUIRE( ui.keyHint->isHidden() );
                }
            }
        }

        THEN( "the placeholder does not suggest typing the =" )
        {
            REQUIRE( !ui.templateKey->placeholderText().contains( "=" ) );
            REQUIRE( ui.keyHint->isHidden() );
        }

        WHEN( "cancelling" )
        {
            ui.templateKey->setText( "user" );
            ui.cancel()->click();

            THEN( "no rule is added" )
            {
                REQUIRE( editor.entries().size() == 2 );
            }

            AND_WHEN( "opening the dialog again" )
            {
                ui.open();

                THEN( "it starts over, without the key given before" )
                {
                    REQUIRE( ui.templateKey->text().isEmpty() );
                    REQUIRE( ui.checkedTemplates().isEmpty() );
                }
            }
        }

        WHEN( "checking a template that needs no key too" )
        {
            ui.check( "Version" );

            THEN( "the key is still asked for, and OK still waits for it" )
            {
                REQUIRE( ui.templateKey->isEnabled() );
                REQUIRE( !ui.ok()->isEnabled() );
            }

            AND_WHEN( "unchecking key=value" )
            {
                ui.check( "key=value", false );

                THEN( "the key field is disabled again, and OK enabled" )
                {
                    REQUIRE( !ui.templateKey->isEnabled() );
                    REQUIRE( ui.ok()->isEnabled() );
                }
            }
        }
    }
}

SCENARIO( "A template whose key is used already says it adds an alternative",
          "[ruletemplate][footereditor]" )
{
    GIVEN( "an editor with a rule for the key Version" )
    {
        const QList<FooterEntry> rules{ { "Version", "Version=(\\S+)", "", true, {} } };
        FooterEditor editor( rules );
        TemplateUi ui( editor );
        editor.show();
        ui.open();

        WHEN( "checking the version template" )
        {
            ui.check( "Version" );

            THEN( "a note says the new rule becomes an alternative for Version" )
            {
                REQUIRE( !ui.note->isHidden() );
                REQUIRE( ui.note->text().contains( "Version" ) );
                REQUIRE( ui.note->text().contains( "alternative" ) );
                REQUIRE( ui.ok()->isEnabled() );
            }

            AND_WHEN( "adding it" )
            {
                ui.ok()->click();

                THEN( "it is added after the existing rule, which stays unchanged" )
                {
                    const auto entries = editor.entries();
                    REQUIRE( entries.size() == 2 );
                    REQUIRE( sameEntry( entries[ 0 ], rules[ 0 ] ) );
                    REQUIRE( entries[ 1 ].key == "Version" );
                    REQUIRE( ui.currentRow() == 1 );
                }
            }
        }

        WHEN( "checking a template with another key" )
        {
            ui.check( "Log start" );

            THEN( "there is no note" )
            {
                REQUIRE( ui.note->isHidden() );
            }
        }

        WHEN( "giving key=value the key Version" )
        {
            ui.check( "key=value" );
            ui.templateKey->setText( "Version" );

            THEN( "the note shows up for the given key" )
            {
                REQUIRE( !ui.note->isHidden() );
            }

            AND_WHEN( "changing the key" )
            {
                ui.templateKey->setText( "Versions" );

                THEN( "the note goes away" )
                {
                    REQUIRE( ui.note->isHidden() );
                }
            }
        }
    }
}

SCENARIO( "The alternative note names every checked template whose key is used",
          "[ruletemplate][footereditor]" )
{
    GIVEN( "an editor with rules for the keys Version and IP address" )
    {
        const QList<FooterEntry> rules{ { "Version", "Version=(\\S+)", "", true, {} },
                                        { "IP address", "ip=(\\S+)", "", true, {} } };
        FooterEditor editor( rules );
        TemplateUi ui( editor );
        editor.show();
        ui.open();

        WHEN( "checking version, build number and the IPv4 address" )
        {
            ui.check( "Version" );
            ui.check( "Build number" );
            ui.check( "IPv4 address" );

            THEN( "the note names Version and IP address, but not Build" )
            {
                REQUIRE( !ui.note->isHidden() );
                REQUIRE( ui.note->text().contains( "“Version”" ) );
                REQUIRE( ui.note->text().contains( "“IP address”" ) );
                REQUIRE( !ui.note->text().contains( "Build" ) );
                REQUIRE( ui.note->text().contains( "alternative" ) );
                REQUIRE( ui.ok()->isEnabled() );
            }

            AND_WHEN( "unchecking version" )
            {
                ui.check( "Version", false );

                THEN( "the note names only IP address" )
                {
                    REQUIRE( !ui.note->isHidden() );
                    REQUIRE( ui.note->text().contains( "“IP address”" ) );
                    REQUIRE( !ui.note->text().contains( "Version" ) );
                }
            }

            AND_WHEN( "adding them" )
            {
                ui.ok()->click();

                THEN( "they are added after the existing rules, which stay unchanged" )
                {
                    const auto entries = editor.entries();
                    REQUIRE( entries.size() == 5 );
                    REQUIRE( sameEntry( entries[ 0 ], rules[ 0 ] ) );
                    REQUIRE( sameEntry( entries[ 1 ], rules[ 1 ] ) );
                    REQUIRE( entries[ 2 ].key == "Version" );
                    REQUIRE( entries[ 3 ].key == "Build" );
                    REQUIRE( entries[ 4 ].key == "IP address" );
                    REQUIRE( ui.currentRow() == 2 );
                }
            }
        }

        WHEN( "checking key=value with the key Version along with the IPv4 address" )
        {
            ui.check( "key=value" );
            ui.check( "IPv4 address" );
            ui.templateKey->setText( "Version" );

            THEN( "the note names the given key too" )
            {
                REQUIRE( ui.note->text().contains( "“Version”" ) );
                REQUIRE( ui.note->text().contains( "“IP address”" ) );
            }
        }

        WHEN( "checking two templates with the same key no rule has yet" )
        {
            // Both use the key Serial number (#58).
            ui.check( "Serial number" );
            ui.check( "Device serial number" );

            THEN( "the note names that key" )
            {
                REQUIRE( !ui.note->isHidden() );
                REQUIRE( ui.note->text().contains( "“Serial number”" ) );
                REQUIRE( ui.note->text().contains( "alternative" ) );
                REQUIRE( !ui.note->text().contains( "Version" ) );
            }

            AND_WHEN( "checking version too" )
            {
                ui.check( "Version" );

                THEN( "the note names both keys" )
                {
                    REQUIRE( ui.note->text().contains( "“Serial number”" ) );
                    REQUIRE( ui.note->text().contains( "“Version”" ) );
                }
            }

            AND_WHEN( "unchecking one of them" )
            {
                ui.check( "Device serial number", false );

                THEN( "there is no note" )
                {
                    REQUIRE( ui.note->isHidden() );
                }
            }
        }
    }
}

SCENARIO( "The alternative note counts only rules the scanner uses",
          "[ruletemplate][footereditor]" )
{
    GIVEN( "a disabled rule for Version and an enabled one without a line pattern for "
           "IP address" )
    {
        const QList<FooterEntry> rules{ { "Version", "Version=(\\S+)", "", false, {} },
                                        { "IP address", "", "", true, {} } };
        FooterEditor editor( rules );
        TemplateUi ui( editor );
        editor.show();
        ui.open();

        THEN( "neither key gets the note" )
        {
            ui.check( "Version" );
            REQUIRE( ui.note->isHidden() );
            ui.check( "IPv4 address" );
            REQUIRE( ui.note->isHidden() );
        }
    }

    GIVEN( "an enabled rule for Version whose line pattern is invalid" )
    {
        // The scanner skips it, as the rule check decides.
        FooterEditor editor( { { "Version", "Version=(", "", true, {} } } );
        TemplateUi ui( editor );
        editor.show();
        ui.open();

        THEN( "its key gets no note" )
        {
            ui.check( "Version" );
            REQUIRE( ui.note->isHidden() );
        }
    }
}

SCENARIO( "From template… keeps a mapping still being typed", "[ruletemplate][footereditor]" )
{
    GIVEN( "a mapping cell of the first rule open for editing" )
    {
        FooterEditor editor( twoRules() );
        TemplateUi ui( editor );
        editor.show();
        ui.list->setCurrentIndex( ui.model->index( 0, RuleListModel::KeyColumn ) );
        ui.mappings->setCurrentCell( 0, 1 );
        ui.mappings->editItem( ui.mappings->item( 0, 1 ) );
        auto* cell = ui.mappings->viewport()->findChild<QLineEdit*>();
        REQUIRE( cell );
        cell->setText( "typed" );

        WHEN( "clicking From template…" )
        {
            ui.open();

            THEN( "the typed text is stored in its rule before the dialog opens" )
            {
                REQUIRE( editor.entries()[ 0 ].mappings[ 0 ].displayValue == "typed" );
            }
        }
    }
}
