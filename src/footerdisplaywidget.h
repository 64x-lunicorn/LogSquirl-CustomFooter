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

#include "elidedtext.h"
#include "footervalue.h"

#include <QList>
#include <QToolButton>
#include <QWidget>

class QAction;
class QHBoxLayout;
class QLabel;
class QMenu;

namespace custom_footer {

/**
 * One value in the footer: a flat button that copies the value on a click.
 *
 * It takes the keyboard focus with Tab and copies on Space, Return or the
 * copy shortcut too, before any shortcut of the window. Its tooltip tells
 * which rule supplied the value, the raw value a mapping replaced, and the
 * Log Line it was found in. Its context menu offers copying the value, or
 * key and value; more actions on the value can be added to contextMenu().
 *
 * Where going to the Log Line is offered (setGoToLineOffered()) and the
 * value's line is known, a double-click and the context menu's "Go to Log
 * Line" emit goToLineRequested(); the context menu key reaches it from the
 * keyboard. A double-click begins with a click, so it copies the value too;
 * its second press is not taken as another click.
 */
class FooterValueItem : public QToolButton {
    Q_OBJECT

public:
    /// How long the confirmation of a copy is shown.
    static constexpr int kConfirmationMs = 1500;

    /// How many UTF-16 units of a value, and of its raw value, the button,
    /// its tooltip and its accessible texts show at most. A longer one is
    /// cut there, before a character rather than within it, and shown with
    /// an ellipsis; copying it still copies all of it.
    static constexpr int kMaxShownLength = kMaxShownValueLength;

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

    /// Show @p message briefly at this item, as a copy is confirmed, and
    /// announce it to screen readers. No dialog.
    void showNotice( const QString& message );

    /// Whether the item offers going to the value's Log Line. Not by default.
    void setGoToLineOffered( bool offered );

    /// Whether going to the Log Line can be asked for now: it is offered,
    /// and the value's line is known.
    bool canGoToLine() const;

signals:
    /// The user asked to go to the value's Log Line.
    void goToLineRequested();

protected:
    bool event( QEvent* event ) override;
    void keyPressEvent( QKeyEvent* event ) override;
    void mouseDoubleClickEvent( QMouseEvent* event ) override;

private:
    /// Show what can be done with the value: tooltip, accessible texts and
    /// the context menu's entries.
    void updateOffers();
    /// Emit goToLineRequested(), if it can be asked for.
    void requestGoToLine();

    FooterValue value_;
    QMenu* contextMenu_ = nullptr;
    QAction* goToLine_ = nullptr;
    bool goToLineOffered_ = false;
};

/**
 * Widget that displays matched key-value pairs, one FooterValueItem per
 * value after its key.
 *
 * The item of a key is kept while the key is shown, so that the keyboard
 * focus, a confirmation or an open context menu survive new values.
 *
 * It knows nothing of the host: whoever can go to a Log Line offers it with
 * setGoToLineOffered(), handles goToLineRequested(), and may tell the user
 * how it went with notify().
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

    /// Whether the values offer going to their Log Line, by a double-click
    /// and in their context menu, to values shown later too. Not by default.
    void setGoToLineOffered( bool offered );

    /// Show @p message briefly at the value of @p key, as a copy is
    /// confirmed; nothing when the key is not shown.
    void notify( const QString& key, const QString& message );

signals:
    /// The user asked to go to the Log Line of @p value, which is known
    /// (FooterValue::lineNumber > 0). Only emitted while it is offered.
    void goToLineRequested( const FooterValue& value );

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
    bool goToLineOffered_ = false;
};

} // namespace custom_footer
