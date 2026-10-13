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
#include "ruledetailpanel.h"
#include "rulefromline.h"
#include "rulelistmodel.h"
#include "rulelistview.h"
#include "rulepreviewer.h"
#include "rulepreviewview.h"
#include "ruletemplatedialog.h"
#include "selectedlinepicker.h"
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

namespace custom_footer {

namespace {

/// The inline option that makes a whole pattern ignore case, the only way
/// a rule says so.
const QString ignoreCase = QStringLiteral( "(?i)" );

/// How long the pattern's leading start-of-pattern items are, such as
/// (*CRLF) or (*UTF): PCRE takes them only before everything else, (?i)
/// included.
qsizetype startItemsLength( const QString& pattern )
{
    qsizetype length = 0;
    while ( pattern.mid( length ).startsWith( QStringLiteral( "(*" ) ) ) {
        const auto end = pattern.indexOf( QLatin1Char( ')' ), length );
        if ( end < 0 ) {
            break;
        }
        length = end + 1;
    }
    return length;
}

/// A rule's pattern as the Regex Lab tests it: (?i) at its start, after
/// any start-of-pattern items, becomes the Lab's Match case flag, as long
/// as more of the pattern follows it.
RegexLabPattern toRegexLab( const QString& pattern )
{
    const auto start = startItemsLength( pattern );
    const auto rest = pattern.mid( start );
    if ( rest.startsWith( ignoreCase ) && rest.size() > ignoreCase.size() ) {
        return { pattern.left( start ) + rest.mid( ignoreCase.size() ), false };
    }
    return { pattern, true };
}

/// An applied pattern as a rule's pattern: without Match case, (?i) at its
/// start, after any start-of-pattern items, unless it is there already or
/// nothing would follow it.
QString fromRegexLab( const RegexLabPattern& applied )
{
    const auto start = startItemsLength( applied.pattern );
    const auto rest = applied.pattern.mid( start );
    if ( applied.matchesCase || rest.isEmpty() || rest.startsWith( ignoreCase ) ) {
        return applied.pattern;
    }
    return applied.pattern.left( start ) + ignoreCase + rest;
}

/// The pattern of @p entry in @p field.
QString& patternOf( FooterEntry& entry, PatternField field )
{
    return field == PatternField::Line ? entry.linePattern : entry.valuePattern;
}

const QString& patternOf( const FooterEntry& entry, PatternField field )
{
    return field == PatternField::Line ? entry.linePattern : entry.valuePattern;
}

} // namespace

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

    ruleFromLineButton_ = new QToolButton( listSide );
    ruleFromLineButton_->setObjectName( "ruleFromLineButton" );
    ruleFromLineButton_->setText( tr( "From selected line…" ) );
    ruleFromLineButton_->setToolTip(
        tr( "Add a rule made from the Log Line selected in LogSquirl: click or mark its value" ) );
    // Only where the host can hand over the selected line.
    ruleFromLineButton_->hide();
    toolLayout->addWidget( ruleFromLineButton_ );

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
    importButton_->setObjectName( "importButton" );
    importButton_->setText( tr( "Import" ) );
    importButton_->setToolTip( tr( "Import rules from a JSON file" ) );
    toolLayout->addWidget( importButton_ );

    exportButton_ = new QToolButton( listSide );
    exportButton_->setObjectName( "exportButton" );
    exportButton_->setText( tr( "Export" ) );
    exportButton_->setToolTip( tr( "Export rules to a JSON file" ) );
    toolLayout->addWidget( exportButton_ );

    listLayout->addLayout( toolLayout );

    // ── Right: the selected rule, below the line it was made from ────────
    auto* ruleSide = new QWidget( splitter );
    auto* ruleLayout = new QVBoxLayout( ruleSide );
    ruleLayout->setContentsMargins( 0, 0, 0, 0 );
    linePicker_ = new SelectedLinePicker( ruleSide );
    linePicker_->hide();
    ruleLayout->addWidget( linePicker_ );
    panel_ = new RuleDetailPanel( ruleSide );
    ruleLayout->addWidget( panel_, 1 );

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
    connect( ruleFromLineButton_, &QToolButton::clicked, this, [ this ] {
        panel_->commitPendingEdit();
        Q_EMIT ruleFromLineRequested();
    } );
    connect( linePicker_, &SelectedLinePicker::marked, this, &FooterEditor::lineMarked );
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
    connect( panel_, &RuleDetailPanel::patternTestRequested, this, &FooterEditor::testPattern );

