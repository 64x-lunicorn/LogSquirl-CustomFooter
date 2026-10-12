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

#include "footersession.h"

#include "footerconfig.h"
#include "footercontroller.h"
#include "footerdisplaywidget.h"
#include "footereditor.h"

#include <QApplication>
#include <QEvent>

namespace custom_footer {

namespace {

/**
 * Keeps the editor over an application-modal window that blocks the
 * editor's own window, e.g. the host's Plugins dialog, and moves it back
 * over its own window when that modal window is hidden, with its content,
 * so that the editor is not deleted with it. A child of the editor, so it
 * goes with the editor; it posts nothing.
 */
class ModalGuest : public QObject {
public:
    ModalGuest( FooterEditor* editor, QWidget* modal, QWidget* home, HostFunctions host )
        : QObject( editor )
        , editor_( editor )
        , modal_( modal )
        , home_( home )
        , host_( std::move( host ) )
    {
        modal->installEventFilter( this );
    }

    ~ModalGuest() override
    {
        leave();
    }

    ModalGuest( const ModalGuest& ) = delete;
    ModalGuest& operator=( const ModalGuest& ) = delete;

protected:
    bool eventFilter( QObject* watched, QEvent* event ) override
    {
        if ( watched == modal_ && event->type() == QEvent::Hide ) {
            guarded( host_, "moving the rule editor", [ this ] { returnHome(); } );
        }
        return QObject::eventFilter( watched, event );
    }

private:
    void returnHome()
    {
        auto* modal = modal_.data();
        leave();
        if ( editor_->parentWidget() != modal ) {
            return;
        }
        const bool wasOpen = editor_->isVisible();
        editor_->setParent( home_, editor_->windowFlags() );
        if ( wasOpen ) {
            editor_->open();
        }
    }

    void leave()
    {
        if ( modal_ ) {
            modal_->removeEventFilter( this );
            modal_.clear();
        }
    }

    FooterEditor* editor_;
    QPointer<QWidget> modal_;
    QPointer<QWidget> home_;
    const HostFunctions host_;
};

/// The application-modal window that blocks the editor, if one does: not
/// the editor, nor one of its own dialogs, nor a window it is already over.
QWidget* blockingModal( QWidget* editor )
{
    auto* modal = QApplication::activeModalWidget();
    if ( !modal ) {
        return nullptr;
    }
    for ( auto* widget = modal; widget; widget = widget->parentWidget() ) {
        if ( widget == editor ) {
            return nullptr;
        }
    }
    for ( auto* widget = editor->parentWidget(); widget; widget = widget->parentWidget() ) {
        if ( widget == modal ) {
            return nullptr;
        }
    }
    return modal;
}

/**
 * Put the editor over @p home, the window it is opened from, or over the
 * application-modal window blocking that one, where it is usable. Moving
 * keeps its window flags and its content, edits included.
 */
void placeEditor( FooterEditor* editor, QWidget* home, const HostFunctions& host )
{
    if ( !home ) {
        home = editor->parentWidget();
    }
    const auto children = editor->children();
    for ( auto* child : children ) {
        delete dynamic_cast<ModalGuest*>( child );
    }
    auto* modal = blockingModal( editor );
    auto* over = modal ? modal : home;
    if ( over && editor->parentWidget() != over ) {
        editor->setParent( over, editor->windowFlags() );
    }
    if ( modal ) {
        new ModalGuest( editor, modal, home, host );
    }
}

} // namespace

FooterSession::FooterSession( const QString& configDir, FooterDisplayWidget* footer,
                              HostFunctions host )
    : configDir_( configDir )
    , host_( std::move( host ) )
{
    if ( configDir_.isEmpty() ) {
        host_.logMessage( LOGSQUIRL_LOG_WARNING,
                          QStringLiteral( "Custom Footer: the host gave no config directory; "
                                          "rules are neither loaded nor saved" ) );
    }
    controller_ = std::make_unique<FooterController>(
        footer, configDir_, [ host = host_ ]( const QString& warning ) {
            host.logMessage( LOGSQUIRL_LOG_WARNING,
                             QStringLiteral( "Custom Footer: %1" ).arg( warning ) );
        } );
}

FooterSession::~FooterSession()
{
    // First the editor, open or closed but not yet deleted: deleting it
    // stops its preview and waits for it, and drops its dialogs, timers and
    // pending events, its deferred deletion included. Nothing of the plugin
    // is on the stack here, as the editor and its dialogs run in the host's
    // event loop, not in one of ours. The one nested loop left is a drag of
    // a rule in the list (QDrag::exec(), which cannot be avoided); a
    // shutdown during a drag cannot be triggered from the UI, as the drag
    // holds the mouse until it ends.
    delete editor_.data();
    // Then the controller: it cancels a running scan and waits for it, and
    // drops results not yet shown.
    controller_.reset();
}

void FooterSession::setActiveFile( const QString& filePath )
{
    activeFile_ = filePath;
    controller_->setActiveFile( filePath );
    if ( editor_ ) {
        editor_->setActiveFile( filePath );
    }
}

FooterEditor* FooterSession::openEditor( QWidget* over )
{
    if ( editor_ && !editor_->isVisible() ) {
        // Closed, and only waiting to be deleted: a new one is opened.
        delete editor_.data();
    }
    if ( editor_ ) {
        // Opened again, maybe from another window, or while the host's
        // Plugins dialog blocks the window the editor is over.
        placeEditor( editor_, over, host_ );
        if ( !editor_->isVisible() ) {
            editor_->open();
        }
        editor_->raise();
        editor_->activateWindow();
        return editor_;
    }

    // Tracked at once, so that whatever throws from here on leaves no
    // editor, with its timers, that the destructor would not delete.
    auto* editor = new FooterEditor( FooterConfig::loadEntries( configDir_ ), over );
    editor_ = editor;
    editor->setMaxLines( FooterConfig::loadMaxLines( configDir_ ) );
    editor->setActiveFile( activeFile_ );
    placeEditor( editor, over, host_ );

    // Apply button: save and rescan without closing the dialog.
    QObject::connect( editor, &FooterEditor::applied, editor, [ this, editor ] {
        guarded( host_, "saving the rules", [ this, editor ] { save( editor->entries() ); } );
    } );
    QObject::connect( editor, &QDialog::finished, editor, [ this, editor ]( int result ) {
        guarded( host_, "saving the rules", [ this, editor, result ] {
            if ( result == QDialog::Accepted ) {
                save( editor->entries() );
            }
        } );
        // Deleted later, as it is still in its own signal; the destructor
        // deletes it at once if that comes first.
        editor->deleteLater();
    } );

    editor->open();
    return editor;
}

void FooterSession::save( const QList<FooterEntry>& entries )
{
    if ( !FooterConfig::saveEntries( configDir_, entries ) ) {
        host_.logMessage(
            LOGSQUIRL_LOG_WARNING,
            QStringLiteral( "Custom Footer: could not save the rules to the config directory" ) );
    }
    controller_->reloadConfig();
}

} // namespace custom_footer
