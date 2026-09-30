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

    void choose( const QString& name ) const
    {
        const auto items = templates->findItems( name, Qt::MatchExactly, 0 );
        REQUIRE( items.size() == 1 );
        templates->setCurrentItem( items.first() );
    }

    QPushButton* ok() const
    {
        return buttons->button( QDialogButtonBox::Ok );
    }

    QPushButton* cancel() const
    {
        return buttons->button( QDialogButtonBox::Cancel );
    }

    /// Choose a template and click OK.
    void add( const QString& name, const QString& askedKey = QString() )
    {
        open();
        choose( name );
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

} // namespace

SCENARIO( "Rule templates are named and described", "[ruletemplate]" )
{
    GIVEN( "the templates" )
    {
        const auto& templates = ruleTemplates();

        THEN( "there are the six of the issue, each with a name and a one-line description" )
        {
            QStringList names;
            for ( const auto& t : templates ) {
                names.append( t.name );
                REQUIRE( !t.description.isEmpty() );
                REQUIRE( !t.description.contains( '\n' ) );
            }
            REQUIRE( names
                     == QStringList{ "Version", "Build number", "Serial number", "IPv4 address",
                                     "Timestamp (ISO 8601)", "key=value" } );
        }

        THEN( "every template makes an enabled rule without mappings whose pattern compiles" )
        {
            for ( const auto& t : templates ) {
                INFO( t.name.toStdString() );
                const auto entry = t.entry( "k" );
                REQUIRE( !entry.key.isEmpty() );
                REQUIRE( !entry.linePattern.isEmpty() );
                REQUIRE( entry.valuePattern.isEmpty() );
                REQUIRE( entry.enabled );
                REQUIRE( entry.mappings.isEmpty() );
                REQUIRE( FooterScanner::patternError( entry.linePattern ).isEmpty() );
            }
        }
    }
}

