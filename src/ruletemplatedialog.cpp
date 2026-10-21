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
#include <QLocale>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

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
    list_->setSelectionMode( QAbstractItemView::SingleSelection );
    list_->setAllColumnsShowFocus( true );
    list_->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    list_->header()->setStretchLastSection( true );
    // A group item per group, with its templates as children. Checking a
    // group checks its templates, and it shows whether all, some or none
    // of them are checked (Qt::ItemIsAutoTristate).
    const auto& templates = ruleTemplates();
    QTreeWidgetItem* group = nullptr;
    for ( int i = 0; i < templates.size(); ++i ) {
        const auto& ruleTemplate = templates[ i ];
        if ( !group || group->text( 0 ) != ruleTemplate.group ) {
            group = new QTreeWidgetItem( list_, { ruleTemplate.group } );
            group->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable
                             | Qt::ItemIsAutoTristate );
            group->setCheckState( 0, Qt::Unchecked );
            group->setFirstColumnSpanned( true );
        }
        auto* item = new QTreeWidgetItem( group, { ruleTemplate.name, ruleTemplate.description } );
        item->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable
                        | Qt::ItemNeverHasChildren );
        item->setCheckState( 0, Qt::Unchecked );
        item->setData( 0, Qt::UserRole, i );
        item->setToolTip( 1, ruleTemplate.description );
    }
    list_->expandAll();
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
    buttons_->button( QDialogButtonBox::Ok )->setText( tr( "Add Rules" ) );
    layout->addWidget( buttons_ );

    connect( list_, &QTreeWidget::itemChanged, this, &RuleTemplateDialog::updateChoice );
    connect( keyEdit_, &QLineEdit::textChanged, this, &RuleTemplateDialog::updateChoice );
    // Double-clicking a template's name checks or unchecks it, like its
    // check box; Space does so from the keyboard, and Enter adds the rules.
    list_->viewport()->installEventFilter( this );
    connect( buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject );

    reset( {} );
}

void RuleTemplateDialog::reset( const QStringList& existingKeys )
{
    existingKeys_ = existingKeys;
    keyEdit_->clear();
    // Unchecking a group unchecks its templates.
    for ( int g = 0; g < list_->topLevelItemCount(); ++g ) {
        list_->topLevelItem( g )->setCheckState( 0, Qt::Unchecked );
    }
    list_->setCurrentItem( list_->topLevelItem( 0 ) );
    list_->scrollToTop();
    list_->setFocus();
    updateChoice();
}

QList<FooterEntry> RuleTemplateDialog::entries() const
{
    QList<FooterEntry> entries;
    for ( const int index : checkedTemplates() ) {
        entries.append( ruleTemplates().at( index ).entry( keyEdit_->text() ) );
    }
    return entries;
}

void RuleTemplateDialog::updateChoice()
{
    bool asksForKey = false;
    for ( const int index : checkedTemplates() ) {
        asksForKey = asksForKey || ruleTemplates().at( index ).asksForKey();
    }
    keyEdit_->setEnabled( asksForKey );
    keyLabel_->setEnabled( asksForKey );

    // A template asking for a key without one makes no rule.
    const auto newEntries = entries();
    const bool complete = !newEntries.isEmpty()
                          && std::none_of( newEntries.begin(), newEntries.end(),
                                           []( const FooterEntry& e ) { return e.key.isEmpty(); } );
    buttons_->button( QDialogButtonBox::Ok )->setEnabled( complete );

    // The pattern adds the `=`: one typed with the key is dropped.
    const auto givenKey = RuleTemplate::givenKey( keyEdit_->text() );
    const bool typedEquals = asksForKey && !givenKey.isEmpty()
                             && keyEdit_->text().trimmed().endsWith( QLatin1Char( '=' ) );
    keyHint_->setText( typedEquals
                           ? tr( "The = is added for you: the rule looks for %1=" ).arg( givenKey )
                           : QString() );
    keyHint_->setVisible( typedEquals );

    // Rules sharing a key are alternatives: allowed, but never silently,
    // whether the key is an existing rule's or that of another checked
    // template.
    QStringList usedKeys;
    QStringList sharedKeys;
    QStringList newKeys;
    for ( const auto& entry : newEntries ) {
        if ( entry.key.isEmpty() ) {
            continue;
        }
        if ( existingKeys_.contains( entry.key ) ) {
            if ( !usedKeys.contains( entry.key ) ) {
                usedKeys.append( entry.key );
            }
        }
        else if ( newKeys.contains( entry.key ) && !sharedKeys.contains( entry.key ) ) {
            sharedKeys.append( entry.key );
        }
        newKeys.append( entry.key );
    }
    const auto quotedList = []( const QStringList& keys ) {
        QStringList quoted;
        for ( const auto& key : keys ) {
            quoted.append( tr( "“%1”" ).arg( key ) );
        }
        return QLocale().createSeparatedList( quoted );
    };
    QString note;
    if ( usedKeys.size() == 1 ) {
        note = tr( "A rule with the key “%1” exists already: the new rule becomes an "
                   "alternative for its value: whichever of them matches first in the "
                   "file supplies it." )
                   .arg( usedKeys.first() );
    }
    else if ( usedKeys.size() > 1 ) {
        note = tr( "Rules with the keys %1 exist already: each new rule with one of them "
                   "becomes an alternative for its value: whichever matches first in the file "
                   "supplies it." )
                   .arg( quotedList( usedKeys ) );
    }
    if ( !sharedKeys.isEmpty() ) {
        const auto shared
            = tr( "Several checked templates have the key %1: their rules become alternatives "
                  "for its value: whichever matches first in the file supplies it." )
                  .arg( quotedList( sharedKeys ) );
        note = note.isEmpty() ? shared : note + QLatin1Char( ' ' ) + shared;
    }
    note_->setText( note );
    note_->setVisible( !note.isEmpty() );
}

bool RuleTemplateDialog::eventFilter( QObject* watched, QEvent* event )
{
    if ( watched != list_->viewport() || event->type() != QEvent::MouseButtonDblClick ) {
        return QDialog::eventFilter( watched, event );
    }
    const auto pos = static_cast<QMouseEvent*>( event )->position().toPoint();
    auto* item = list_->itemAt( pos );
    if ( !item || !item->parent() ) {
        return QDialog::eventFilter( watched, event );
    }
    // Where the delegate draws the check box, which toggles on the release
    // that follows.
    QStyleOptionViewItem option;
    option.initFrom( list_ );
    option.rect = list_->visualItemRect( item );
    option.features = QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDisplay;
    option.text = item->text( 0 );
    const auto checkBox
        = list_->style()->subElementRect( QStyle::SE_ItemViewItemCheckIndicator, &option, list_ );
    if ( checkBox.contains( pos ) ) {
        return QDialog::eventFilter( watched, event );
    }
    item->setCheckState( 0, item->checkState( 0 ) == Qt::Checked ? Qt::Unchecked : Qt::Checked );
    return true;
}

QList<int> RuleTemplateDialog::checkedTemplates() const
{
    QList<int> indexes;
    for ( int g = 0; g < list_->topLevelItemCount(); ++g ) {
        const auto* group = list_->topLevelItem( g );
        for ( int i = 0; i < group->childCount(); ++i ) {
            if ( group->child( i )->checkState( 0 ) == Qt::Checked ) {
                indexes.append( group->child( i )->data( 0, Qt::UserRole ).toInt() );
            }
        }
    }
    return indexes;
}

} // namespace custom_footer
