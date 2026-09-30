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

#include "footervalue.h"

#include <QList>
#include <QToolButton>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QMenu;

namespace custom_footer {

/**
 * One value in the footer: a flat button that copies the value on a click.
 *
 * It takes the keyboard focus with Tab and copies on Space, Return or the
 * copy shortcut too, before any shortcut of the window. Its tooltip tells
 * which rule supplied the value, and the raw value a mapping replaced. Its
 * context menu offers copying the value, or key and value; more actions on
 * the value can be added to contextMenu().
 */
class FooterValueItem : public QToolButton {
    Q_OBJECT

public:
    /// How long the confirmation of a copy is shown.
    static constexpr int kConfirmationMs = 1500;

    explicit FooterValueItem( const FooterValue& value, QWidget* parent = nullptr );

    const FooterValue& value() const
    {
        return value_;
    }

    /// Show another value, e.g. a new one for the same key.
    void setValue( const FooterValue& value );

    /// The menu shown on a right click, or with the context menu key.
    QMenu* contextMenu() const
    {
        return contextMenu_;
    }

    /// Put the value on the clipboard and confirm it briefly.
    void copyToClipboard();

    /// Put some text on the clipboard and confirm it briefly at this item.
    void copyText( const QString& text );

protected:
    bool event( QEvent* event ) override;
    void keyPressEvent( QKeyEvent* event ) override;

private:
    FooterValue value_;
    QMenu* contextMenu_ = nullptr;
};

/**
 * Widget that displays matched key-value pairs, one FooterValueItem per
 * value after its key.
 *
 * The item of a key is kept while the key is shown, so that the keyboard
 * focus, a confirmation or an open context menu survive new values.
 */
class FooterDisplayWidget : public QWidget {
    Q_OBJECT

public:
    explicit FooterDisplayWidget( QWidget* parent = nullptr );

    /// Replace displayed values with an ordered list of values.
    void updateValues( const QList<FooterValue>& values );

    /// Clear all displayed values.
    void clearValues();

    /// The values shown, in order.
    QList<FooterValue> values() const;

    /// Every shown key and value as "key: value", one per line.
    QString allValuesText() const;

private:
    /// A key and its value item, laid out together.
    struct Entry {
        QWidget* box = nullptr;
        QLabel* key = nullptr;
        FooterValueItem* item = nullptr;
    };

    Entry createEntry( const FooterValue& value );

    QHBoxLayout* layout_ = nullptr;
    QList<Entry> entries_;      ///< In the order shown.
    QList<QLabel*> separators_; ///< Between the entries.
};

} // namespace custom_footer
