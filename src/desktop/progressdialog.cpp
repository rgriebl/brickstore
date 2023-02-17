// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QDialogButtonBox>
#include <QLayout>
#include <QApplication>

#include "progressdialog.h"


ProgressDialog::ProgressDialog(const QString &title, TransferJob *job, QWidget *parent)
    : QDialog(parent)
{
    m_has_errors = false;
    m_autoclose = true;
    m_job = job;

    m_message_progress = false;

    setWindowTitle(title);

    int minwidth = fontMetrics().horizontalAdvance(u'm') * 40;

    auto *lay = new QVBoxLayout(this);

    m_header = new QLabel(this);
    m_header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_header->setMinimumWidth(minwidth);
    m_header->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    lay->addWidget(m_header);

    QFrame *frame = new QFrame(this);
    frame->setFrameStyle(int(QFrame::HLine) | int(QFrame::Sunken));
    frame->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    lay->addWidget(frame);

    m_message = new QLabel(this);
    m_message->setAlignment(Qt::AlignLeft);
    m_message->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_message->setOpenExternalLinks(true);
    m_message->setTextInteractionFlags(Qt::TextBrowserInteraction);
    lay->addWidget(m_message);

    m_progress_container = new QWidget(this);
    lay->addWidget(m_progress_container);
    auto *play = new QHBoxLayout(m_progress_container);

    m_progress = new QProgressBar(m_progress_container);
    m_progress->setTextVisible(false);
    play->addSpacing(20);
    play->addWidget(m_progress);
    play->addSpacing(20);

    frame = new QFrame(this);
    frame->setFrameStyle(int(QFrame::HLine) | int(QFrame::Sunken));
    frame->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    lay->addWidget(frame);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, Qt::Horizontal, this);
    m_buttons->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    lay->addWidget(m_buttons);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    m_override = true;
}

void ProgressDialog::done(int r)
{
    if (m_job && m_job->isActive())
        m_job->transfer()->abortJob(m_job);

    QDialog::done(r);

    if (m_override)
        QApplication::restoreOverrideCursor();
}

void ProgressDialog::setHeaderText(const QString &str)
{
    m_header->setText(u"<b>" + str + u"</b>");
    syncRepaint(m_header);
}

void ProgressDialog::setMessageText(const QString &str)
{
    m_message_text = str;
    m_message_progress = str.contains(u"%p");

    if (m_message_progress)
        setProgress(0, 0);
    else
        m_message->setText(str);

    syncRepaint(m_message);
}

void ProgressDialog::setErrorText(const QString &str)
{
    m_message->setText(u"<b>" + tr("Error") + u"</b>: " + str);
    setFinished(false);

    syncRepaint(m_message);
}

void ProgressDialog::setFinished(bool ok)
{
    QApplication::restoreOverrideCursor();
    m_override = false;

    m_has_errors = !ok;
    m_progress->setMaximum(100);
    m_progress->setValue(100);

    if (m_autoclose && ok) {
        accept();
    }
    else {
        m_buttons->setStandardButtons(QDialogButtonBox::Ok);
        connect(m_buttons, &QDialogButtonBox::accepted,
                this, ok ? &QDialog::accept : &QDialog::reject);
        layout();
    }
}

void ProgressDialog::setProgress(int s, int t)
{
    m_progress->setMaximum(t);
    m_progress->setValue(s);

    if (m_message_progress) {
        QString str = m_message_text;
        
        if (str.contains(u"%p")) {
            QString prog;

            if (t)
                prog = u"%1/%2 KB"_qs.arg(s).arg(t);
            else
                prog = u"%1 KB"_qs.arg(s);

            str.replace(u"%p"_qs, prog);
        }
        m_message->setText(str);
    }
}

void ProgressDialog::setProgressVisible(bool b)
{
    if (b)
        m_progress_container->show();
    else
        m_progress_container->hide();
}

void ProgressDialog::syncRepaint(QWidget *w)
{
    layout();
    w->repaint();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

TransferJob *ProgressDialog::job() const
{
    return m_job;
}

void ProgressDialog::transferProgress(TransferJob *j, int s, int t)
{
    if (j && (j == m_job))
        setProgress(s / 1024, t / 1024);
}

void ProgressDialog::transferDone(TransferJob *j)
{
    if (!j || (j != m_job))
        return;

    if (j->file() && j->file()->isOpen())
        j->file()->close();

    if (j->isFailed() || j->isAborted())
        setErrorText(tr("Download failed: %1").arg(j->errorString()));

    emit transferFinished();

    if (m_job == j) // if we didn't immediately get a new job, then clear the pointer
        m_job = nullptr;
}

bool ProgressDialog::hasErrors() const
{
    return m_has_errors;
}

void ProgressDialog::layout()
{
    QDialog::layout()->activate();

#if 0
    QSize now = size();
    QSize then = sizeHint();

    setFixedSize(QSize(std::max(now.width(), then.height()), std::max(now.height(), then.height())));
#else
    setFixedSize(sizeHint());
#endif
}

void ProgressDialog::setAutoClose(bool ac)
{
    m_autoclose = ac;
}


#include "moc_progressdialog.cpp"
