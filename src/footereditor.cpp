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

#include "footereditor.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace costume_footer {

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
                            DisplayMode mode, QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle( tr( "Custom Footer — Edit Rules" ) );
    setMinimumSize( 520, 360 );

    auto* mainLayout = new QVBoxLayout( this );

    // ── Table ────────────────────────────────────────────────────────────
    table_ = new QTableWidget( 0, 3, this );
    table_->setHorizontalHeaderLabels( { tr( "Enabled" ), tr( "Key" ), tr( "Regex" ) } );
    table_->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    table_->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Interactive );
    table_->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Stretch );
    table_->verticalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
    table_->setSelectionBehavior( QAbstractItemView::SelectRows );
    table_->setSelectionMode( QAbstractItemView::SingleSelection );
    table_->setWordWrap( false );
    mainLayout->addWidget( table_ );

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
    mainLayout->addLayout( toolLayout );

    // ── Display Mode ─────────────────────────────────────────────────────
    auto* modeLayout = new QHBoxLayout;
    modeLayout->addWidget( new QLabel( tr( "Display in:" ), this ) );

    modeCombo_ = new QComboBox( this );
    modeCombo_->addItem( tr( "Footer" ), static_cast<int>( DisplayMode::Footer ) );
    modeCombo_->addItem( tr( "Sidebar" ), static_cast<int>( DisplayMode::Sidebar ) );
    modeCombo_->addItem( tr( "Both" ), static_cast<int>( DisplayMode::Both ) );
    modeCombo_->setCurrentIndex( static_cast<int>( mode ) );
    modeLayout->addWidget( modeCombo_ );

    modeLayout->addStretch();
    mainLayout->addLayout( modeLayout );

    // ── Button Box ───────────────────────────────────────────────────────
    buttonBox_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this );
    mainLayout->addWidget( buttonBox_ );

    // ── Connections ──────────────────────────────────────────────────────
    connect( addButton_, &QToolButton::clicked, this, &FooterEditor::addEntry );
    connect( removeButton_, &QToolButton::clicked, this, &FooterEditor::removeEntry );
    connect( upButton_, &QToolButton::clicked, this, &FooterEditor::moveEntryUp );
    connect( downButton_, &QToolButton::clicked, this, &FooterEditor::moveEntryDown );
    connect( table_, &QTableWidget::currentCellChanged, this,
             [this]( int, int, int, int ) { updateButtons(); } );

    connect( buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject );
    connect( buttonBox_->button( QDialogButtonBox::Apply ), &QPushButton::clicked,
             this, &FooterEditor::applied );

    // ── Populate ─────────────────────────────────────────────────────────
    populateTable( entries );
    updateButtons();
}

// ── Public accessors ─────────────────────────────────────────────────────

QList<FooterEntry> FooterEditor::entries() const
{
    return tableToEntries();
}

DisplayMode FooterEditor::displayMode() const
{
    return static_cast<DisplayMode>( modeCombo_->currentData().toInt() );
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
    table_->removeRow( row );
    updateButtons();
}

void FooterEditor::moveEntryUp()
{
    const int row = table_->currentRow();
    if ( row <= 0 ) {
        return;
    }

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
}

// ── Private helpers ──────────────────────────────────────────────────────

void FooterEditor::populateTable( const QList<FooterEntry>& entries )
{
    table_->setRowCount( entries.size() );
    for ( int i = 0; i < entries.size(); ++i ) {
        auto* checkbox = new CenteredCheckbox( entries[ i ].enabled, table_ );
        table_->setCellWidget( i, 0, checkbox );
        table_->setItem( i, 1, new QTableWidgetItem( entries[ i ].key ) );
        table_->setItem( i, 2, new QTableWidgetItem( entries[ i ].regexPattern ) );
    }
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

        auto* regexItem = table_->item( i, 2 );
        entry.regexPattern = regexItem ? regexItem->text() : QString();

        result.append( entry );
    }
    return result;
}

} // namespace costume_footer
