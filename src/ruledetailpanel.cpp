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
#include <QComboBox>
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
#include <utility>

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

    // ── Simple mode: the text before the value, and where it ends ────────
    textBeforeEdit_ = new QLineEdit( this );
    textBeforeEdit_->setObjectName( "textBeforeEdit" );
    textBeforeEdit_->setMaxLength( std::numeric_limits<int>::max() );
    textBeforeEdit_->setPlaceholderText( tr( "Matched literally, e.g. VIN:" ) );
    watchField( textBeforeEdit_ );
    auto* textBeforeLabel = new QLabel( tr( "&Text before the value:" ), this );
    textBeforeLabel->setBuddy( textBeforeEdit_ );
    form->addRow( textBeforeLabel, textBeforeEdit_ );

    auto* valueEnd = new QWidget( this );
    auto* valueEndLayout = new QHBoxLayout( valueEnd );
    valueEndLayout->setContentsMargins( 0, 0, 0, 0 );
    valueEndCombo_ = new QComboBox( valueEnd );
    valueEndCombo_->setObjectName( "valueEndCombo" );
    valueEndCombo_->addItem( tr( "Whitespace" ), static_cast<int>( ValueEnd::Whitespace ) );
    valueEndCombo_->addItem( tr( "End of line" ), static_cast<int>( ValueEnd::EndOfLine ) );
    valueEndCombo_->addItem( tr( "Character" ), static_cast<int>( ValueEnd::Character ) );
    valueEndLayout->addWidget( valueEndCombo_ );
    endCharacterEdit_ = new QLineEdit( valueEnd );
    endCharacterEdit_->setObjectName( "endCharacterEdit" );
    endCharacterEdit_->setMaxLength( 1 );
    endCharacterEdit_->setPlaceholderText( QStringLiteral( "," ) );
    endCharacterEdit_->setToolTip(
        tr( "The character before which the value ends, e.g. a comma or a semicolon" ) );
    endCharacterEdit_->setMaximumWidth( 4 * fontMetrics().horizontalAdvance( QLatin1Char( 'M' ) ) );
    watchField( endCharacterEdit_ );
    valueEndLayout->addWidget( endCharacterEdit_ );
    valueEndLayout->addStretch();
    auto* valueEndLabel = new QLabel( tr( "Value &ends at:" ), this );
    valueEndLabel->setBuddy( valueEndCombo_ );
    form->addRow( valueEndLabel, valueEnd );
    simpleRows_ = { textBeforeLabel, textBeforeEdit_, valueEndLabel, valueEnd };

    // ── Advanced mode: the patterns themselves ───────────────────────────
    advancedCheck_ = new QCheckBox( tr( "&Advanced: edit the patterns" ), this );
    advancedCheck_->setObjectName( "advancedCheck" );
    form->addRow( QString(), advancedCheck_ );

    linePatternEdit_
        = addField( form, tr( "&Line pattern:" ), "linePatternEdit", &linePatternProblem_ );
    valuePatternEdit_
        = addField( form, tr( "&Value pattern:" ), "valuePatternEdit", &valuePatternProblem_ );

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
    connect( keyEdit_, &QLineEdit::textChanged, this, &RuleDetailPanel::edited );
    for ( auto* field : { textBeforeEdit_, endCharacterEdit_ } ) {
        connect( field, &QLineEdit::textChanged, this, &RuleDetailPanel::simpleFieldEdited );
    }
    connect( valueEndCombo_, &QComboBox::currentIndexChanged, this,
             &RuleDetailPanel::valueEndChosen );
    connect( advancedCheck_, &QCheckBox::toggled, this, &RuleDetailPanel::advancedToggled );
    for ( auto* field : { linePatternEdit_, valuePatternEdit_ } ) {
        connect( field, &QLineEdit::textChanged, this, &RuleDetailPanel::patternEdited );
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
    const auto simple = simpleRuleOf( entry );
    setSimpleFields( simple.value_or( SimpleRule() ) );
    setAdvanced( !simple );
    for ( auto field = revertText_.begin(); field != revertText_.end(); ++field ) {
        field.value() = field.key()->text();
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

void RuleDetailPanel::showEnabled( bool enabled )
{
    const QSignalBlocker blocker( enabledCheck_ );
    enabledCheck_->setChecked( enabled );
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

void RuleDetailPanel::advancedToggled( bool advanced )
{
    if ( advanced ) {
        // The patterns stay as they are, now editable.
        setAdvanced( true );
        return;
    }

    const auto linePattern = linePatternEdit_->text();
    const auto valuePattern = valuePatternEdit_->text();
    const auto simple = simpleRuleOf( linePattern, valuePattern );
    if ( !simple ) {
        // Not offered then; the check box is disabled.
        setAdvanced( true );
        return;
    }
    // Keep the simple fields if they still make these patterns, e.g. a text
    // whose end character is missing, which makes no pattern at all.
    const bool fieldsMatch
        = valuePattern.isEmpty() && simpleLinePattern( simpleRule() ) == linePattern;
    if ( !fieldsMatch ) {
        setSimpleFields( *simple );
    }
    setAdvanced( false );
}

void RuleDetailPanel::simpleFieldEdited()
{
    if ( advanced_ ) {
        return;
    }
    FooterEntry generated;
    applySimpleRule( simpleRule(), generated );
    {
        const QSignalBlocker lineBlocker( linePatternEdit_ );
        const QSignalBlocker valueBlocker( valuePatternEdit_ );
        linePatternEdit_->setText( generated.linePattern );
        valuePatternEdit_->setText( generated.valuePattern );
    }
    Q_EMIT edited();
}

void RuleDetailPanel::valueEndChosen()
{
    const bool atCharacter = simpleRule().valueEnd == ValueEnd::Character;
    endCharacterEdit_->setEnabled( atCharacter );
    if ( atCharacter && endCharacterEdit_->text().isEmpty() ) {
        const QSignalBlocker blocker( endCharacterEdit_ );
        endCharacterEdit_->setText( QStringLiteral( "," ) );
    }
    simpleFieldEdited();
}

void RuleDetailPanel::patternEdited()
{
    updateAdvancedCheck();
    Q_EMIT edited();
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
    watchField( field );

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

void RuleDetailPanel::watchField( QLineEdit* field )
{
    field->installEventFilter( this );
    revertText_.insert( field, QString() );
}

void RuleDetailPanel::setSimpleFields( const SimpleRule& rule )
{
    const QSignalBlocker textBlocker( textBeforeEdit_ );
    const QSignalBlocker endBlocker( valueEndCombo_ );
    const QSignalBlocker characterBlocker( endCharacterEdit_ );
    textBeforeEdit_->setText( rule.textBefore );
    valueEndCombo_->setCurrentIndex(
        valueEndCombo_->findData( static_cast<int>( rule.valueEnd ) ) );
    endCharacterEdit_->setText( rule.endCharacter.isNull() ? QString()
                                                           : QString( rule.endCharacter ) );
    endCharacterEdit_->setEnabled( rule.valueEnd == ValueEnd::Character );
}

SimpleRule RuleDetailPanel::simpleRule() const
{
    SimpleRule rule;
    rule.textBefore = textBeforeEdit_->text();
    rule.valueEnd = static_cast<ValueEnd>( valueEndCombo_->currentData().toInt() );
    const auto character = endCharacterEdit_->text();
    rule.endCharacter = character.isEmpty() ? QChar() : character.at( 0 );
    return rule;
}

void RuleDetailPanel::setAdvanced( bool advanced )
{
    advanced_ = advanced;
    {
        const QSignalBlocker blocker( advancedCheck_ );
        advancedCheck_->setChecked( advanced );
    }
    for ( auto* widget : std::as_const( simpleRows_ ) ) {
        widget->setVisible( !advanced );
    }
    linePatternEdit_->setReadOnly( !advanced );
    valuePatternEdit_->setReadOnly( !advanced );
    if ( advanced ) {
        linePatternEdit_->setPlaceholderText( tr( "Regex finding the line, e.g. VIN:\\s+(\\S+)" ) );
        valuePatternEdit_->setPlaceholderText(
            tr( "Optional regex for the value in the line, e.g. :\\s+(\\S+)$" ) );
    }
    else {
        linePatternEdit_->setPlaceholderText( tr( "Made from the text before the value" ) );
        valuePatternEdit_->setPlaceholderText( tr( "Not needed: the line pattern has the value" ) );
    }
    updateAdvancedCheck();
}

void RuleDetailPanel::updateAdvancedCheck()
{
    const bool canSwitch
        = !advanced_ || simpleRuleOf( linePatternEdit_->text(), valuePatternEdit_->text() );
    advancedCheck_->setEnabled( canSwitch );
    advancedCheck_->setToolTip(
        canSwitch ? tr( "Edit the line and value patterns as regular expressions" )
                  : tr( "Simple mode needs a line pattern made from a text before the value, "
                        "and no value pattern" ) );
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
