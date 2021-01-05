/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QDialogButtonBox>
#include <QLayout>
#include <QApplication>

//#include "config.h"

#include "progressdialog.h"

ProgressDialog::ProgressDialog(const QString &title, Transfer *trans, QWidget *parent)
    : QDialog(parent)
{
    m_has_errors = false;
    m_autoclose = true;
    m_job = nullptr;
    m_trans = trans;

    connect(m_trans, &Transfer::finished, this, &ProgressDialog::transferDone);
    connect(m_trans, &Transfer::jobProgress, this, &ProgressDialog::transferProgress);

    m_message_progress = false;

    setWindowTitle(title);

    int minwidth = fontMetrics().horizontalAdvance('m') * 40;

    auto *lay = new QVBoxLayout(this);

    m_header = new QLabel(this);
    m_header->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_header->setMinimumWidth(minwidth);
    m_header->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    lay->addWidget(m_header);

    QFrame *frame = new QFrame(this);
    frame->setFrameStyle(QFrame::HLine | QFrame::Sunken);
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
    frame->setFrameStyle(QFrame::HLine | QFrame::Sunken);
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
        m_job->abort();

    QDialog::done(r);

    if (m_override)
        QApplication::restoreOverrideCursor();
}

void ProgressDialog::setHeaderText(const QString &str)
{
    m_header->setText(QString("<b>%1</b>").arg(str));
    syncRepaint(m_header);
}

void ProgressDialog::setMessageText(const QString &str)
{
    m_message_text = str;
    m_message_progress = str.contains("%p");

    if (m_message_progress)
        setProgress(0, 0);
    else
        m_message->setText(str);

    syncRepaint(m_message);
}

void ProgressDialog::setErrorText(const QString &str)
{
    m_message->setText(QString("<b>%1</b>: %2").arg(tr("Error"), str));
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
        
        if (str.contains("%p")) {
            QString prog;

            if (t)
                prog = QString("%1/%2 KB").arg(s).arg(t);
            else
                prog = QString("%1 KB").arg(s);

            str.replace("%p", prog);
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
    else
        emit transferFinished();
    m_job = nullptr;
}

bool ProgressDialog::hasErrors() const
{
    return m_has_errors;
}

bool ProgressDialog::post(const QUrl &url, QIODevice *file)
{
    m_job = TransferJob::post(url, file);
    m_trans->retrieve(m_job);
    return true;
}

bool ProgressDialog::get(const QUrl &url, const QDateTime &ifnewer, QIODevice *file)
{
    m_job = TransferJob::getIfNewer(url, ifnewer, file);
    m_trans->retrieve(m_job);
    return true;
}


void ProgressDialog::layout()
{
    QDialog::layout()->activate();

#if 0
    QSize now = size();
    QSize then = sizeHint();

    setFixedSize(QSize(qMax(now.width(), then.height()), qMax(now.height(), then.height())));
#else
    setFixedSize(sizeHint());
#endif
}

void ProgressDialog::setAutoClose(bool ac)
{
    m_autoclose = ac;
}


#include "moc_progressdialog.cpp"
