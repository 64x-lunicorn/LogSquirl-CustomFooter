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

#include "footereditor.h"
#include "footerconfig.h"
#include "footerscanner.h"
#include "ruledetailpanel.h"
#include "rulelistmodel.h"
#include "rulelistview.h"
#include "ruletemplatedialog.h"
#include "simplerule.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace custom_footer {

FooterEditor::FooterEditor( const QList<FooterEntry>& entries, QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle( tr( "Custom Footer — Edit Rules" ) );
    setMinimumSize( 800, 520 );

    auto* mainLayout = new QVBoxLayout( this );
    auto* splitter = new QSplitter( Qt::Horizontal, this );
    splitter->setChildrenCollapsible( false );
    mainLayout->addWidget( splitter, 1 );

    // ── Left: the rule list and its buttons ──────────────────────────────
    auto* listSide = new QWidget( splitter );
    auto* listLayout = new QVBoxLayout( listSide );
    listLayout->setContentsMargins( 0, 0, 0, 0 );

    model_ = new RuleListModel( this );
    list_ = new RuleListView( listSide );
    list_->setObjectName( "ruleList" );
    list_->setModel( model_ );
    list_->setSelectionBehavior( QAbstractItemView::SelectRows );
    list_->setSelectionMode( QAbstractItemView::SingleSelection );
    list_->setEditTriggers( QAbstractItemView::NoEditTriggers );
    list_->setWordWrap( false );
    list_->setTextElideMode( Qt::ElideRight );
    list_->setShowGrid( false );
    list_->setAlternatingRowColors( true );
    // Row numbers, as the problems below the list refer to them.
    list_->verticalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
    list_->horizontalHeader()->setSectionResizeMode( RuleListModel::KeyColumn,
                                                     QHeaderView::Interactive );
    list_->horizontalHeader()->setSectionResizeMode( RuleListModel::LinePatternColumn,
                                                     QHeaderView::Stretch );
    list_->horizontalHeader()->resizeSection( RuleListModel::KeyColumn, 150 );
    listLayout->addWidget( list_, 1 );

    auto* toolLayout = new QHBoxLayout;

    addButton_ = new QToolButton( listSide );
    addButton_->setObjectName( "addRuleButton" );
    addButton_->setText( "+" );
    addButton_->setToolTip( tr( "Add rule" ) );
    toolLayout->addWidget( addButton_ );

    templateButton_ = new QToolButton( listSide );
    templateButton_->setObjectName( "templateButton" );
    templateButton_->setText( tr( "From template…" ) );
    templateButton_->setToolTip(
        tr( "Add a ready-made rule, e.g. for a version or an IP address" ) );
    toolLayout->addWidget( templateButton_ );

    removeButton_ = new QToolButton( listSide );
    removeButton_->setObjectName( "removeRuleButton" );
    removeButton_->setText( "−" ); // minus sign
    removeButton_->setToolTip( tr( "Remove rule" ) );
    toolLayout->addWidget( removeButton_ );

    upButton_ = new QToolButton( listSide );
    upButton_->setObjectName( "moveUpButton" );
    upButton_->setText( "↑" ); // up arrow
    upButton_->setToolTip( tr( "Move up (%1), or drag the rule" )
                               .arg( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_Up )
                                         .toString( QKeySequence::NativeText ) ) );
    toolLayout->addWidget( upButton_ );

    downButton_ = new QToolButton( listSide );
    downButton_->setObjectName( "moveDownButton" );
    downButton_->setText( "↓" ); // down arrow
    downButton_->setToolTip( tr( "Move down (%1), or drag the rule" )
                                 .arg( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_Down )
                                           .toString( QKeySequence::NativeText ) ) );
    toolLayout->addWidget( downButton_ );

    toolLayout->addStretch();

    importButton_ = new QToolButton( listSide );
    importButton_->setText( tr( "Import" ) );
    importButton_->setToolTip( tr( "Import rules from a JSON file" ) );
    toolLayout->addWidget( importButton_ );

    exportButton_ = new QToolButton( listSide );
    exportButton_->setText( tr( "Export" ) );
    exportButton_->setToolTip( tr( "Export rules to a JSON file" ) );
    toolLayout->addWidget( exportButton_ );

    listLayout->addLayout( toolLayout );

    // ── Right: the selected rule ─────────────────────────────────────────
    panel_ = new RuleDetailPanel( splitter );

    splitter->setStretchFactor( 0, 2 );
    splitter->setStretchFactor( 1, 3 );

    // ── Problems + Button Box ────────────────────────────────────────────
    problemLabel_ = new QLabel( this );
    problemLabel_->setObjectName( "problemLabel" );
    problemLabel_->setWordWrap( true );
    problemLabel_->hide();
    mainLayout->addWidget( problemLabel_ );

    buttonBox_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this );
    mainLayout->addWidget( buttonBox_ );

    // ── Connections ──────────────────────────────────────────────────────
    connect( addButton_, &QToolButton::clicked, this, &FooterEditor::addEntry );
    connect( templateButton_, &QToolButton::clicked, this, &FooterEditor::addFromTemplate );
    connect( removeButton_, &QToolButton::clicked, this, &FooterEditor::removeEntry );
    connect( upButton_, &QToolButton::clicked, this, &FooterEditor::moveEntryUp );
    connect( downButton_, &QToolButton::clicked, this, &FooterEditor::moveEntryDown );
    connect( list_, &RuleListView::moveUpRequested, this, &FooterEditor::moveEntryUp );
    connect( list_, &RuleListView::moveDownRequested, this, &FooterEditor::moveEntryDown );
    connect( importButton_, &QToolButton::clicked, this, &FooterEditor::importRules );
    connect( exportButton_, &QToolButton::clicked, this, &FooterEditor::exportRules );

    connect( list_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
             &FooterEditor::showCurrentRule );
    connect( panel_, &RuleDetailPanel::edited, this, &FooterEditor::storePanelInCurrentRule );

    // Validation marks and row numbers follow every change of the rules.
    // Moved rules keep their marks; validating again only renumbers them.
    connect( model_, &RuleListModel::ruleChanged, this, &FooterEditor::ruleChanged );
    connect( model_, &RuleListModel::rowsInserted, this,
             [ this ]( const QModelIndex&, int first, int last ) { validateRows( first, last ); } );
    connect( model_, &RuleListModel::rowsRemoved, this, &FooterEditor::listProblems );
    connect( model_, &RuleListModel::rowsMoved, this, &FooterEditor::listProblems );
    connect( model_, &RuleListModel::modelReset, this,
             [ this ] { validateRows( 0, model_->rowCount() - 1 ); } );
    connect( model_, &RuleListModel::rowsInserted, this, &FooterEditor::updateButtons );
    connect( model_, &RuleListModel::rowsRemoved, this, &FooterEditor::updateButtons );
    connect( model_, &RuleListModel::rowsMoved, this, &FooterEditor::updateButtons );
    // A dragged rule takes a mapping still being typed with it. The
    // selection, and so the panel, follows the dragged rule by itself.
    connect( model_, &RuleListModel::aboutToDropRules, panel_,
             &RuleDetailPanel::commitPendingEdit );

    // A mapping still being typed belongs to the rules that are saved.
    connect( buttonBox_, &QDialogButtonBox::accepted, this, [ this ] {
        panel_->commitPendingEdit();
        accept();
    } );
    connect( buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject );
    connect( buttonBox_->button( QDialogButtonBox::Apply ), &QPushButton::clicked, this, [ this ] {
        panel_->commitPendingEdit();
        Q_EMIT applied();
    } );

    // ── Populate ─────────────────────────────────────────────────────────
    model_->setEntries( entries );
    if ( model_->rowCount() > 0 ) {
        selectRow( 0 );
    }
    showCurrentRule();
}