    // The model's rule check is up to date with every signal of the model;
    // after a change of the rows, only the listing is shown again, with the
    // new rule numbers. Moved rules keep their marks.
    connect( model_, &RuleListModel::ruleChanged, this, &FooterEditor::ruleChanged );
    connect( model_, &RuleListModel::rowsInserted, this, &FooterEditor::showProblems );
    connect( model_, &RuleListModel::rowsRemoved, this, &FooterEditor::showProblems );
    connect( model_, &RuleListModel::rowsMoved, this, &FooterEditor::showProblems );
    connect( model_, &RuleListModel::modelReset, this, &FooterEditor::showProblems );
    connect( model_, &RuleListModel::rowsInserted, this, &FooterEditor::updateButtons );
    connect( model_, &RuleListModel::rowsRemoved, this, &FooterEditor::updateButtons );
    connect( model_, &RuleListModel::rowsMoved, this, &FooterEditor::updateButtons );
    // A dragged rule takes a mapping still being typed with it. The
    // selection, and so the panel, follows the dragged rule by itself.
    connect( model_, &RuleListModel::aboutToDropRules, panel_,
             &RuleDetailPanel::commitPendingEdit );

    // ── The live preview (#16) ───────────────────────────────────────────
    // Any change of the rules may change what the selected rule finds, or
    // which rule of its key supplies the value.
    previewer_ = new RulePreviewer( this );
    connect( previewer_, &RulePreviewer::updating, panel_->previewView(),
             &RulePreviewView::showUpdating );
    connect( previewer_, &RulePreviewer::previewed, this, &FooterEditor::showLineRulePreview );
    connect( previewer_, &RulePreviewer::previewed, panel_->previewView(),
             &RulePreviewView::showPreview );
    // Edits schedule it where they are stored, once per edit; not on every
    // dataChanged(), which validation marks emit too.
    connect( model_, &RuleListModel::rowsInserted, this, [ this ] { schedulePreview(); } );
    connect( model_, &RuleListModel::rowsRemoved, this, [ this ] { schedulePreview(); } );
    connect( model_, &RuleListModel::rowsMoved, this, [ this ] { schedulePreview(); } );
    connect( model_, &RuleListModel::modelReset, this, [ this ] { schedulePreview(); } );

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

    updatePatternTest();

    // ── Populate ─────────────────────────────────────────────────────────
    model_->setEntries( entries );
    if ( model_->rowCount() > 0 ) {
        selectRow( 0 );
    }
    showCurrentRule();
}

