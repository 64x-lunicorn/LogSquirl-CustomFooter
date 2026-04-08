/*
 * Copyright (C) 2026 LogSquirl Contributors
 *
 * This file is part of logsquirl-costume-footer.
 *
 * logsquirl-costume-footer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * logsquirl-costume-footer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logsquirl-costume-footer.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QLabel>
#include <QMap>
#include <QString>
#include <QWidget>

namespace costume_footer {

/**
 * Widget that displays matched key-value pairs.
 *
 * Used both as a status-bar widget (horizontal flow) and as a sidebar widget.
 * Contains a single QLabel that is updated via updateValues().
 */
class FooterDisplayWidget : public QWidget {
    Q_OBJECT

  public:
    explicit FooterDisplayWidget( QWidget* parent = nullptr );

    /// Replace displayed values with a new set of key-value pairs.
    void updateValues( const QMap<QString, QString>& values );

    /// Clear all displayed values.
    void clearValues();

  private:
    QLabel* label_ = nullptr;
};

} // namespace costume_footer
