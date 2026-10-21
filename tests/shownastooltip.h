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
 * @file shownastooltip.h
 * @brief What Qt shows for a tooltip's text, and which resources showing it
 *        loads.
 */

#pragma once

#include <QStringList>
#include <QTextDocument>
#include <QUrl>
#include <QVariant>

namespace custom_footer {

/// A tooltip as Qt shows it.
struct ShownToolTip {
    QString text;          ///< The text on the screen.
    QStringList resources; ///< What showing it asked to load, e.g. images.
};

/// Qt shows a tooltip as rich text as soon as its text looks like HTML,
/// and loads the images that rich text names when it lays it out. This lays
/// it out the same way, recording each resource instead of loading it.
inline ShownToolTip shownAsToolTip( const QString& toolTip )
{
    if ( !Qt::mightBeRichText( toolTip ) ) {
        return { toolTip, {} };
    }

    class RecordingDocument : public QTextDocument {
    public:
        QStringList resources;

    protected:
        QVariant loadResource( int, const QUrl& name ) override
        {
            resources.append( name.toString() );
            return {};
        }
    };

    RecordingDocument document;
    document.setHtml( toolTip );
    document.size(); // lays it out, as showing it does
    return { document.toPlainText(), document.resources };
}

} // namespace custom_footer
