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

#pragma once

#include "footerentry.h"
#include "simplerule.h"

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace custom_footer {

/// Why a rule cannot be used, per field. Empty when the field is fine.
/// Wherever problems are listed, they come in the order of the fields here:
/// key, line pattern, value pattern, end character.
struct RuleProblems {
    QString key;
    QString linePattern;
    QString valuePattern;
    QString endCharacter; ///< Of a simple rule's value end.

    bool isEmpty() const
    {
        return key.isEmpty() && linePattern.isEmpty() && valuePattern.isEmpty()
               && endCharacter.isEmpty();
    }

    /// The problems that are set, in their order.
    QStringList all() const;

    /// The first problem that keeps the rule from matching anything, which
    /// the preview shows instead of a result: any but the key's, as a rule
    /// without a key still matches. Empty when the rule can be previewed.
    QString preventingMatches() const;

    bool operator==( const RuleProblems& other ) const
    {
        return key == other.key && linePattern == other.linePattern
               && valuePattern == other.valuePattern && endCharacter == other.endCharacter;
    }
    bool operator!=( const RuleProblems& other ) const
    {
        return !( *this == other );
    }
};

/**
 * Decides whether rules are usable and, if not, why: the one place that
 * knows what makes a rule unusable, for the scanner and the editor alike.
 *
 * A rule is checked on its own. An invalid line or value pattern is a
 * problem, whether the rule is enabled or not; an enabled rule with a line
 * pattern needs a key, as rules sharing a key are alternatives, but a rule
 * without a line pattern is incomplete and needs nothing; and a simple rule
 * still being written whose end character cannot be used has a problem
 * there. The scanner uses an enabled rule with a line pattern (isUsed()),
 * unless it has problems; the rules can be saved only without any.
 *
 * The check keeps the problems of a list of rules, which the editor changes
 * as its rows change: setRule() checks only the rule it is given, inserted
 * rules are checked once, and removing or moving rules checks nothing. Each
 * pattern is compiled once per check, however often it is checked; its
 * error is remembered.
 */
class RuleCheck {
    Q_DECLARE_TR_FUNCTIONS( RuleCheck )

public:
    /// Why a pattern does not compile, or an empty string if it does.
    using PatternCompiler = std::function<QString( const QString& pattern )>;

    /// A rule as checked: the rule, and the simple rule being written in its
    /// row whose end character keeps it from having patterns yet.
    struct Rule {
        Rule( const FooterEntry& entry = {},
              const std::optional<SimpleRule>& unfinishedSimpleRule = std::nullopt )
            : entry( entry )
            , unfinishedSimpleRule( unfinishedSimpleRule )
        {
        }

        FooterEntry entry;
        std::optional<SimpleRule> unfinishedSimpleRule;
    };

    /// @p compile is the scanner's compiling unless tests count it.
    explicit RuleCheck( PatternCompiler compile = {} );

    /// Whether the scanner looks at @p entry at all: enabled, with a line
    /// pattern. It uses the rule if it has no problems, and reports it
    /// otherwise; any other rule is ignored silently.
    static bool isUsed( const FooterEntry& entry );

    /// The problems of one rule, compiling its patterns.
    static RuleProblems problemsOf( const Rule& rule );

    /// The lines listing @p problems of the rule in @p row, counting rows
    /// from 0: "Rule N: …", one per problem, in their order.
    static QStringList listing( int row, const RuleProblems& problems );

    /// Check all rules anew.
    void setRules( const QList<Rule>& rules );
    /// Check the rule in @p row again, and only it.
    void setRule( int row, const Rule& rule );
    /// Check the new rules, inserted before @p row.
    void insertRules( int row, const QList<Rule>& rules );
    void removeRules( int row, int count );
    /// Move @p count rules from @p first to before @p destination, which is
    /// counted before the move, as QAbstractItemModel::moveRows() does.
    void moveRules( int first, int count, int destination );

    int count() const
    {
        return static_cast<int>( rows_.size() );
    }

    RuleProblems problems( int row ) const;

    /// The problems of all rules, numbered by their current rows.
    QStringList listing() const;

    /// Whether no rule has a problem.
    bool canSave() const;

    /// The keys of the rules the scanner uses, each once, in the order of
    /// their first rule.
    QStringList keysInUse() const;

private:
    struct Row {
        RuleProblems problems;
        /// Whether the scanner uses the rule.
        bool used = false;
        QString key;
    };

    Row check( const Rule& rule );
    /// Why @p pattern does not compile, compiling it only the first time.
    QString patternError( const QString& pattern );

    PatternCompiler compile_;
    QHash<QString, QString> patternErrors_;
    QList<Row> rows_;
};

} // namespace custom_footer
