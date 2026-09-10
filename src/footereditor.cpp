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

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

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

    bool isChecked() const { return checkbox_->isChecked(); }
    void setChecked( bool checked ) { checkbox_->setChecked( checked ); }

  private:
    QCheckBox* checkbox_;
};

} // namespace

// ── FooterEditor ─────────────────────────────────────────────────────────

FooterEditor::FooterEditor( const QList<FooterEntry>& entries,
                            QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle( tr( "Custom Footer — Edit Rules" ) );
    setMinimumSize( 700, 500 );

    auto* mainLayout = new QVBoxLayout( this );

    // ── Rules table (5 columns) ──────────────────────────────────────────
    table_ = new QTableWidget( 0, 5, this );
    table_->setHorizontalHeaderLabels(
        { tr( "Enabled" ), tr( "Key" ), tr( "Line Pattern" ),
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
    removeButton_->setText( "\u2212" ); // minus sign
    removeButton_->setToolTip( tr( "Remove entry" ) );
    toolLayout->addWidget( removeButton_ );

    upButton_ = new QToolButton( this );
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
    auto* mappingLayout = new QVBoxLayout( mappingGroup_ );

    mappingTable_ = new QTableWidget( 0, 2, mappingGroup_ );
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

    // ── Button Box ───────────────────────────────────────────────────────
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

    connect( table_, &QTableWidget::currentCellChanged, this,
             [this]( int, int, int, int ) {
                 onRuleSelectionChanged();
                 updateButtons();
             } );

    connect( addMappingButton_, &QToolButton::clicked, this, &FooterEditor::addMapping );
    connect( removeMappingButton_, &QToolButton::clicked, this, &FooterEditor::removeMapping );

    // Enable/disable remove-mapping button when mapping table selection changes.
    connect( mappingTable_, &QTableWidget::currentCellChanged, this,
             [this]( int, int, int, int ) {
                 removeMappingButton_->setEnabled( mappingTable_->currentRow() >= 0 );
             } );

    // Sync mapping edits back to mappingsData_ when cells change
    connect( mappingTable_, &QTableWidget::cellChanged, this,
             &FooterEditor::syncMappingsToEntry );

    connect( buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject );
    connect( buttonBox_->button( QDialogButtonBox::Apply ), &QPushButton::clicked,
             this, &FooterEditor::applied );

    // ── Populate ─────────────────────────────────────────────────────────
    populateTable( entries );
    updateButtons();
}

// ── Public accessors ─────────────────────────────────────────────────────

QList<FooterEntry> FooterEditor::entries()
{
    // Flush any pending mapping-table state for the active row.
    saveMappingsForRow( currentMappingRow_ );
    return tableToEntries();
}

// ── Slots ────────────────────────────────────────────────────────────────

void FooterEditor::addEntry()
{
    const int row = table_->rowCount();
    table_->setRowCount( row + 1 );

    auto* checkbox = new CenteredCheckbox( true, table_ );
    table_->setCellWidget( row, 0, checkbox );
    table_->setItem( row, 1, new QTableWidgetItem( "" ) );
    table_->setItem( row, 2, new QTableWidgetItem( "" ) );
    table_->setItem( row, 3, new QTableWidgetItem( "" ) );

    // Mappings count label (read-only)
    auto* mappingsItem = new QTableWidgetItem( "0" );
    mappingsItem->setFlags( mappingsItem->flags() & ~Qt::ItemIsEditable );
    mappingsItem->setTextAlignment( Qt::AlignCenter );
    table_->setItem( row, 4, mappingsItem );

    mappingsData_.append( QList<ValueMapping>() );

    table_->scrollToItem( table_->item( row, 1 ) );
    table_->setCurrentCell( row, 1 );
    table_->editItem( table_->item( row, 1 ) );

    updateButtons();
}

void FooterEditor::removeEntry()
{
    const int row = table_->currentRow();
    if ( row < 0 ) {
        return;
    }

    // Clear mapping panel if the removed row is currently shown
    if ( row == currentMappingRow_ ) {
        currentMappingRow_ = -1;
        mappingTable_->setRowCount( 0 );
        mappingGroup_->setEnabled( false );
    }
    else if ( row < currentMappingRow_ ) {
        --currentMappingRow_;
    }

    table_->removeRow( row );
    mappingsData_.removeAt( row );
    updateButtons();
}

void FooterEditor::moveEntryUp()
{
    const int row = table_->currentRow();
    if ( row <= 0 ) {
        return;
    }

    // Save current mapping panel state before rebuilding
    saveMappingsForRow( currentMappingRow_ );

    auto entryList = tableToEntries();
    entryList.move( row, row - 1 );
    populateTable( entryList );
    table_->setCurrentCell( row - 1, 1 );
    updateButtons();
}

void FooterEditor::moveEntryDown()
{
    const int row = table_->currentRow();
    if ( row < 0 || row >= table_->rowCount() - 1 ) {
        return;
    }

    saveMappingsForRow( currentMappingRow_ );

    auto entryList = tableToEntries();
    entryList.move( row, row + 1 );
    populateTable( entryList );
    table_->setCurrentCell( row + 1, 1 );
    updateButtons();
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
        this, tr( "Import Rules" ), QString(),
        tr( "JSON Files (*.json);;All Files (*)" ) );
    if ( filePath.isEmpty() ) {
        return;
    }

    QString error;
    const auto imported = FooterConfig::importFromJson( filePath, &error );
    if ( imported.isEmpty() && !error.isEmpty() ) {
        QMessageBox::warning( this, tr( "Import Error" ), error );
        return;
    }

    // Append imported entries to the current list
    const auto current = tableToEntries();
    auto combined = current + imported;
    populateTable( combined );
    updateButtons();
}

void FooterEditor::exportRules()
{
    const auto filePath = QFileDialog::getSaveFileName(
        this, tr( "Export Rules" ), QStringLiteral( "footer_rules.json" ),
        tr( "JSON Files (*.json);;All Files (*)" ) );
    if ( filePath.isEmpty() ) {
        return;
    }

    const auto allEntries = tableToEntries();
    if ( !FooterConfig::exportToJson( filePath, allEntries ) ) {
        QMessageBox::warning( this, tr( "Export Error" ),
                              tr( "Could not write to file: %1" ).arg( filePath ) );
    }
}

void FooterEditor::onRuleSelectionChanged()
{
    // Save mappings for the previously selected row
    saveMappingsForRow( currentMappingRow_ );

    const int row = table_->currentRow();
    currentMappingRow_ = row;

    if ( row < 0 || row >= mappingsData_.size() ) {
        mappingGroup_->setEnabled( false );
        mappingTable_->setRowCount( 0 );
        return;
    }

    mappingGroup_->setEnabled( true );
    loadMappingsForRow( row );
}

void FooterEditor::addMapping()
{
    if ( currentMappingRow_ < 0 ) {
        return;
    }

    const int row = mappingTable_->rowCount();
    mappingTable_->setRowCount( row + 1 );
    mappingTable_->setItem( row, 0, new QTableWidgetItem( "" ) );
    mappingTable_->setItem( row, 1, new QTableWidgetItem( "" ) );
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
    syncMappingsToEntry();
    updateButtons();
}

void FooterEditor::syncMappingsToEntry()
{
    if ( currentMappingRow_ < 0 || currentMappingRow_ >= mappingsData_.size() ) {
        return;
    }

    saveMappingsForRow( currentMappingRow_ );
    updateMappingLabel( currentMappingRow_ );
}

// ── Private helpers ──────────────────────────────────────────────────────

void FooterEditor::populateTable( const QList<FooterEntry>& entries )
{
    mappingsData_.clear();
    table_->setRowCount( entries.size() );

    for ( int i = 0; i < entries.size(); ++i ) {
        auto* checkbox = new CenteredCheckbox( entries[ i ].enabled, table_ );
        table_->setCellWidget( i, 0, checkbox );
        table_->setItem( i, 1, new QTableWidgetItem( entries[ i ].key ) );
        table_->setItem( i, 2, new QTableWidgetItem( entries[ i ].linePattern ) );
        table_->setItem( i, 3, new QTableWidgetItem( entries[ i ].valuePattern ) );

        auto* mappingsItem
            = new QTableWidgetItem( QString::number( entries[ i ].mappings.size() ) );
        mappingsItem->setFlags( mappingsItem->flags() & ~Qt::ItemIsEditable );
        mappingsItem->setTextAlignment( Qt::AlignCenter );
        table_->setItem( i, 4, mappingsItem );

        mappingsData_.append( entries[ i ].mappings );
    }

    // Reset mapping panel
    currentMappingRow_ = -1;
    mappingTable_->setRowCount( 0 );
    mappingGroup_->setEnabled( false );
}

QList<FooterEntry> FooterEditor::tableToEntries() const
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

        if ( i < mappingsData_.size() ) {
            entry.mappings = mappingsData_[ i ];
        }

        result.append( entry );
    }
    return result;
}

void FooterEditor::loadMappingsForRow( int row )
{
    mappingTable_->blockSignals( true );
    const auto& mappings = mappingsData_[ row ];
    mappingTable_->setRowCount( mappings.size() );
    for ( int i = 0; i < mappings.size(); ++i ) {
        mappingTable_->setItem( i, 0, new QTableWidgetItem( mappings[ i ].pattern ) );
        mappingTable_->setItem( i, 1, new QTableWidgetItem( mappings[ i ].displayValue ) );
    }
    mappingTable_->blockSignals( false );
}

void FooterEditor::saveMappingsForRow( int row )
{
    if ( row < 0 || row >= mappingsData_.size() ) {
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
    mappingsData_[ row ] = mappings;
}

void FooterEditor::updateMappingLabel( int row )
{
    if ( row < 0 || row >= table_->rowCount() ) {
        return;
    }
    auto* item = table_->item( row, 4 );
    if ( item ) {
        item->setText( QString::number( mappingsData_[ row ].size() ) );
    }
}

} // namespace custom_footer
