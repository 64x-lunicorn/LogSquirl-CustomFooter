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

#include <QDialog>
#include <QStringList>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTreeWidget;

namespace custom_footer {

/**
 * Chooses rule templates: the templates with their name and description
 * under their group, each with a check box, and a key field for a
 * template that asks for its key, such as `key=value`. Checking a group
 * checks all its templates; a group with some of them checked shows as
 * partly checked. OK is enabled once at least one template is checked and
 * every checked one has everything it needs.
 *
 * A key typed with its `=`, such as `user=`, loses it, as the pattern adds
 * it; a hint says so.
 *
 * If the key of a new rule is already used by a rule, a note names every
 * such key and says that the new rules become alternatives for them. That
 * is allowed, as rules sharing a key are alternatives, but never happens
 * silently.
 *
 * The editor keeps one dialog and shows it with open(), so tests drive it
 * through its widgets instead of waiting for exec() to return.
 */
class RuleTemplateDialog : public QDialog {
    Q_OBJECT

public:
    explicit RuleTemplateDialog( QWidget* parent = nullptr );

    /// Start over for a new choice: nothing checked, no key, and
    /// @p existingKeys as the keys of the rules the scanner uses
    /// (RuleCheck::keysInUse()).
    void reset( const QStringList& existingKeys );

    /// The rules the checked templates make, in list order, with the given
    /// key for a template that asks for one.
    QList<FooterEntry> entries() const;

private Q_SLOTS:
    void updateChoice();

private:
    /// The indexes of the checked templates in ruleTemplates(), in order.
    QList<int> checkedTemplates() const;

    QStringList existingKeys_;
    QTreeWidget* list_ = nullptr;
    QLabel* keyLabel_ = nullptr;
    QLineEdit* keyEdit_ = nullptr;
    QLabel* keyHint_ = nullptr;
    QLabel* note_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;
};

} // namespace custom_footer