// ── Public accessors ─────────────────────────────────────────────────────

QList<FooterEntry> FooterEditor::entries() const
{
    return model_->entries();
}

void FooterEditor::appendEntries( const QList<FooterEntry>& entries )
{
    model_->appendEntries( entries );
    if ( currentRow() < 0 && model_->rowCount() > 0 ) {
        selectRow( 0 );
    }
}

// ── Slots ────────────────────────────────────────────────────────────────

void FooterEditor::addEntry()
{
    panel_->commitPendingEdit();
    appendAndSelect( FooterEntry() );
}

void FooterEditor::addFromTemplate()
{
    panel_->commitPendingEdit();
    if ( !templateDialog_ ) {
        templateDialog_ = new RuleTemplateDialog( this );
        connect( templateDialog_, &QDialog::accepted, this, &FooterEditor::addTemplateRule );
    }
    // Only rules the scanner uses make the new one an alternative.
    QStringList keys;
    for ( const auto& entry : model_->entries() ) {
        if ( entry.enabled && !entry.linePattern.isEmpty() ) {
            keys.append( entry.key );
        }
    }
    templateDialog_->reset( keys );
    // Not exec(): the dialog stays window-modal, and tests can drive it.
    templateDialog_->open();
}

void FooterEditor::addTemplateRule()
{
    const auto entry = templateDialog_->entry();
    if ( entry.key.isEmpty() ) {
        return;
    }
    appendAndSelect( entry );
}

