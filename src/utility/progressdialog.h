/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

#include <QDialog>

#include "transfer.h"

class QIODevice;
class QLabel;
class QProgressBar;
class QWidget;
class QDialogButtonBox;


class ProgressDialog : public QDialog
{
    Q_OBJECT
public:
    ProgressDialog(Transfer *trans, QWidget *parent = nullptr);
    virtual ~ProgressDialog();

    void setAutoClose(bool ac);
    void setHeaderText(const QString &str);
    void setMessageText(const QString &str);
    void setErrorText(const QString &str);
    void setFinished(bool ok);
    void setProgress(int steps, int total);
    void setProgressVisible(bool b);

    bool post(const QUrl &url, QIODevice *file = nullptr);
    bool get(const QUrl &url, const QDateTime &ifnewer = QDateTime(), QIODevice *file = nullptr);

    TransferJob *job() const;
    bool hasErrors() const;

    void layout();

signals:
    void transferFinished();

protected slots:
    void done(int r) override;

private slots:
    void transferProgress(ThreadPoolJob *job, int, int);
    void transferDone(ThreadPoolJob *job);

private:
    void syncRepaint(QWidget *w);

private:
    QLabel *m_header;
    QLabel *m_message;
    QProgressBar *m_progress;
    QWidget *m_progress_container;
    QDialogButtonBox *m_buttons;

    QString m_message_text;
    bool m_message_progress : 1;
    bool m_has_errors       : 1;
    bool m_autoclose        : 1;
    bool m_override         : 1;

    Transfer *m_trans;
    TransferJob *m_job;
};
