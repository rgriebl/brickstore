// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

#include "utility/transfer.h"

QT_FORWARD_DECLARE_CLASS(QIODevice)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QWidget)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)


class ProgressDialog : public QDialog
{
    Q_OBJECT
public:
    ProgressDialog(const QString &title, TransferJob *job, QWidget *parent = nullptr);

    void setAutoClose(bool ac);
    void setHeaderText(const QString &str);
    void setMessageText(const QString &str);
    void setErrorText(const QString &str);
    void setFinished(bool ok);
    void setProgress(int steps, int total);
    void setProgressVisible(bool b);

    TransferJob *job() const;
    bool hasErrors() const;

    void layout();

signals:
    void transferFinished();

protected slots:
    void done(int r) override;

public slots:
    void transferProgress(TransferJob *job, int, int);
    void transferDone(TransferJob *j);

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

    TransferJob *m_job;
};
