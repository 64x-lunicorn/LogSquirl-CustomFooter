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

#include <QPoint>
#include <QTableView>

namespace custom_footer {

/**
 * The rule list: a table view whose rules are reordered by dragging them,
 * or with Ctrl+Shift+Up / Ctrl+Shift+Down (⇧⌘↑ / ⇧⌘↓ on macOS).
 *
 * The drop moves the rules in the model (RuleListModel::dropMimeData()),
 * which accepts only moves. The view never removes the dragged rows after
 * the drag, as QAbstractItemView would once the drop reports a move: the
 * selected row is then the moved rule itself. So a drag can neither copy
 * nor delete a rule.
 */
class RuleListView : public QTableView {
    Q_OBJECT

public:
    explicit RuleListView( QWidget* parent = nullptr );

Q_SIGNALS:
    /// Ctrl+Shift+Up was pressed: move the selected rule up.
    void moveUpRequested();
    /// Ctrl+Shift+Down was pressed: move the selected rule down.
    void moveDownRequested();

protected:
    void keyPressEvent( QKeyEvent* event ) override;
    void mousePressEvent( QMouseEvent* event ) override;
    void startDrag( Qt::DropActions supportedActions ) override;

private:
    /// Where the drag started, in viewport coordinates: the drag image is
    /// held there.
    QPoint pressPosition_;
};

} // namespace custom_footer
