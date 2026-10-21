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

#include <QList>
#include <QString>

namespace custom_footer {

/**
 * A ready-made rule for a common value, such as a version or an IPv4
 * address, that users only need to adjust.
 *
 * A template that fits the simple form is a SimpleRule, built into patterns
 * by applySimpleRule(), so it opens in simple mode. One whose value needs a
 * shape, such as a dotted version or a timestamp, has an advanced line
 * pattern instead and opens in advanced mode. A template may bring value
 * mappings, so that a raw code reads as text, e.g. the manufacturer of a
 * VIN's first three characters.
 */
struct RuleTemplate {
    QString name;        ///< E.g. "Version".
    QString description; ///< One line: what it finds.
    /// The key of the rule; empty if the template asks for it.
    QString key;
    /// A simple rule, used without a linePattern.
    SimpleRule simple;
    /// An advanced line pattern, with the value as the first capturing
    /// group. Empty for a simple template. With asksForKey(), `%1` in it
    /// stands for the given key, escaped to match literally.
    QString linePattern;
    /// The group the dialog shows it under, e.g. "General"; set for all
    /// templates of a group by makeTemplates(). The initializers leave it
    /// and the mappings out: the default initializers say so.
    QString group {};
    /// Value mappings the new rule starts with, e.g. a code to its name;
    /// empty for most templates.
    QList<ValueMapping> mappings {};

    /// Whether the user gives the key, e.g. for `key=value`.
    bool asksForKey() const
    {
        return key.isEmpty();
    }

    /// Whether the rule is built from a simple rule, and opens in simple
    /// mode.
    bool isSimple() const
    {
        return linePattern.isEmpty();
    }

    /**
     * The new rule, enabled and with the template's mappings, which users
     * can edit like any others. @p askedKey is the key of
     * a template that asks for one, see givenKey(); other templates ignore
     * it. A template asking for a key gives an empty rule without one.
     */
    FooterEntry entry( const QString& askedKey = QString() ) const;

    /// The key a template asking for one uses for @p askedKey: trimmed,
    /// without one `=` at its end, which the pattern adds anyway.
    static QString givenKey( const QString& askedKey );
};

/// The rule templates, in the order they are offered: group by group, the
/// templates of a group next to each other.
const QList<RuleTemplate>& ruleTemplates();

} // namespace custom_footer
