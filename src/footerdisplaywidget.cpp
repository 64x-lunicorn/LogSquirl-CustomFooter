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

#include "footerdisplaywidget.h"

#include <QHBoxLayout>

namespace costume_footer {

FooterDisplayWidget::FooterDisplayWidget( QWidget* parent )
    : QWidget( parent )
{
    auto* layout = new QHBoxLayout( this );
    layout->setContentsMargins( 4, 0, 4, 0 );

    label_ = new QLabel( this );
    label_->setTextInteractionFlags( Qt::TextSelectableByMouse );
    label_->setWordWrap( false );
    layout->addWidget( label_ );
}

void FooterDisplayWidget::updateValues( const QList<QPair<QString, QString>>& values )
{
    if ( values.isEmpty() ) {
        clearValues();
        return;
    }

    QStringList parts;
    for ( const auto& [key, value] : values ) {
        parts.append( QString( "<b>%1:</b> %2" ).arg( key.toHtmlEscaped(),
                                                       value.toHtmlEscaped() ) );
    }

    label_->setText( parts.join( "  |  " ) );
}

void FooterDisplayWidget::clearValues()
{
    label_->clear();
}

} // namespace costume_footer
