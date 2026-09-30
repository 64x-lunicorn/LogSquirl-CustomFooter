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

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace custom_footer {

// ── Centered checkbox helper ─────────────────────────────────────────────

namespace {

/// A QCheckBox centered in a container widget, for use in QTableWidget cells.
class CenteredCheckbox : public QWidget {
public:
    explicit CenteredCheckbox( bool checked, QWidget* parent = nullptr )
        : QWidget( parent )
    {
        auto* layout = new QHBoxLayout( this );
        layout->setAlignment( Qt::AlignCenter );
        layout->setContentsMargins( 0, 0, 0, 0 );
        checkbox_ = new QCheckBox;
        checkbox_->setChecked( checked );
        layout->addWidget( checkbox_ );
    }

    bool isChecked() const
    {
        return checkbox_->isChecked();
    }
    QCheckBox* checkbox() const
    {
        return checkbox_;
    }

private:
    QCheckBox* checkbox_;
};

/// Background for cells whose content keeps the rules from being saved.
const QColor kProblemColor( 220, 50, 50, 70 );

} // namespace

// ── FooterEditor ─────────────────────────────────────────────────────────

FooterEditor::FooterEditor( const QList<FooterEntry>& entries, QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle( tr( "Custom Footer — Edit Rules" ) );
    setMinimumSize( 700, 500 );

    auto* mainLayout = new QVBoxLayout( this );

    // ── Rules table (5 columns) ──────────────────────────────────────────
    table_ = new QTableWidget( 0, 5, this );
    table_->setObjectName( "rulesTable" );
    table_->setHorizontalHeaderLabels( { tr( "Enabled" ), tr( "Key" ), tr( "Line Pattern" ),
                                         tr( "Value Pattern" ), tr( "Mappings" ) } );
    table_->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    table_->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Interactive );
    table_->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Stretch );
    table_->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::Stretch );
    table_->horizontalHeader()->setSectionResizeMode( 4, QHeaderView::ResizeToContents );
    table_->verticalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
    table_->setSelectionBehavior( QAbstractItemView::SelectRows );
    table_->setSelectionMode( QAbstractItemView::SingleSelection );
    table_->setWordWrap( false );
    mainLayout->addWidget( table_, 3 );

    // ── Toolbar ──────────────────────────────────────────────────────────
    auto* toolLayout = new QHBoxLayout;

    addButton_ = new QToolButton( this );
    addButton_->setText( "+" );
    addButton_->setToolTip( tr( "Add entry" ) );
    toolLayout->addWidget( addButton_ );

    removeButton_ = new QToolButton( this );
    removeButton_->setObjectName( "removeRuleButton" );
    removeButton_->setText( "\u2212" ); // minus sign
    removeButton_->setToolTip( tr( "Remove entry" ) );
    toolLayout->addWidget( removeButton_ );

    upButton_ = new QToolButton( this );
    upButton_->setObjectName( "moveUpButton" );
    upButton_->setText( "\u2191" ); // up arrow
    upButton_->setToolTip( tr( "Move up" ) );
    toolLayout->addWidget( upButton_ );

    downButton_ = new QToolButton( this );
    downButton_->setText( "\u2193" ); // down arrow
    downButton_->setToolTip( tr( "Move down" ) );
    toolLayout->addWidget( downButton_ );

    toolLayout->addStretch();

    importButton_ = new QToolButton( this );
    importButton_->setText( tr( "Import" ) );
    importButton_->setToolTip( tr( "Import rules from a JSON file" ) );
    toolLayout->addWidget( importButton_ );

    exportButton_ = new QToolButton( this );
    exportButton_->setText( tr( "Export" ) );
    exportButton_->setToolTip( tr( "Export rules to a JSON file" ) );
    toolLayout->addWidget( exportButton_ );

    mainLayout->addLayout( toolLayout );

    // ── Mapping editor panel ─────────────────────────────────────────────
    mappingGroup_ = new QGroupBox( tr( "Value Mappings" ), this );
    mappingGroup_->setObjectName( "mappingGroup" );
    auto* mappingLayout = new QVBoxLayout( mappingGroup_ );

    mappingTable_ = new QTableWidget( 0, 2, mappingGroup_ );
    mappingTable_->setObjectName( "mappingTable" );
    mappingTable_->setHorizontalHeaderLabels( { tr( "Pattern" ), tr( "Display" ) } );
    mappingTable_->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    mappingTable_->verticalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
    mappingTable_->setSelectionBehavior( QAbstractItemView::SelectRows );
    mappingTable_->setSelectionMode( QAbstractItemView::SingleSelection );
    mappingLayout->addWidget( mappingTable_ );

    auto* mappingToolLayout = new QHBoxLayout;
    addMappingButton_ = new QToolButton( mappingGroup_ );
    addMappingButton_->setText( "+" );
    addMappingButton_->setToolTip( tr( "Add mapping" ) );
    mappingToolLayout->addWidget( addMappingButton_ );

    removeMappingButton_ = new QToolButton( mappingGroup_ );
    removeMappingButton_->setText( "\u2212" );
    removeMappingButton_->setToolTip( tr( "Remove mapping" ) );
    mappingToolLayout->addWidget( removeMappingButton_ );

    mappingToolLayout->addStretch();
    mappingLayout->addLayout( mappingToolLayout );

    mappingGroup_->setEnabled( false );
    mainLayout->addWidget( mappingGroup_, 2 );

    // ── Problems + Button Box ────────────────────────────────────────────
    problemLabel_ = new QLabel( this );
    problemLabel_->setWordWrap( true );
    problemLabel_->hide();
    mainLayout->addWidget( problemLabel_ );

    buttonBox_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this );
    mainLayout->addWidget( buttonBox_ );

    // ── Connections ──────────────────────────────────────────────────────
    connect( addButton_, &QToolButton::clicked, this, &FooterEditor::addEntry );
    connect( removeButton_, &QToolButton::clicked, this, &FooterEditor::removeEntry );
    connect( upButton_, &QToolButton::clicked, this, &FooterEditor::moveEntryUp );
    connect( downButton_, &QToolButton::clicked, this, &FooterEditor::moveEntryDown );
    connect( importButton_, &QToolButton::clicked, this, &FooterEditor::importRules );
    connect( exportButton_, &QToolButton::clicked, this, &FooterEditor::exportRules );

    connect( table_, &QTableWidget::itemChanged, this, &FooterEditor::validate );
    connect( table_, &QTableWidget::currentCellChanged, this, [ this ]( int, int, int, int ) {
        showMappingsOfCurrentRule();
        updateButtons();
    } );

    connect( addMappingButton_, &QToolButton::clicked, this, &FooterEditor::addMapping );
    connect( removeMappingButton_, &QToolButton::clicked, this, &FooterEditor::removeMapping );

    // Enable/disable remove-mapping button when mapping table selection changes.
    connect( mappingTable_, &QTableWidget::currentCellChanged, this,
             [ this ]( int, int, int, int ) {
                 removeMappingButton_->setEnabled( mappingTable_->currentRow() >= 0 );
             } );

    // Store mapping edits in the current rule as they are made.
    connect( mappingTable_, &QTableWidget::cellChanged, this,
             &FooterEditor::storeMappingsOfCurrentRule );

    connect( buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject );
    connect( buttonBox_->button( QDialogButtonBox::Apply ), &QPushButton::clicked, this,
             &FooterEditor::applied );

    // ── Populate ─────────────────────────────────────────────────────────
    populateTable( entries );
}

