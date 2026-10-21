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
#include "footerscanner.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QMessageBox>

#include <cstdint>
#include <utility>

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
    , footer_( footer )
{
    controller_ = std::make_unique<FooterController>(
        footer, configDir_, [ host = host_ ]( const QString& warning ) {
            host.logMessage( LOGSQUIRL_LOG_WARNING,
                             QStringLiteral( "Custom Footer: %1" ).arg( warning ) );
        } );

    footer->setGoToLineOffered( has( HostFeature::GoToLogLine ) );
    if ( has( HostFeature::GoToLogLine ) ) {
        goToLineConnection_
            = QObject::connect( footer, &FooterDisplayWidget::goToLineRequested, footer,
                                [ this ]( const FooterValue& value ) { goToLine( value ); } );
    }
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
    // The message that no line is selected, if it is over another window.
    delete noSelectedLine_.data();
    // Then the controller: it cancels a running scan and waits for it, and
    // drops results not yet shown.
    controller_.reset();
    // Last, the footer: its values no longer offer what no one handles.
    QObject::disconnect( goToLineConnection_ );
    guarded( host_, "withdrawing Go to Log Line", [ this ] {
        if ( footer_ ) {
            footer_->setGoToLineOffered( false );
        }
    } );
}

void FooterSession::setActiveFile( const QString& filePath )
{
    activeFile_ = filePath;
    // Asked now, on the UI thread, as the host requires; a change later is
    // told by encodingChanged().
    activeFileEncoding_.clear();
    if ( host_.logFileEncoding && !filePath.isEmpty() ) {
        activeFileEncoding_ = host_.logFileEncoding( filePath ).value_or( QByteArray() );
    }
    controller_->setActiveFile( activeFile_, activeFileEncoding_ );
    if ( editor_ ) {
        editor_->setActiveFile( activeFile_, activeFileEncoding_ );
    }
}

