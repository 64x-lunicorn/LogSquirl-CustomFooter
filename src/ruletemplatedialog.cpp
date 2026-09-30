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

#include "ruletemplatedialog.h"
#include "ruletemplate.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace custom_footer {

RuleTemplateDialog::RuleTemplateDialog( QWidget* parent )
    : QDialog( parent )
{
    setObjectName( "ruleTemplateDialog" );
    setWindowTitle( tr( "Add Rule from Template" ) );
    setMinimumWidth( 560 );

    auto* layout = new QVBoxLayout( this );

    list_ = new QTreeWidget( this );
    list_->setObjectName( "templateList" );
    list_->setColumnCount( 2 );
    list_->setHeaderLabels( { tr( "Template" ), tr( "Finds" ) } );
    list_->setRootIsDecorated( false );
    list_->setSelectionMode( QAbstractItemView::SingleSelection );
    list_->setAllColumnsShowFocus( true );
    list_->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    list_->header()->setStretchLastSection( true );
    for ( const auto& ruleTemplate : ruleTemplates() ) {
        auto* item = new QTreeWidgetItem( list_, { ruleTemplate.name, ruleTemplate.description } );
        item->setToolTip( 1, ruleTemplate.description );
    }
    layout->addWidget( list_, 1 );

    auto* form = new QFormLayout;
    keyEdit_ = new QLineEdit( this );
    keyEdit_->setObjectName( "templateKeyEdit" );
    keyEdit_->setPlaceholderText( tr( "E.g. user, to find the value of user" ) );
    keyLabel_ = new QLabel( tr( "&Key:" ), this );
    keyLabel_->setBuddy( keyEdit_ );
    form->addRow( keyLabel_, keyEdit_ );
    keyHint_ = new QLabel( this );
    keyHint_->setObjectName( "templateKeyHint" );
    keyHint_->setWordWrap( true );
    keyHint_->hide();
    form->addRow( QString(), keyHint_ );
    layout->addLayout( form );

    note_ = new QLabel( this );
    note_->setObjectName( "templateKeyNote" );
    note_->setWordWrap( true );
    note_->hide();
    layout->addWidget( note_ );

    buttons_ = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
    buttons_->button( QDialogButtonBox::Ok )->setText( tr( "Add Rule" ) );
    layout->addWidget( buttons_ );

    connect( list_, &QTreeWidget::currentItemChanged, this, &RuleTemplateDialog::updateChoice );
    connect( keyEdit_, &QLineEdit::textChanged, this, &RuleTemplateDialog::updateChoice );
    connect( list_, &QTreeWidget::itemActivated, this, [ this ] {
        if ( buttons_->button( QDialogButtonBox::Ok )->isEnabled() ) {
            accept();
        }
        else {
            keyEdit_->setFocus();
        }
    } );
    connect( buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject );

    reset( {} );
}

void RuleTemplateDialog::reset( const QStringList& existingKeys )
{
    existingKeys_ = existingKeys;
    keyEdit_->clear();
    list_->setCurrentItem( list_->topLevelItem( 0 ) );
    list_->setFocus();
    updateChoice();
}

FooterEntry RuleTemplateDialog::entry() const
{
    const int index = chosenTemplate();
    if ( index < 0 ) {
        return {};
    }
    return ruleTemplates().at( index ).entry( keyEdit_->text() );
}

void RuleTemplateDialog::updateChoice()
{
    const int index = chosenTemplate();
    const bool asksForKey = index >= 0 && ruleTemplates().at( index ).asksForKey();
    keyEdit_->setEnabled( asksForKey );
    keyLabel_->setEnabled( asksForKey );

    const auto key = entry().key;
    buttons_->button( QDialogButtonBox::Ok )->setEnabled( !key.isEmpty() );

    // The pattern adds the `=`: one typed with the key is dropped.
    const bool typedEquals
        = asksForKey && !key.isEmpty() && keyEdit_->text().trimmed().endsWith( QLatin1Char( '=' ) );
    keyHint_->setText( typedEquals
                           ? tr( "The = is added for you: the rule looks for %1=" ).arg( key )
                           : QString() );
    keyHint_->setVisible( typedEquals );

    // Rules sharing a key are alternatives: allowed, but never silently.
    const bool used = !key.isEmpty() && existingKeys_.contains( key );
    note_->setText( used ? tr( "A rule with the key “%1” exists already: the new rule becomes an "
                               "alternative for its value: whichever of them matches first in the "
                               "file supplies it." )
                               .arg( key )
                         : QString() );
    note_->setVisible( used );
}

int RuleTemplateDialog::chosenTemplate() const
{
    auto* item = list_->currentItem();
    return item ? list_->indexOfTopLevelItem( item ) : -1;
}

} // namespace custom_footer