// ── Public accessors ─────────────────────────────────────────────────────

QList<FooterEntry> FooterEditor::entries() const
{
    QList<FooterEntry> result;
    const int rows = table_->rowCount();
    result.reserve( rows );

    for ( int i = 0; i < rows; ++i ) {
        FooterEntry entry;
        auto* checkbox = static_cast<CenteredCheckbox*>( table_->cellWidget( i, 0 ) );
        entry.enabled = checkbox ? checkbox->isChecked() : true;

        auto* keyItem = table_->item( i, 1 );
        entry.key = keyItem ? keyItem->text() : QString();

        auto* lineItem = table_->item( i, 2 );
        entry.linePattern = lineItem ? lineItem->text() : QString();

        auto* valueItem = table_->item( i, 3 );
        entry.valuePattern = valueItem ? valueItem->text() : QString();

        entry.mappings = mappingsOf( i );

        result.append( entry );
    }
    return result;
}

void FooterEditor::appendEntries( const QList<FooterEntry>& entries )
{
    populateTable( this->entries() + entries );
}

// ── Slots ────────────────────────────────────────────────────────────────

void FooterEditor::addEntry()
{
    const int row = table_->rowCount();
    table_->setRowCount( row + 1 );
    setRow( row, FooterEntry() );

    table_->scrollToItem( table_->item( row, 1 ) );
    table_->setCurrentCell( row, 1 );
    table_->editItem( table_->item( row, 1 ) );
}

