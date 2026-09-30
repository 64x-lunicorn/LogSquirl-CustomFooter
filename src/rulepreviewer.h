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
#include <QTimer>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace custom_footer {

/**
 * Runs the rule editor's live preview: FooterScanner::preview() of the
 * selected rule against the active file, on a worker thread.
 *
 * A request is started after a short pause, so typing does not start a scan
 * per key. Every request makes earlier ones outdated: a running preview is
 * cancelled, and a result arriving for an outdated request is never emitted,
 * whatever order results arrive in. Without an active file, the preview is
 * made on the calling thread, as it reads nothing.
 *
 * Destroying the previewer, or stop(), cancels a running preview and waits
 * for the worker: no code of the plugin runs on it afterwards.
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

    /// The pause after the last request before the preview starts.
    static constexpr int kDelayMs = 300;

    explicit RulePreviewer( QObject* parent = nullptr );
    ~RulePreviewer() override;

    /// The file to preview against; empty for none.
    void setActiveFile( const QString& filePath );
    const QString& activeFile() const
    {
        return activeFile_;
    }

    /// The scan's line limit, as configured for the footer.
    void setMaxLines( int maxLines );
    int maxLines() const
    {
        return maxLines_;
    }

    /// Preview rule @p rule of @p entries after kDelayMs, instead of any
    /// earlier request.
    void schedule( const QList<FooterEntry>& entries, int rule );

    /// Start the scheduled preview now instead of after the pause.
    void flush();

    /// Drop scheduled and running previews without waiting for the worker.
    void cancel();

    /// Drop scheduled and running previews, and wait for the worker.
    void stop();

    /// Whether a preview is scheduled or running and not yet emitted.
    bool isBusy() const
    {
        return pending_.has_value() || running_;
    }

    /// How many previews were started on the worker thread.
    int previewsStarted() const
    {
        return previewsStarted_;
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

    /// Make the running preview, if any, outdated.
    void dropRunning();

    QString activeFile_;
    int maxLines_ = FooterScanner::kDefaultMaxLines;
    PreviewFunction previewFunction_;

    std::optional<Request> pending_;
    QTimer delay_;

    /// Counts requests; a result is emitted only for the latest one.
    quint64 generation_ = 0;
    bool running_ = false;
    std::shared_ptr<std::atomic_bool> cancelRunning_;
    int previewsStarted_ = 0;

    /// One worker thread: a new preview waits for the cancelled one to stop.
    QThreadPool pool_;
};

} // namespace custom_footer
