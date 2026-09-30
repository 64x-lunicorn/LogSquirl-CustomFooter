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

#include "ruledetailpanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <limits>

namespace custom_footer {

namespace {

/// A colour for style sheets.
QString css( const QColor& color )
{
    return QStringLiteral( "rgba(%1, %2, %3, %4)" )
        .arg( color.red() )
        .arg( color.green() )
        .arg( color.blue() )
        .arg( color.alpha() );
}

/// How a field whose content keeps the rules from being saved looks. With a
/// border, native styles draw the background too.
QString problemFieldStyle()
{
    return QStringLiteral( "QLineEdit { border: 1px solid %1; border-radius: 3px;"
                           " background-color: %2; padding: 1px; }" )
        .arg( css( problemColor() ), css( problemBackgroundColor() ) );
}

void markField( QLineEdit* field, QLabel* label, const QString& problem )
{
    if ( field->toolTip() == problem ) {
        return;
    }
    field->setToolTip( problem );
    field->setStyleSheet( problem.isEmpty() ? QString() : problemFieldStyle() );
    label->setText( problem );
    label->setVisible( !problem.isEmpty() );
}

} // namespace

RuleDetailPanel::RuleDetailPanel( QWidget* parent )
    : QGroupBox( tr( "Rule" ), parent )
{
    setObjectName( "ruleDetailPanel" );
    auto* panelLayout = new QVBoxLayout( this );

    // ── The rule's fields ────────────────────────────────────────────────
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy( QFormLayout::AllNonFixedFieldsGrow );

    enabledCheck_ = new QCheckBox( tr( "Enabled" ), this );
    enabledCheck_->setObjectName( "ruleEnabledCheck" );
    form->addRow( QString(), enabledCheck_ );

    keyEdit_ = addField( form, tr( "&Key:" ), "keyEdit", &keyProblem_ );
    keyEdit_->setPlaceholderText( tr( "Shown in the footer, e.g. VIN" ) );
    linePatternEdit_
        = addField( form, tr( "&Line pattern:" ), "linePatternEdit", &linePatternProblem_ );
    linePatternEdit_->setPlaceholderText( tr( "Regex finding the line, e.g. VIN:\\s+(\\S+)" ) );
    valuePatternEdit_
        = addField( form, tr( "&Value pattern:" ), "valuePatternEdit", &valuePatternProblem_ );
    valuePatternEdit_->setPlaceholderText(
        tr( "Optional regex for the value in the line, e.g. :\\s+(\\S+)$" ) );

    // ── Its value mappings ───────────────────────────────────────────────
    auto* mappings = new QWidget( this );
    auto* mappingLayout = new QVBoxLayout( mappings );
    mappingLayout->setContentsMargins( 0, 0, 0, 0 );

    mappingTable_ = new QTableWidget( 0, 2, mappings );
    mappingTable_->setObjectName( "mappingTable" );
    mappingTable_->setHorizontalHeaderLabels( { tr( "Raw value" ), tr( "Display" ) } );
    mappingTable_->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    mappingTable_->verticalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
    mappingTable_->setSelectionBehavior( QAbstractItemView::SelectRows );
    mappingTable_->setSelectionMode( QAbstractItemView::SingleSelection );
    mappingLayout->addWidget( mappingTable_ );

    auto* mappingTools = new QHBoxLayout;
    addMappingButton_ = new QToolButton( mappings );
    addMappingButton_->setObjectName( "addMappingButton" );
    addMappingButton_->setText( "+" );
    addMappingButton_->setToolTip( tr( "Add mapping" ) );
    mappingTools->addWidget( addMappingButton_ );

    removeMappingButton_ = new QToolButton( mappings );
    removeMappingButton_->setObjectName( "removeMappingButton" );
    removeMappingButton_->setText( "−" ); // minus sign
    removeMappingButton_->setToolTip( tr( "Remove mapping" ) );
    mappingTools->addWidget( removeMappingButton_ );
    mappingTools->addStretch();
    mappingLayout->addLayout( mappingTools );

    auto* mappingLabel = new QLabel( tr( "&Mappings:" ), this );
    mappingLabel->setBuddy( mappingTable_ );
    form->addRow( mappingLabel, mappings );

    panelLayout->addLayout( form, 1 );

    // ── Connections ──────────────────────────────────────────────────────
    connect( enabledCheck_, &QCheckBox::toggled, this, &RuleDetailPanel::edited );
    for ( auto* field : { keyEdit_, linePatternEdit_, valuePatternEdit_ } ) {
        connect( field, &QLineEdit::textChanged, this, &RuleDetailPanel::edited );
    }
    connect( mappingTable_, &QTableWidget::cellChanged, this, &RuleDetailPanel::edited );
    connect( mappingTable_, &QTableWidget::currentCellChanged, this,
             &RuleDetailPanel::updateMappingButtons );
    connect( addMappingButton_, &QToolButton::clicked, this, &RuleDetailPanel::addMapping );
    connect( removeMappingButton_, &QToolButton::clicked, this, &RuleDetailPanel::removeMapping );

    showNoEntry();
}

void RuleDetailPanel::showEntry( const FooterEntry& entry )
{
    {
        const QSignalBlocker enabledBlocker( enabledCheck_ );
        const QSignalBlocker keyBlocker( keyEdit_ );
        const QSignalBlocker lineBlocker( linePatternEdit_ );
        const QSignalBlocker valueBlocker( valuePatternEdit_ );
        enabledCheck_->setChecked( entry.enabled );
        keyEdit_->setText( entry.key );
        linePatternEdit_->setText( entry.linePattern );
        valuePatternEdit_->setText( entry.valuePattern );
        setMappings( entry.mappings );
    }
    for ( auto* field : { keyEdit_, linePatternEdit_, valuePatternEdit_ } ) {
        revertText_[ field ] = field->text();
    }
    setEnabled( true );
    updateMappingButtons();
}

void RuleDetailPanel::showNoEntry()
{
    showEntry( FooterEntry() );
    setProblems( RuleProblems() );
    setEnabled( false );
}

FooterEntry RuleDetailPanel::entry() const
{
    FooterEntry entry;
    entry.enabled = enabledCheck_->isChecked();
    entry.key = keyEdit_->text();
    entry.linePattern = linePatternEdit_->text();
    entry.valuePattern = valuePatternEdit_->text();
    for ( int row = 0; row < mappingTable_->rowCount(); ++row ) {
        const auto* pattern = mappingTable_->item( row, 0 );
        const auto* display = mappingTable_->item( row, 1 );
        entry.mappings.append(
            { pattern ? pattern->text() : QString(), display ? display->text() : QString() } );
    }
    return entry;
}

void RuleDetailPanel::setProblems( const RuleProblems& problems )
{
    markField( keyEdit_, keyProblem_, problems.key );
    markField( linePatternEdit_, linePatternProblem_, problems.linePattern );
    markField( valuePatternEdit_, valuePatternProblem_, problems.valuePattern );
}

void RuleDetailPanel::focusKey()
{
    keyEdit_->setFocus();
}

void RuleDetailPanel::commitPendingEdit()
{
    // The table has no persistent editors: an editor is an edit in progress.
    const auto current = mappingTable_->currentIndex();
    auto* editor = current.isValid() ? mappingTable_->indexWidget( current ) : nullptr;
    if ( editor ) {
        auto* delegate = mappingTable_->itemDelegateForIndex( current );
        Q_EMIT delegate->commitData( editor );
        Q_EMIT delegate->closeEditor( editor, QAbstractItemDelegate::NoHint );
    }
}

bool RuleDetailPanel::eventFilter( QObject* watched, QEvent* event )
{
    auto* field = qobject_cast<QLineEdit*>( watched );
    if ( !field || !revertText_.contains( field ) ) {
        return QGroupBox::eventFilter( watched, event );
    }

    if ( event->type() == QEvent::FocusIn ) {
        revertText_[ field ] = field->text();
    }
    else if ( event->type() == QEvent::KeyPress ) {
        switch ( static_cast<QKeyEvent*>( event )->key() ) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            revertText_[ field ] = field->text();
            return true;
        case Qt::Key_Escape:
            if ( field->text() != revertText_.value( field ) ) {
                field->setText( revertText_.value( field ) );
            }
            return true;
        default:
            break;
        }
    }
    return QGroupBox::eventFilter( watched, event );
}