void FooterSession::encodingChanged( const QString& filePath, const QByteArray& encoding )
{
    if ( activeFile_.isEmpty() || filePath != activeFile_ || encoding == activeFileEncoding_ ) {
        return;
    }
    activeFileEncoding_ = encoding;
    controller_->setActiveFile( activeFile_, activeFileEncoding_ );
    if ( editor_ ) {
        editor_->setActiveFile( activeFile_, activeFileEncoding_ );
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
    editor->setActiveFile( activeFile_, activeFileEncoding_ );
    if ( host_.openRegexLab ) {
        // Test… opens the host's Regex Lab; a failure to open it is logged
        // instead of thrown into the editor's button handler.
        editor->setRegexLab(
            [ this ]( const RegexLabPattern& pattern, const QObject* context, RegexLabDone done ) {
                bool opened = false;
                guarded( host_, "opening the Regex Lab", [ & ] {
                    opened = host_.openRegexLab( pattern, context, std::move( done ) );
                } );
                return opened;
            } );
    }
    placeEditor( editor, over, host_ );

    // Rule from selected line, where the host can hand the line over.
    editor->offerRuleFromLine( has( HostFeature::SelectedLogLines ) );
    QObject::connect( editor, &FooterEditor::ruleFromLineRequested, editor, [ this, editor ] {
        guarded( host_, "making a rule from the selected line", [ this, editor ] {
            if ( const auto selected = selectedLinesOrSayWhy( editor ) ) {
                addRuleFromLine( *editor, *selected );
            }
        } );
    } );

    // Apply button: save and rescan without closing the dialog.
    QObject::connect( editor, &FooterEditor::applied, editor,
                      [ this, editor ] { saveFrom( *editor ); } );
    QObject::connect( editor, &QDialog::finished, editor, [ this, editor ]( int result ) {
        if ( result == QDialog::Accepted ) {
            saveFrom( *editor );
        }
        // Deleted later, as it is still in its own signal; the destructor
        // deletes it at once if that comes first.
        editor->deleteLater();
    } );

    editor->open();
    return editor;
}

FooterEditor* FooterSession::ruleFromSelectedLine( QWidget* over )
{
    if ( !host_.selectedLogLines ) {
        return nullptr;
    }
    const bool editorOpen = editor_ && editor_->isVisible();
    const auto selected = selectedLinesOrSayWhy( editorOpen ? editor_.data() : over );
    if ( !selected ) {
        return nullptr;
    }
    // Opened, or the open one brought forward with its edits.
    auto* editor = openEditor( over );
    addRuleFromLine( *editor, *selected );
    return editor;
}

void FooterSession::addRuleFromLine( FooterEditor& editor, const SelectedLogLines& selected )
{
    // Only the first bytes are read, for a byte order mark, as the
    // footer's scan tells the encoding.
    editor.addRuleFromLine( selected.lines.first(), static_cast<int>( selected.lines.size() ),
                            FooterScanner::Encoding::ofFile( activeFile_, activeFileEncoding_ ) );
}

std::optional<SelectedLogLines> FooterSession::selectedLinesOrSayWhy( QWidget* messageOver )
{
    // Read now, on the UI thread, as the host requires.
    auto selected = host_.selectedLogLines ? host_.selectedLogLines() : std::nullopt;
    if ( selected && !selected->lines.isEmpty() ) {
        return selected;
    }
    showNoSelectedLine( messageOver );
    return std::nullopt;
}

void FooterSession::showNoSelectedLine( QWidget* over )
{
    if ( noSelectedLine_ && noSelectedLine_->parentWidget() == over ) {
        noSelectedLine_->raise();
        noSelectedLine_->activateWindow();
        return;
    }
    delete noSelectedLine_.data();

    // The active file is the host's, pushed in: without one, no Log Line
    // can be selected.
    const bool noFile = activeFile_.isEmpty();
    QString text;
    if ( over && over == editor_.data() ) {
        // The window-modal editor blocks LogSquirl's window, and Cancel
        // would drop the user's edits.
        text = noFile ? QObject::tr( "No Log File is open in LogSquirl, and nothing can change "
                                     "there while the rule editor is open. Click OK to keep your "
                                     "rules and close the editor (Cancel discards them). Then "
                                     "open a Log File, select the Log Line with the "
                                     "value the rule should find, and choose Plugins → Custom "
                                     "Footer → Rule from Selected Line…" )
                      : QObject::tr( "No Log Line is selected in LogSquirl, and the selection "
                                     "cannot change while the rule editor is open. Click OK to "
                                     "keep your rules and close the editor (Cancel discards "
                                     "them). Then select the Log Line with the value the "
                                     "rule should find, and choose Plugins → Custom Footer → "
                                     "Rule from Selected Line…" );
    }
    else {
        text = noFile ? QObject::tr( "No Log File is open in LogSquirl. Open one, select the Log "
                                     "Line with the value the rule should find, and choose "
                                     "Plugins → Custom Footer → Rule from Selected Line… again." )
                      : QObject::tr( "No Log Line is selected in LogSquirl. Select the Log Line "
                                     "with the value the rule should find, and choose Plugins → "
                                     "Custom Footer → Rule from Selected Line… again." );
    }
    // Opened, never run in a nested event loop of its own: deleting the
    // session deletes the message.
    auto* box = new QMessageBox( QMessageBox::Information, QObject::tr( "Rule from Selected Line" ),
                                 text, QMessageBox::Ok, over );
    box->setObjectName( "noSelectedLineBox" );
    box->setAttribute( Qt::WA_DeleteOnClose );
    noSelectedLine_ = box;
    box->open();
}

bool FooterSession::has( HostFeature feature ) const noexcept
{
    switch ( feature ) {
    case HostFeature::RegexLab:
        return static_cast<bool>( host_.openRegexLab );
    case HostFeature::GoToLogLine:
        return static_cast<bool>( host_.goToLogLine );
    case HostFeature::SelectedLogLines:
        return static_cast<bool>( host_.selectedLogLines );
    case HostFeature::LogFileEncoding:
        return static_cast<bool>( host_.logFileEncoding );
    }
    return false;
}

void FooterSession::goToLine( const FooterValue& value ) noexcept
{
    guarded( host_, "going to a Log Line", [ this, &value ] {
        // On the UI thread, as the footer's signals are. The host checks the
        // line, which may be gone since the scan, e.g. after a truncation.
        if ( host_.goToLogLine( static_cast<std::uint64_t>( value.lineNumber ) ) ) {
            return;
        }
        if ( footer_ ) {
            footer_->notify( value.key, QCoreApplication::translate( "custom_footer::FooterSession",
                                                                     "Could not go to Log Line %1" )
                                            .arg( value.lineNumber ) );
        }
    } );
}

void FooterSession::saveFrom( const FooterEditor& editor ) noexcept
{
    guarded( host_, "saving the rules", [ this, &editor ] {
        if ( !FooterConfig::saveEntries( configDir_, editor.entries() ) ) {
            host_.logMessage(
                LOGSQUIRL_LOG_WARNING,
                QStringLiteral(
                    "Custom Footer: could not save the rules to the config directory" ) );
        }
        // Loaded again all the same: the footer shows what the file holds.
        controller_->reloadConfig();
    } );
}

} // namespace custom_footer
