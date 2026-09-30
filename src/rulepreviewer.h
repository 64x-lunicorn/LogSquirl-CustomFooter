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

#include "footerentry.h"
#include "footerscanner.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QThreadPool>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

class QFileSystemWatcher;
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
 * The active file is watched: when it changes on disk, e.g. a growing log,
 * the preview is made again, after a pause that collects the changes. A
 * running preview of the file is not cancelled for that; it finishes, and
 * one more preview then covers the changes made meanwhile.
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
    using PreviewFunction
        = std::function<Preview( const QString& filePath, const QList<FooterEntry>& entries,
                                 int rule, int maxLines, const std::atomic_bool* cancelled )>;

    /// When a requested preview starts.
    enum class Start {
        AfterPause, ///< Once no other request came for kDelayMs, e.g. while typing.
        Soon,       ///< With the next pass of the event loop, e.g. for another rule.
    };

    /// The pause after the last edit before the preview starts.
    static constexpr int kDelayMs = 300;
    /// The pause collecting changes of the active file before it is previewed again.
    static constexpr int kRescanDelayMs = 500;

    explicit RulePreviewer( QObject* parent = nullptr );
    ~RulePreviewer() override;

    /// The file to preview against; empty for none. The same file again
    /// counts as a change of it, which does not cancel a running preview.
    void setActiveFile( const QString& filePath );
    const QString& activeFile() const
    {
        return activeFile_;
    }

    /// The scan's line limit, as configured for the footer. A new limit
    /// previews the request again.
    void setMaxLines( int maxLines );

    /// The pauses, kDelayMs and kRescanDelayMs by default; tests set 0.
    void setDelays( int editMs, int fileChangeMs );

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
        return previewsStarted_;
    }
    int previewsFinished() const
    {
        return previewsFinished_;
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

    /// Preview the current request now.
    void start();
    /// The active file changed on disk.
    void fileChanged();
    void watchActiveFile();
    /// Make the running preview, if any, outdated.
    void dropRunning();

    QString activeFile_;
    int maxLines_ = FooterScanner::kDefaultMaxLines;
    int editDelayMs_ = kDelayMs;
    PreviewFunction previewFunction_;

    /// The latest request, kept to preview it again when the file changes.
    std::optional<Request> request_;
    /// A start of request_ is scheduled; soon rather than after the pause.
    bool startPending_ = false;
    bool startSoon_ = false;
    QTimer* startTimer_ = nullptr;

    QFileSystemWatcher* fileWatcher_ = nullptr;
    QTimer* rescanTimer_ = nullptr;
    /// The file changed while its preview was running: preview it again.
    bool rerun_ = false;

    /// Counts requests; a result is emitted only for the latest one.
    quint64 generation_ = 0;
    bool running_ = false;
    std::shared_ptr<std::atomic_bool> cancelRunning_;
    int previewsStarted_ = 0;
    int previewsFinished_ = 0;

    /// One worker thread: a new preview waits for the cancelled one to stop.
    QThreadPool pool_;
};

} // namespace custom_footer
