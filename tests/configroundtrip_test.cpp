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
 * @file configroundtrip_test.cpp
 * @brief Configs written by 0.3.0 pass through the rule editor unchanged.
 */

#include <catch2/catch.hpp>

#include "footerconfig.h"
#include "footereditor.h"
#include "rulelistmodel.h"

#include <QFile>
#include <QTableView>
#include <QTemporaryDir>

using namespace custom_footer;

namespace {

/// The file as QSettings and a text-mode QFile write it on this platform.
QByteArray withNativeLineEnds( QByteArray text )
{
#ifdef Q_OS_WIN
    text.replace( "\n", "\r\n" );
#endif
    return text;
}

QByteArray readFile( const QString& path )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::ReadOnly ) );
    return file.readAll();
}

void writeFile( const QString& path, const QByteArray& content )
{
    QFile file( path );
    REQUIRE( file.open( QIODevice::WriteOnly ) );
    REQUIRE( file.write( content ) == content.size() );
}

/// Pass rules through the editor as the user would when only looking at
/// them: every rule is selected and shown in the detail panel.
QList<FooterEntry> throughEditor( const QList<FooterEntry>& entries )
{
    FooterEditor editor( entries );
    auto* list = editor.findChild<QTableView*>( "ruleList" );
    REQUIRE( list );
    for ( int row = 0; row < list->model()->rowCount(); ++row ) {
        list->setCurrentIndex( list->model()->index( row, RuleListModel::KeyColumn ) );
    }
    return editor.entries();
}

/// `custom_footer.ini` as written by 0.3.0: a disabled rule, mappings, two
/// rules sharing a key, quoting and non-ASCII text, an empty rule, and the
/// scan limit, which saving the rules must keep.
const char kIni030[] = "[entries]\n"
                       "1\\enabled=true\n"
                       "1\\key=VIN\n"
                       "1\\linePattern=VIN:\\\\s+(\\\\S+)\n"
                       "1\\mappings\\size=0\n"
                       "1\\valuePattern=\n"
                       "2\\enabled=false\n"
                       "2\\key=Component Protection\n"
                       "2\\linePattern=isComponentProtectionEnabled\n"
                       "2\\mappings\\1\\display=Enabled\n"
                       "2\\mappings\\1\\pattern=true\n"
                       "2\\mappings\\2\\display=Disabled\n"
                       "2\\mappings\\2\\pattern=false\n"
                       "2\\mappings\\size=2\n"
                       "2\\valuePattern=:\\\\s+(\\\\S+)$\n"
                       "3\\enabled=true\n"
                       "3\\key=Build\n"
                       "3\\linePattern=\"build=(\\\\d+)\"\n"
                       "3\\mappings\\size=0\n"
                       "3\\valuePattern=\n"
                       "4\\enabled=true\n"
                       "4\\key=Build\n"
                       "4\\linePattern=Build #(\\\\d+)\n"
                       "4\\mappings\\1\\display=none\n"
                       "4\\mappings\\1\\pattern=0\n"
                       "4\\mappings\\size=1\n"
                       "4\\valuePattern=#(\\\\d+)\n"
                       "5\\enabled=true\n"
                       "5\\key=\"Gr\xC3\xBC\xC3\x9F"
                       "e, \\\"quoted\\\"; x=y\"\n"
                       "5\\linePattern=[Ss]tatus\\\\t(\\\\w+) %20 \xC3\xBCmlaut\n"
                       "5\\mappings\\1\\display=\"c;d \\\"e\\\"\"\n"
                       "5\\mappings\\1\\pattern=\"a=b\"\n"
                       "5\\mappings\\size=1\n"
                       "5\\valuePattern=\"(?<=: )\\\\S+\"\n"
                       "6\\enabled=true\n"
                       "6\\key=\n"
                       "6\\linePattern=\n"
                       "6\\mappings\\size=0\n"
                       "6\\valuePattern=\n"
                       "size=6\n"
                       "\n"
                       "[scan]\n"
                       "maxLines=5000\n";