SCENARIO( "Rule templates are simple where the value fits the simple form", "[ruletemplate]" )
{
    GIVEN( "each template" )
    {
        THEN( "build number and serial number are simple rules" )
        {
            for ( const auto* name : { "Build number", "Serial number" } ) {
                INFO( name );
                const auto& t = templateNamed( name );
                REQUIRE( t.isSimple() );
                const auto entry = t.entry( "user" );
                REQUIRE( simpleRuleOf( entry ).has_value() );
                REQUIRE( entry.linePattern == simpleLinePattern( *simpleRuleOf( entry ) ) );
            }
        }

        THEN( "version, IPv4 address, timestamp and key=value need a shape and are advanced" )
        {
            for ( const auto* name :
                  { "Version", "IPv4 address", "Timestamp (ISO 8601)", "key=value" } ) {
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

SCENARIO( "The IPv4 template finds dotted quads", "[ruletemplate]" )
{
    THEN( "four numbers from 0 to 255, anywhere in the line" )
    {
        requireFinds( "IPv4 address", { { "connected to 192.168.1.20:8080", "192.168.1.20" },
                                        { "10.0.0.1", "10.0.0.1" },
                                        { "peer=255.255.255.0.", "255.255.255.0" },
                                        { "from 0.0.0.0 and 1.2.3.4", "0.0.0.0" } } );
    }

    THEN( "between punctuation, and before a dot ending a sentence" )
    {
        requireFinds( "IPv4 address", { { "ip=10.0.0.1,", "10.0.0.1" },
                                        { "(192.168.1.1)", "192.168.1.1" },
                                        { "Connected to 10.0.0.1.", "10.0.0.1" } } );
    }

    THEN( "not out of range, too few or too many parts, with leading zeros, or in a word" )
    {
        requireFindsNothing( "IPv4 address",
                             { "256.1.1.1", "1.2.3", "1.2.3.4.5", "01.2.3.4", "version 1.2.3",
                               "1.2.3.999", "a.b.c.d", "v1.2.3.4", "x.1.2.3.4", "1.2.3.4a" } );
    }
}

SCENARIO( "The timestamp template finds ISO 8601 date and time", "[ruletemplate]" )
{
    THEN( "date, T or a space, time, optional seconds, fraction and offset" )
    {
        requireFinds( "Timestamp (ISO 8601)",
                      { { "2024-01-15T10:30:00Z start", "2024-01-15T10:30:00Z" },
                        { "[2024-01-15 10:30:00.123] INFO", "2024-01-15 10:30:00.123" },
                        { "at 2024-12-31T23:59:59,5+01:00.", "2024-12-31T23:59:59,5+01:00" },
                        { "ts=2024-02-29T08:15-0500 x", "2024-02-29T08:15-0500" },
                        { "2024-06-01T00:00:00", "2024-06-01T00:00:00" } } );
    }

    THEN( "an offset of hours only, and lower-case t and z" )
    {
        requireFinds( "Timestamp (ISO 8601)",
                      { { "2024-01-15T10:30:00+01 up", "2024-01-15T10:30:00+01" },
                        { "2024-01-15 10:30:00-05", "2024-01-15 10:30:00-05" },
                        { "2024-01-15t10:30:00z", "2024-01-15t10:30:00z" } } );
    }

    THEN( "not a date alone, another format, or values out of range" )
    {
        requireFindsNothing( "Timestamp (ISO 8601)",
                             { "2024-01-15", "2024/01/15 10:30:00", "15.01.2024 10:30",
                               "2024-13-01T10:00:00", "2024-01-32T10:00", "2024-01-15T24:00",
                               "2024-01-15T10:60", "12024-01-15T10:30:00", "10:30:00",
                               "2024-01-15T10:30:5", "2024-01-15T10:30:00.5:1" } );
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
            REQUIRE( entry.linePattern == "(?<![\\w.-])user=(\\S+)" );
        }

        THEN( "the value directly after = ends at whitespace" )
        {
            for ( const Sample& sample :
                  std::initializer_list<Sample>{ { "user=alice", "alice" },
                                                 { "level=info user=alice msg=hi", "alice" },
                                                 { "a=1 user=x;y=2", "x;y=2" },
                                                 { "superuser=root user=alice", "alice" },
                                                 { "user= msg=hi user=bob", "bob" } } ) {
                INFO( sample.line );
                REQUIRE( extract( entry, sample.line ) == QString( sample.value ) );
            }
        }

        THEN( "not for other keys, another separator, or without a value" )
        {
            for ( const auto* line :
                  { "users: alice", "user: alice", "User=alice", "user=", "user= msg=hi",
                    "superuser=root", "a.user=x", "my-user=x" } ) {
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

            THEN( "it lists every template with its name and description" )
            {
                const auto& templates = ruleTemplates();
                REQUIRE( ui.templates->topLevelItemCount() == templates.size() );
                for ( int i = 0; i < templates.size(); ++i ) {
                    REQUIRE( ui.templates->topLevelItem( i )->text( 0 ) == templates[ i ].name );
                    REQUIRE( ui.templates->topLevelItem( i )->text( 1 )
                             == templates[ i ].description );
                }
            }

            THEN( "the first template is chosen and needs no key" )
            {
                REQUIRE( ui.templates->currentItem() == ui.templates->topLevelItem( 0 ) );
                REQUIRE( !ui.templateKey->isEnabled() );
                REQUIRE( ui.ok()->isEnabled() );
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
            ui.choose( "IPv4 address" );
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

            THEN( "it is the only rule, selected" )
            {
                REQUIRE( editor.entries().size() == 1 );
                REQUIRE( ui.currentRow() == 0 );
                REQUIRE( ui.key->text() == templateNamed( "IPv4 address" ).key );
            }
        }
    }
}

SCENARIO( "The key=value template asks for the key", "[ruletemplate][footereditor]" )
{
    GIVEN( "the template dialog with key=value chosen" )
    {
        FooterEditor editor( twoRules() );
        TemplateUi ui( editor );
        editor.show();
        ui.open();
        ui.choose( "key=value" );

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
                    REQUIRE( ui.templates->currentItem() == ui.templates->topLevelItem( 0 ) );
                }
            }
        }

        WHEN( "choosing a template that needs no key" )
        {
            ui.choose( "Version" );

            THEN( "the key field is disabled again" )
            {
                REQUIRE( !ui.templateKey->isEnabled() );
                REQUIRE( ui.ok()->isEnabled() );
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

        WHEN( "choosing the version template" )
        {
            ui.choose( "Version" );

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

        WHEN( "choosing a template with another key" )
        {
            ui.choose( "Timestamp (ISO 8601)" );

            THEN( "there is no note" )
            {
                REQUIRE( ui.note->isHidden() );
            }
        }

        WHEN( "giving key=value the key Version" )
        {
            ui.choose( "key=value" );
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
            ui.choose( "Version" );
            REQUIRE( ui.note->isHidden() );
            ui.choose( "IPv4 address" );
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