FooterEditor::~FooterEditor()
{
    // While the dialog and its children are torn down, a mapping cell being
    // edited would commit as it loses the focus, and reach this editor half
    // destroyed. Commit it now, while everything is whole, then cut the
    // children off from this editor before any of them dies.
    closing_ = true;
    panel_->commitPendingEdit();
    disconnect( panel_, nullptr, this, nullptr );
    disconnect( model_, nullptr, this, nullptr );
    disconnect( list_, nullptr, this, nullptr );
    disconnect( list_->selectionModel(), nullptr, this, nullptr );
    disconnect( linePicker_, nullptr, this, nullptr );
    if ( templateDialog_ ) {
        disconnect( templateDialog_, nullptr, this, nullptr );
    }
    // No preview may run on while, or after, the widgets are deleted.
    previewer_->stop();
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

void FooterEditor::offerRuleFromLine( bool offered )
{
    ruleFromLineButton_->setVisible( offered );
}

void FooterEditor::addRuleFromLine( const QString& line, int selectedLines )
{
    panel_->commitPendingEdit();
    linePicker_->showLine( line, selectedLines );
    lineKey_.clear();
    const int row = model_->rowCount();
    model_->appendEntries( { FooterEntry() } );
    // Known before the rule is selected, which shows the line.
    lineRule_ = model_->index( row, RuleListModel::KeyColumn );
    selectRow( row );
    linePicker_->focusLine();
}

void FooterEditor::setActiveFile( const QString& filePath )
{
    previewer_->setActiveFile( filePath );
}

QString FooterEditor::activeFile() const
{
    return previewer_->activeFile();
}

void FooterEditor::setMaxLines( int maxLines )
{
    previewer_->setMaxLines( maxLines );
}

void FooterEditor::setRegexLab( OpenRegexLab openRegexLab )
{
    openRegexLab_ = std::move( openRegexLab );
    updatePatternTest();
}

void FooterEditor::done( int result )
{
    // Closing may still commit a mapping cell being edited, as its editor
    // loses the focus; closing_ keeps that from starting a preview.
    closing_ = true;
    previewer_->stop();
    QDialog::done( result );
}

// ── Slots ────────────────────────────────────────────────────────────────

void FooterEditor::addEntry()
{
    panel_->commitPendingEdit();
    appendAndSelect( { FooterEntry() } );
}

void FooterEditor::addFromTemplate()
{
    panel_->commitPendingEdit();
    if ( !templateDialog_ ) {
        templateDialog_ = new RuleTemplateDialog( this );
        connect( templateDialog_, &QDialog::accepted, this, &FooterEditor::addTemplateRules );
    }
    // Only rules the scanner uses make the new one an alternative.
    templateDialog_->reset( model_->check().keysInUse() );
    // Not exec(): the dialog stays window-modal, and tests can drive it.
    templateDialog_->open();
}

void FooterEditor::addTemplateRules()
{
    // In the order the dialog lists them. OK is disabled while a checked
    // template lacks its key; a rule without one is never added anyway.
    QList<FooterEntry> entries;
    for ( const auto& entry : templateDialog_->entries() ) {
        if ( !entry.key.isEmpty() ) {
            entries.append( entry );
        }
    }
    if ( !entries.isEmpty() ) {
        appendAndSelect( entries );
    }
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

// The file dialogs and messages are opened with open(), never run in a
// nested event loop of their own: the plugin may be shut down while they
// are open, and deleting the editor deletes them.

void FooterEditor::importRules()
{
    panel_->commitPendingEdit();
    auto* dialog = new QFileDialog( this, tr( "Import Rules" ), QString(),
                                    tr( "JSON Files (*.json);;All Files (*)" ) );
    dialog->setObjectName( "importDialog" );
    dialog->setAcceptMode( QFileDialog::AcceptOpen );
    dialog->setFileMode( QFileDialog::ExistingFile );
    dialog->setAttribute( Qt::WA_DeleteOnClose );
    connect( dialog, &QFileDialog::fileSelected, this, &FooterEditor::importFrom );
    dialog->open();
}

void FooterEditor::importFrom( const QString& filePath )
{
    QString error;
    const auto imported = FooterConfig::importFromJson( filePath, &error );
    if ( imported.isEmpty() && !error.isEmpty() ) {
        showError( tr( "Import Error" ), error, "importErrorBox" );
        return;
    }

    appendEntries( imported );
}

void FooterEditor::exportRules()
{
    panel_->commitPendingEdit();
    auto* dialog = new QFileDialog( this, tr( "Export Rules" ), QString(),
                                    tr( "JSON Files (*.json);;All Files (*)" ) );
    dialog->setObjectName( "exportDialog" );
    dialog->setAcceptMode( QFileDialog::AcceptSave );
    dialog->selectFile( QStringLiteral( "footer_rules.json" ) );
    dialog->setAttribute( Qt::WA_DeleteOnClose );
    connect( dialog, &QFileDialog::fileSelected, this, &FooterEditor::exportTo );
    dialog->open();
}

void FooterEditor::exportTo( const QString& filePath )
{
    if ( !FooterConfig::exportToJson( filePath, entries() ) ) {
        showError( tr( "Export Error" ), tr( "Could not write to file: %1" ).arg( filePath ),
                   "exportErrorBox" );
    }
}

void FooterEditor::showError( const QString& title, const QString& text, const QString& objectName )
{
    auto* box = new QMessageBox( QMessageBox::Warning, title, text, QMessageBox::Ok, this );
    box->setObjectName( objectName );
    box->setAttribute( Qt::WA_DeleteOnClose );
    box->open();
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
    linePicker_->setVisible( isLineRule( row ) );

    // Another rule: no need to wait for typing to pause. Coalesced with the
    // change that follows, e.g. the removal of the rule selected before.
    schedulePreview( RulePreviewer::Start::Soon );
}

void FooterEditor::storePanelInCurrentRule()
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    const auto before = model_->entry( row );
    const auto edited = panel_->entry();
    model_->setEntry( row, edited, panel_->unfinishedSimpleRule() );
    if ( isLineRule( row )
         && ( edited.linePattern != before.linePattern
              || edited.valuePattern != before.valuePattern ) ) {
        // The hint about the last mark no longer describes the rule.
        linePicker_->showRuleEdited();
    }
    showProblems();
    schedulePreview();
}

void FooterEditor::ruleChanged( int row )
{
    // Enabled or disabled in the list. Showing the whole rule again would
    // put it back into simple mode after a switch to advanced.
    if ( row == currentRow() ) {
        panel_->showEnabled( model_->entry( row ).enabled );
    }
    showProblems();
    // Another rule of the key may supply its value now.
    schedulePreview();
}

void FooterEditor::testPattern( PatternField field )
{
    panel_->commitPendingEdit();
    const int row = currentRow();
    if ( row < 0 || !openRegexLab_ || regexLabOpen_ || closing_ ) {
        return;
    }
    const auto pattern = toRegexLab( patternOf( model_->entry( row ), field ) );
    // Follows the rule when rules move, and is invalid once it is removed.
    const QPersistentModelIndex rule( model_->index( row, RuleListModel::KeyColumn ) );

    setRegexLabOpen( true );
    // The host never answers once this editor is destroyed.
    const bool opened = openRegexLab_(
        pattern, this,
        [ this, rule, field, pattern ]( const std::optional<RegexLabPattern>& applied ) {
            regexLabAnswered( rule, field, pattern, applied );
        } );
    if ( !opened ) {
        setRegexLabOpen( false );
    }
}

void FooterEditor::lineMarked( qsizetype start, qsizetype length )
{
    const int row = currentRow();
    if ( !isLineRule( row ) ) {
        return;
    }
    if ( regexLabOpen_ ) {
        // Its answer may write a pattern into this very rule: the rule is
        // left to it, as Test… is.
        linePicker_->showMarkIgnored(
            tr( "The Regex Lab is open: apply or cancel it there first, then click or mark the "
                "value again." ) );
        return;
    }
    const auto made = ruleFromLine( linePicker_->line(), start, length );
    linePicker_->showRule( made );
    if ( !made.isRule() ) {
        // The rule stays as the last mark made it.
        return;
    }
    panel_->commitPendingEdit();
    auto entry = model_->entry( row );
    if ( entry.key.isEmpty() || entry.key == lineKey_ ) {
        entry.key = made.key;
    }
    lineKey_ = made.key;
    applySimpleRule( made.rule, entry );
    model_->setEntry( row, entry );
    // Shown again, in simple mode, and previewed soon.
    showCurrentRule();
    showProblems();
}

// ── Private helpers ──────────────────────────────────────────────────────

void FooterEditor::regexLabAnswered( const QPersistentModelIndex& rule, PatternField field,
                                     const RegexLabPattern& opened,
                                     const std::optional<RegexLabPattern>& applied )
{
    setRegexLabOpen( false );
    // Cancelled, the editor closed (its rules are saved or dropped already),
    // or the rule removed: nothing to write the pattern into. Applied as it
    // was opened: nothing to change, not even edits made meanwhile.
    if ( !applied || closing_ || !rule.isValid()
         || ( applied->pattern == opened.pattern && applied->matchesCase == opened.matchesCase ) ) {
        return;
    }
    const auto pattern = fromRegexLab( *applied );
    const int row = rule.row();
    if ( row == currentRow() ) {
        // As if typed: the panel stores it, and the check and the preview follow.
        panel_->setPattern( field, pattern );
        return;
    }
    auto entry = model_->entry( row );
    auto& target = patternOf( entry, field );
    if ( target == pattern ) {
        return;
    }
    target = pattern;
    // A pattern written by hand: no longer an unfinished simple rule.
    model_->setEntry( row, entry );
    showProblems();
    // Another rule of the key may supply its value now.
    schedulePreview();
}

void FooterEditor::setRegexLabOpen( bool open )
{
    regexLabOpen_ = open;
    updatePatternTest();
}

void FooterEditor::updatePatternTest()
{
    if ( !openRegexLab_ ) {
        panel_->setPatternTestUnavailable( tr( "Needs LogSquirl 26.11 or later" ) );
    }
    else if ( regexLabOpen_ ) {
        panel_->setPatternTestUnavailable(
            tr( "The Regex Lab is open: apply or cancel it there first" ) );
    }
    else {
        panel_->setPatternTestUnavailable( {} );
    }
}

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

void FooterEditor::appendAndSelect( const QList<FooterEntry>& entries )
{
    const int row = model_->rowCount();
    model_->appendEntries( entries );
    selectRow( row );
    panel_->focusKey();
}

bool FooterEditor::isLineRule( int row ) const
{
    return row >= 0 && lineRule_.isValid() && lineRule_.row() == row;
}

void FooterEditor::showLineRulePreview( const FooterScanner::Preview& preview )
{
    // The preview is always of the selected rule.
    if ( !isLineRule( currentRow() ) ) {
        return;
    }
    if ( preview.status != FooterScanner::Preview::Status::Scanned || !preview.value ) {
        linePicker_->clearEarlierMatch();
        return;
    }
    // The preview's line is as matched: without a line break, and only the
    // start of a long line.
    auto matched = QStringView( preview.line );
    auto shown = QStringView( linePicker_->line() );
    if ( matched.endsWith( QLatin1Char( '\r' ) ) ) {
        matched.chop( 1 );
    }
    if ( shown.endsWith( QLatin1Char( '\r' ) ) ) {
        shown.chop( 1 );
    }
    // Both may be only the start of a long line.
    if ( !matched.isEmpty() && ( shown.startsWith( matched ) || matched.startsWith( shown ) ) ) {
        linePicker_->clearEarlierMatch();
        return;
    }
    linePicker_->showEarlierMatch( preview.lineNumber, preview.value->value );
}

void FooterEditor::moveEntry( int from, int to )
{
    // The selection moves with the rule, and the panel keeps showing it.
    model_->moveRule( from, to );
    list_->scrollTo( model_->index( to, RuleListModel::KeyColumn ) );
}

void FooterEditor::showProblems()
{
    const auto& check = model_->check();
    const bool valid = check.canSave();
    panel_->setProblems( model_->problems( currentRow() ) );
    problemLabel_->setText( check.listing().join( '\n' ) );
    problemLabel_->setVisible( !valid );
    buttonBox_->button( QDialogButtonBox::Ok )->setEnabled( valid );
    buttonBox_->button( QDialogButtonBox::Apply )->setEnabled( valid );
}

void FooterEditor::schedulePreview( RulePreviewer::Start start )
{
    if ( closing_ ) {
        return;
    }
    const int row = currentRow();
    if ( row < 0 ) {
        previewer_->clear();
        panel_->previewView()->showNoRule();
        return;
    }
    // A rule that cannot match anything, e.g. with an invalid pattern or
    // as an unfinished simple rule: the check's problem says why, and
    // nothing is scanned.
    const auto problem = model_->problems( row ).preventingMatches();
    if ( !problem.isEmpty() ) {
        previewer_->clear();
        panel_->previewView()->showRuleProblem( problem );
        if ( isLineRule( row ) ) {
            // Nothing matches first now.
            linePicker_->clearEarlierMatch();
        }
        return;
    }
    previewer_->schedule( model_->entries(), row, start );
}

} // namespace custom_footer