/// The same rules as exported to JSON by 0.3.0.
const char kJson030[] = R"json({
    "entries": [
        {
            "enabled": true,
            "key": "VIN",
            "linePattern": "VIN:\\s+(\\S+)",
            "mappings": [
            ],
            "valuePattern": ""
        },
        {
            "enabled": false,
            "key": "Component Protection",
            "linePattern": "isComponentProtectionEnabled",
            "mappings": [
                {
                    "display": "Enabled",
                    "pattern": "true"
                },
                {
                    "display": "Disabled",
                    "pattern": "false"
                }
            ],
            "valuePattern": ":\\s+(\\S+)$"
        },
        {
            "enabled": true,
            "key": "Build",
            "linePattern": "build=(\\d+)",
            "mappings": [
            ],
            "valuePattern": ""
        },
        {
            "enabled": true,
            "key": "Build",
            "linePattern": "Build #(\\d+)",
            "mappings": [
                {
                    "display": "none",
                    "pattern": "0"
                }
            ],
            "valuePattern": "#(\\d+)"
        },
        {
            "enabled": true,
            "key": "Gr)json"
                        "\xC3\xBC\xC3\x9F"
                        R"json(e, \"quoted\"; x=y",
            "linePattern": "[Ss]tatus\\t(\\w+) %20 )json"
                        "\xC3\xBC"
                        R"json(mlaut",
            "mappings": [
                {
                    "display": "c;d \"e\"",
                    "pattern": "a=b"
                }
            ],
            "valuePattern": "(?<=: )\\S+"
        },
        {
            "enabled": true,
            "key": "",
            "linePattern": "",
            "mappings": [
            ],
            "valuePattern": ""
        }
    ],
    "version": 1
}
)json";

/// `custom_footer.ini` from before linePattern existed, with the legacy
/// regex key.
const char kLegacyIni[] = "[entries]\n"
                          "1\\key=VIN\n"
                          "1\\regex=VIN:(\\\\S+)\n"
                          "1\\enabled=false\n"
                          "1\\mappings\\1\\pattern=x\n"
                          "1\\mappings\\1\\display=y\n"
                          "1\\mappings\\size=1\n"
                          "size=1\n";

} // namespace

SCENARIO( "A config saved by 0.3.0 passes through the editor unchanged",
          "[footereditor][footerconfig]" )
{
    QTemporaryDir dir;
    REQUIRE( dir.isValid() );
    const auto iniPath = dir.path() + "/custom_footer.ini";

    GIVEN( "a custom_footer.ini written by 0.3.0" )
    {
        const auto original = withNativeLineEnds( kIni030 );
        writeFile( iniPath, original );

        WHEN( "its rules are loaded, opened in the editor and saved" )
        {
            const auto loaded = FooterConfig::loadEntries( dir.path() );
            REQUIRE( loaded.size() == 6 );
            REQUIRE( FooterConfig::saveEntries( dir.path(), throughEditor( loaded ) ) );

            THEN( "the file is byte for byte the same" )
            {
                REQUIRE( readFile( iniPath ) == original );
            }
        }
    }

    GIVEN( "a rules file exported by 0.3.0" )
    {
        const auto original = withNativeLineEnds( kJson030 );
        const auto importPath = dir.path() + "/import.json";
        const auto exportPath = dir.path() + "/export.json";
        writeFile( importPath, original );

        WHEN( "it is imported into the editor and exported again" )
        {
            QString error;
            const auto imported = FooterConfig::importFromJson( importPath, &error );
            REQUIRE( error.isEmpty() );
            REQUIRE( imported.size() == 6 );
            REQUIRE( FooterConfig::exportToJson( exportPath, throughEditor( imported ) ) );

            THEN( "the file is byte for byte the same" )
            {
                REQUIRE( readFile( exportPath ) == original );
            }
        }
    }

    GIVEN( "a custom_footer.ini from before linePattern, with the legacy regex key" )
    {
        writeFile( iniPath, kLegacyIni );

        WHEN( "its rules are opened in the editor and saved" )
        {
            const auto loaded = FooterConfig::loadEntries( dir.path() );
            REQUIRE( loaded.size() == 1 );
            REQUIRE( loaded[ 0 ].linePattern == "VIN:(\\S+)" );
            REQUIRE( FooterConfig::saveEntries( dir.path(), throughEditor( loaded ) ) );
            const auto saved = readFile( iniPath );

            THEN( "they are saved exactly as without the editor" )
            {
                QTemporaryDir plainDir;
                REQUIRE( plainDir.isValid() );
                writeFile( plainDir.path() + "/custom_footer.ini", kLegacyIni );
                REQUIRE( FooterConfig::saveEntries(
                    plainDir.path(), FooterConfig::loadEntries( plainDir.path() ) ) );
                REQUIRE( saved == readFile( plainDir.path() + "/custom_footer.ini" ) );
                REQUIRE( saved.contains( "1\\linePattern=VIN:(\\\\S+)" ) );
            }

            AND_WHEN( "the saved file goes through the editor again" )
            {
                REQUIRE( FooterConfig::saveEntries(
                    dir.path(), throughEditor( FooterConfig::loadEntries( dir.path() ) ) ) );

                THEN( "it stays byte for byte the same" )
                {
                    REQUIRE( readFile( iniPath ) == saved );
                }
            }
        }
    }
}