void FooterEditor::removeEntry()
{
    panel_->commitPendingEdit();
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }

    // Select the next rule, or the one above the last, before removing the
    // rule, so that the panel shows the right rule once rather than
    // whichever the selection model makes current.
    const int count = model_->rowCount();
    if ( count > 1 ) {
        selectRow( row + 1 < count ? row + 1 : row - 1 );
    }
    else {
        list_->selectionModel()->clear();
    }
    model_->removeRows( row, 1 );
}

void FooterEditor::moveEntryUp()
{
    panel_->commitPendingEdit();
    const int row = currentRow();
    if ( row > 0 ) {
        moveEntry( row, row - 1 );
    }
}

void FooterEditor::moveEntryDown()
{
    panel_->commitPendingEdit();
    const int row = currentRow();
    if ( row >= 0 && row < model_->rowCount() - 1 ) {
        moveEntry( row, row + 1 );
    }
}

void FooterEditor::updateButtons()
{
    const int row = currentRow();
    const int count = model_->rowCount();
    removeButton_->setEnabled( row >= 0 );
    upButton_->setEnabled( row > 0 );
    downButton_->setEnabled( row >= 0 && row < count - 1 );
}

void FooterEditor::importRules()
{
    panel_->commitPendingEdit();
    const auto filePath = QFileDialog::getOpenFileName(
        this, tr( "Import Rules" ), QString(), tr( "JSON Files (*.json);;All Files (*)" ) );
    if ( filePath.isEmpty() ) {
        return;
    }

    QString error;
    const auto imported = FooterConfig::importFromJson( filePath, &error );
    if ( imported.isEmpty() && !error.isEmpty() ) {
        QMessageBox::warning( this, tr( "Import Error" ), error );
        return;
    }

    appendEntries( imported );
}

void FooterEditor::exportRules()
{
    panel_->commitPendingEdit();
    const auto filePath = QFileDialog::getSaveFileName(
        this, tr( "Export Rules" ), QStringLiteral( "footer_rules.json" ),
        tr( "JSON Files (*.json);;All Files (*)" ) );
    if ( filePath.isEmpty() ) {
        return;
    }

    if ( !FooterConfig::exportToJson( filePath, entries() ) ) {
        QMessageBox::warning( this, tr( "Export Error" ),
                              tr( "Could not write to file: %1" ).arg( filePath ) );
    }
}

void FooterEditor::showCurrentRule()
{
    const int row = currentRow();
    if ( row < 0 ) {
        panel_->showNoEntry();
    }
    else {
        panel_->showEntry( model_->entry( row ), model_->unfinishedSimpleRule( row ) );
        panel_->setProblems( model_->problems( row ) );
    }
    updateButtons();
}

void FooterEditor::storePanelInCurrentRule()
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    model_->setEntry( row, panel_->entry() );
    model_->setUnfinishedSimpleRule( row, panel_->unfinishedSimpleRule() );
    validateRow( row );
    showProblems();
}

void FooterEditor::ruleChanged( int row )
{
    // Enabled or disabled in the list. Showing the whole rule again would
    // put it back into simple mode after a switch to advanced.
    if ( row == currentRow() ) {
        panel_->showEnabled( model_->entry( row ).enabled );
    }
    validateRow( row );
    showProblems();
}

// ── Private helpers ──────────────────────────────────────────────────────