void FooterEditor::removeEntry()
{
    const int row = table_->currentRow();
    if ( row < 0 ) {
        return;
    }

    {
        // Select the next rule, or the one above the last, explicitly
        // rather than through whatever removeRow() makes current.
        const QSignalBlocker blocker( table_ );
        table_->removeRow( row );
        if ( table_->rowCount() > 0 ) {
            table_->setCurrentCell( std::min( row, table_->rowCount() - 1 ), 1 );
        }
    }

    showMappingsOfCurrentRule();
    updateButtons();
    validate();
}

void FooterEditor::moveEntryUp()
{
    const int row = table_->currentRow();
    if ( row > 0 ) {
        moveEntry( row, row - 1 );
    }
}

void FooterEditor::moveEntryDown()
{
    const int row = table_->currentRow();
    if ( row >= 0 && row < table_->rowCount() - 1 ) {
        moveEntry( row, row + 1 );
    }
}

void FooterEditor::updateButtons()
{
    const int row = table_->currentRow();
    const int count = table_->rowCount();
    removeButton_->setEnabled( row >= 0 );
    upButton_->setEnabled( row > 0 );
    downButton_->setEnabled( row >= 0 && row < count - 1 );
    removeMappingButton_->setEnabled( mappingTable_->currentRow() >= 0 );
}

