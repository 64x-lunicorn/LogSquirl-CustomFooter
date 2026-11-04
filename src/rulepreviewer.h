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

#pragma once

#include "filefollower.h"
#include "footerentry.h"
#include "footerscanner.h"

#include <QList>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <optional>

class QTimer;

namespace custom_footer {

/**
 * Runs the rule editor's live preview: FooterScanner::preview() of the
 * selected rule against the active file, on a worker thread.
 *
 * A request starts after a short pause, so typing does not start a scan per
 * key, or with the next pass of the event loop when it is urgent (another
 * rule, another file). Requests made before it starts are coalesced into
 * one. Every request makes earlier ones outdated: a running preview is
 * cancelled, and a result arriving for an outdated request is never
 * emitted, whatever order results arrive in. Without an active file the
 * preview is made on the GUI thread, as it reads nothing.
 *
 * The active file is followed by a FileFollower: when it changes on disk,
 * e.g. a growing log, or reappears after it was rotated away, the preview
 * is made again, after a pause that collects the changes. A running preview
 * of the file is not cancelled for that; it finishes, and one more preview
 * covers the changes made meanwhile, again only after the pause, so a busy
 * log is never scanned back to back. A change that cannot alter the shown
 * preview (FooterScanner::fileUnchangedFor()) is not previewed, and the
 * file is followed on.
 *
 * The file is read in the encoding the host reads it in, where the host
 * says; read in another one, it is previewed again at once.
 *
 * Destroying the previewer, or stop(), cancels a running preview, waits for
 * the worker, and leaves no timer running and no result pending: no code of
 * the plugin runs on its behalf afterwards.
 */
class RulePreviewer : public QObject {
    Q_OBJECT

public:
    using Preview = FooterScanner::Preview;

    /// Makes a preview on the worker thread; FooterScanner::preview() unless
    /// a test replaces it.
    using PreviewFunction = std::function<Preview(
        const QString& filePath, const QList<FooterEntry>& entries, int rule, int maxLines,
        const std::atomic_bool* cancelled, const QByteArray& hostEncoding )>;

    /// When a requested preview starts.
    enum class Start {
        AfterPause, ///< Once no other request came for kDelayMs, e.g. while typing.
        Soon,       ///< With the next pass of the event loop, e.g. for another rule.
    };

    /// The pause after the last edit before the preview starts.
    static constexpr int kDelayMs = 300;

    /// @p parts are the clock and change source of the active file's
    /// follower; tests pass their own.
    explicit RulePreviewer( QObject* parent = nullptr, FollowerParts parts = {} );
    ~RulePreviewer() override;

    /// The file to preview against; empty for none. @p hostEncoding is the
    /// name of the encoding the host reads it in, empty where the host does
    /// not say (FooterScanner::Encoding::forHost()). The same file in the
    /// same encoding again counts as a change of it, which does not cancel
    /// a running preview.
    void setActiveFile( const QString& filePath, const QByteArray& hostEncoding = {} );
    const QString& activeFile() const
    {
        return activeFile_;
    }
    /// The name of the encoding the active file is read in; empty where
    /// the host does not say.
    const QByteArray& activeFileEncoding() const
    {
        return activeFileEncoding_;
    }

    /// The scan's line limit, as configured for the footer. A new limit
    /// previews the request again.
    void setMaxLines( int maxLines );

    /// The pause after an edit, kDelayMs by default; tests set 0.
    void setEditDelay( int ms );

    /// Preview rule @p rule of @p entries instead of any earlier request.
    void schedule( const QList<FooterEntry>& entries, int rule, Start start = Start::AfterPause );

    /// Start the requested preview now, instead of after the pause.
    void flush();

    /// Forget the request, e.g. when no rule is selected, and drop scheduled
    /// and running previews without waiting for the worker.
    void clear();

    /// clear(), and wait for the worker: afterwards nothing of the previewer
    /// runs or is scheduled to run.
    void stop();

    /// Whether a preview is scheduled or running and not yet emitted.
    bool isBusy() const;

    /// How many previews were started on the worker thread, and how many of
    /// them finished, emitted or dropped as outdated; for tests.
    int previewsStarted() const
    {
        return follower_.status().started;
    }
    int previewsFinished() const
    {
        return follower_.status().finished;
    }

    void setPreviewFunction( PreviewFunction function );

Q_SIGNALS:
    /// A preview was requested; the shown one is outdated until previewed().
    void updating();

    /// The preview of the latest request.
    void previewed( const custom_footer::FooterScanner::Preview& preview );

private:
    struct Request {
        QList<FooterEntry> entries;
        int rule = -1;
    };

    /// Start the scheduled preview: its pause has passed, or flush().
    void startScheduled();
    /// Preview the current request now, and follow the active file with it.
    void start();

    QString activeFile_;
    QByteArray activeFileEncoding_;
    int maxLines_ = FooterScanner::kDefaultMaxLines;
    int editDelayMs_ = kDelayMs;
    PreviewFunction previewFunction_;

    /// The latest request, kept to preview it again when the file changes.
    std::optional<Request> request_;
    /// A start of request_ is scheduled (startTimer_ runs) soon rather than
    /// after the pause.
    bool startSoon_ = false;
    QTimer* startTimer_ = nullptr;

    /// The previews of the request against the active file, on a worker
    /// thread, followed when the file changes: only the latest request's
    /// result is emitted.
    FileFollower<Preview> follower_;
};

} // namespace custom_footer