int FooterEditor::currentRow() const
{
    const auto current = list_->selectionModel()->currentIndex();
    return current.isValid() ? current.row() : -1;
}

void FooterEditor::selectRow( int row )
{
    const auto index = model_->index( row, RuleListModel::KeyColumn );
    list_->selectionModel()->setCurrentIndex( index, QItemSelectionModel::ClearAndSelect
                                                         | QItemSelectionModel::Rows );
    list_->scrollTo( index );
}

void FooterEditor::appendAndSelect( const FooterEntry& entry )
{
    const int row = model_->rowCount();
    model_->appendEntries( { entry } );
    selectRow( row );
    panel_->focusKey();
}

void FooterEditor::moveEntry( int from, int to )
{
    // The selection moves with the rule, and the panel keeps showing it.
    model_->moveRule( from, to );
    list_->scrollTo( model_->index( to, RuleListModel::KeyColumn ) );
}

void FooterEditor::validateRow( int row )
{
    ++ruleValidations_;
    const auto& entry = model_->entry( row );

    // Rules sharing a key are alternatives. A rule without a line pattern
    // is incomplete and ignored, but one with a pattern needs a key.
    RuleProblems problems;
    if ( entry.enabled && !entry.linePattern.isEmpty() && entry.key.trimmed().isEmpty() ) {
        problems.key = tr( "a rule with a line pattern needs a key" );
    }
    const auto patternProblem = [ this ]( const QString& pattern, const QString& what ) {
        const auto error = patternError( pattern );
        return error.isEmpty() ? QString() : tr( "%1: %2" ).arg( what, error );
    };
    // A simple rule without a usable end character has no patterns yet.
    if ( const auto unfinished = model_->unfinishedSimpleRule( row ) ) {
        problems.endCharacter = endCharacterProblem( *unfinished );
    }
    problems.linePattern = patternProblem( entry.linePattern, tr( "invalid line pattern" ) );
    problems.valuePattern = patternProblem( entry.valuePattern, tr( "invalid value pattern" ) );
    model_->setProblems( row, problems );

    QStringList lines;
    for ( const auto& problem :
          { problems.key, problems.endCharacter, problems.linePattern, problems.valuePattern } ) {
        if ( !problem.isEmpty() ) {
            lines.append( tr( "Rule %1: %2" ).arg( row + 1 ).arg( problem ) );
        }
    }
    if ( lines.isEmpty() ) {
        problemLines_.remove( row );
    }
    else {
        problemLines_.insert( row, lines );
    }
}

void FooterEditor::validateRows( int first, int last )
{
    for ( int row = first; row <= last; ++row ) {
        validateRow( row );
    }
    // Rows after the new ones moved down.
    listProblems();
}

void FooterEditor::listProblems()
{
    problemLines_.clear();
    for ( int row = 0; row < model_->rowCount(); ++row ) {
        const auto problems = model_->problems( row );
        if ( problems.isEmpty() ) {
            continue;
        }
        QStringList lines;
        for ( const auto& problem : { problems.key, problems.endCharacter, problems.linePattern,
                                      problems.valuePattern } ) {
            if ( !problem.isEmpty() ) {
                lines.append( tr( "Rule %1: %2" ).arg( row + 1 ).arg( problem ) );
            }
        }
        problemLines_.insert( row, lines );
    }
    showProblems();
}

void FooterEditor::showProblems()
{
    QStringList lines;
    for ( const auto& ruleLines : std::as_const( problemLines_ ) ) {
        lines += ruleLines;
    }
    const bool valid = problemLines_.isEmpty();

    panel_->setProblems( model_->problems( currentRow() ) );
    problemLabel_->setText( lines.join( '\n' ) );
    problemLabel_->setVisible( !valid );
    buttonBox_->button( QDialogButtonBox::Ok )->setEnabled( valid );
    buttonBox_->button( QDialogButtonBox::Apply )->setEnabled( valid );
}

QString FooterEditor::patternError( const QString& pattern )
{
    auto known = patternErrors_.constFind( pattern );
    if ( known == patternErrors_.constEnd() ) {
        ++patternCompilations_;
        known = patternErrors_.insert( pattern, FooterScanner::patternError( pattern ) );
    }
    return known.value();
}

} // namespace custom_footer