void FooterEditor::importRules()
{
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

void FooterEditor::showMappingsOfCurrentRule()
{
    const int row = table_->currentRow();
    const auto mappings = row >= 0 ? mappingsOf( row ) : QList<ValueMapping>();

    const QSignalBlocker blocker( mappingTable_ );
    mappingTable_->setRowCount( mappings.size() );
    for ( int i = 0; i < mappings.size(); ++i ) {
        mappingTable_->setItem( i, 0, new QTableWidgetItem( mappings[ i ].pattern ) );
        mappingTable_->setItem( i, 1, new QTableWidgetItem( mappings[ i ].displayValue ) );
    }
    mappingGroup_->setEnabled( row >= 0 );
}

void FooterEditor::addMapping()
{
    if ( table_->currentRow() < 0 ) {
        return;
    }

    const int row = mappingTable_->rowCount();
    {
        const QSignalBlocker blocker( mappingTable_ );
        mappingTable_->setRowCount( row + 1 );
        mappingTable_->setItem( row, 0, new QTableWidgetItem( "" ) );
        mappingTable_->setItem( row, 1, new QTableWidgetItem( "" ) );
    }
    storeMappingsOfCurrentRule();

    mappingTable_->setCurrentCell( row, 0 );
    mappingTable_->editItem( mappingTable_->item( row, 0 ) );
    updateButtons();
}

void FooterEditor::removeMapping()
{
    const int row = mappingTable_->currentRow();
    if ( row < 0 ) {
        return;
    }
    mappingTable_->removeRow( row );
    storeMappingsOfCurrentRule();
    updateButtons();
}

void FooterEditor::storeMappingsOfCurrentRule()
{
    const int row = table_->currentRow();
    if ( row < 0 ) {
        return;
    }

    QList<ValueMapping> mappings;
    for ( int i = 0; i < mappingTable_->rowCount(); ++i ) {
        ValueMapping m;
        auto* pItem = mappingTable_->item( i, 0 );
        m.pattern = pItem ? pItem->text() : QString();
        auto* dItem = mappingTable_->item( i, 1 );
        m.displayValue = dItem ? dItem->text() : QString();
        mappings.append( m );
    }
    setMappings( row, mappings );
}

void FooterEditor::validate()
{
    QStringList problems;
    QSet<QString> enabledKeys;

    // Marking a cell changes its item: do not validate again for that.
    const QSignalBlocker blocker( table_ );
    for ( int row = 0; row < table_->rowCount(); ++row ) {
        const auto mark = [ this, row, &problems ]( int column, const QString& problem ) {
            auto* item = table_->item( row, column );
            if ( !item ) {
                return;
            }
            item->setToolTip( problem );
            item->setBackground( problem.isEmpty() ? QBrush() : QBrush( kProblemColor ) );
            if ( !problem.isEmpty() ) {
                problems.append( tr( "Rule %1: %2" ).arg( row + 1 ).arg( problem ) );
            }
        };

        const auto* keyItem = table_->item( row, 1 );
        const auto key = keyItem ? keyItem->text() : QString();
        const auto* checkbox = static_cast<CenteredCheckbox*>( table_->cellWidget( row, 0 ) );
        QString keyProblem;
        if ( checkbox && checkbox->isChecked() ) {
            if ( enabledKeys.contains( key ) ) {
                keyProblem
                    = tr( "the key \"%1\" is already used by an enabled rule above" ).arg( key );
            }
            enabledKeys.insert( key );
        }
        mark( 1, keyProblem );

        const auto patternProblem = [ this, row ]( int column, const QString& what ) {
            const auto* item = table_->item( row, column );
            const auto error = FooterScanner::patternError( item ? item->text() : QString() );
            return error.isEmpty() ? QString() : tr( "%1: %2" ).arg( what, error );
        };
        mark( 2, patternProblem( 2, tr( "invalid line pattern" ) ) );
        mark( 3, patternProblem( 3, tr( "invalid value pattern" ) ) );
    }

    problemLabel_->setText( problems.join( '\n' ) );
    problemLabel_->setVisible( !problems.isEmpty() );
    buttonBox_->button( QDialogButtonBox::Ok )->setEnabled( problems.isEmpty() );
    buttonBox_->button( QDialogButtonBox::Apply )->setEnabled( problems.isEmpty() );
}

// ── Private helpers ──────────────────────────────────────────────────────

void FooterEditor::populateTable( const QList<FooterEntry>& entries )
{
    table_->setRowCount( entries.size() );
    for ( int i = 0; i < entries.size(); ++i ) {
        setRow( i, entries[ i ] );
    }

    // The current cell may stay where it was while its rule is replaced.
    showMappingsOfCurrentRule();
    updateButtons();
    validate();
}

void FooterEditor::setRow( int row, const FooterEntry& entry )
{
    auto* checkbox = new CenteredCheckbox( entry.enabled, table_ );
    connect( checkbox->checkbox(), &QCheckBox::toggled, this, &FooterEditor::validate );
    table_->setCellWidget( row, 0, checkbox );
    table_->setItem( row, 1, new QTableWidgetItem( entry.key ) );
    table_->setItem( row, 2, new QTableWidgetItem( entry.linePattern ) );
    table_->setItem( row, 3, new QTableWidgetItem( entry.valuePattern ) );

    // Mappings count label (read-only)
    auto* mappingsItem = new QTableWidgetItem;
    mappingsItem->setFlags( mappingsItem->flags() & ~Qt::ItemIsEditable );
    mappingsItem->setTextAlignment( Qt::AlignCenter );
    table_->setItem( row, 4, mappingsItem );
    setMappings( row, entry.mappings );
}

void FooterEditor::moveEntry( int from, int to )
{
    auto entryList = entries();
    entryList.move( from, to );
    populateTable( entryList );
    table_->setCurrentCell( to, 1 );
}

QList<ValueMapping> FooterEditor::mappingsOf( int row ) const
{
    const auto* item = table_->item( row, 4 );
    return item ? item->data( Qt::UserRole ).value<QList<ValueMapping>>() : QList<ValueMapping>();
}

void FooterEditor::setMappings( int row, const QList<ValueMapping>& mappings )
{
    auto* item = table_->item( row, 4 );
    if ( item ) {
        item->setData( Qt::UserRole, QVariant::fromValue( mappings ) );
        item->setText( QString::number( mappings.size() ) );
    }
}

} // namespace custom_footer