void RuleDetailPanel::addMapping()
{
    const int row = mappingTable_->rowCount();
    {
        const QSignalBlocker blocker( mappingTable_ );
        mappingTable_->setRowCount( row + 1 );
        mappingTable_->setItem( row, 0, new QTableWidgetItem );
        mappingTable_->setItem( row, 1, new QTableWidgetItem );
    }
    Q_EMIT edited();

    mappingTable_->setCurrentCell( row, 0 );
    mappingTable_->editItem( mappingTable_->item( row, 0 ) );
}

void RuleDetailPanel::removeMapping()
{
    const int row = mappingTable_->currentRow();
    if ( row < 0 ) {
        return;
    }
    {
        const QSignalBlocker blocker( mappingTable_ );
        mappingTable_->removeRow( row );
    }
    Q_EMIT edited();
    updateMappingButtons();
}

void RuleDetailPanel::updateMappingButtons()
{
    removeMappingButton_->setEnabled( mappingTable_->currentRow() >= 0 );
}

QLineEdit* RuleDetailPanel::addField( QFormLayout* form, const QString& label,
                                      const QString& objectName, QLabel** problemLabel )
{
    auto* field = new QLineEdit( this );
    field->setObjectName( objectName );
    // Not the default 32767 characters, which would cut long patterns.
    field->setMaxLength( std::numeric_limits<int>::max() );
    field->installEventFilter( this );
    revertText_.insert( field, QString() );

    auto* problem = new QLabel( this );
    problem->setObjectName( objectName.chopped( 4 ) + "Problem" ); // keyEdit → keyProblem
    problem->setStyleSheet( QStringLiteral( "color: %1;" ).arg( css( problemColor() ) ) );
    problem->setWordWrap( true );
    problem->setTextInteractionFlags( Qt::TextSelectableByMouse );
    problem->hide();
    *problemLabel = problem;

    auto* column = new QVBoxLayout;
    column->setSpacing( 2 );
    column->addWidget( field );
    column->addWidget( problem );

    auto* buddy = new QLabel( label, this );
    buddy->setBuddy( field );
    form->addRow( buddy, column );
    return field;
}

void RuleDetailPanel::setMappings( const QList<ValueMapping>& mappings )
{
    const QSignalBlocker blocker( mappingTable_ );
    mappingTable_->setRowCount( 0 );
    mappingTable_->setRowCount( static_cast<int>( mappings.size() ) );
    for ( int row = 0; row < mappings.size(); ++row ) {
        mappingTable_->setItem( row, 0, new QTableWidgetItem( mappings[ row ].pattern ) );
        mappingTable_->setItem( row, 1, new QTableWidgetItem( mappings[ row ].displayValue ) );
    }
}

} // namespace custom_footer
