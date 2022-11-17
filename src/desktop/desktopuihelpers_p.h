/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include <QtCore/QPropertyAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtCore/QPauseAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

#include "common/uihelpers.h"


class ForceableProgressDialog : public QProgressDialog
{
public:
    ForceableProgressDialog(QWidget *parent)
        : QProgressDialog(parent)
    { }

    // why is forceShow() protected?
    using QProgressDialog::forceShow;
};

class ToastMessage : public QWidget
{
    Q_OBJECT
public:
    ToastMessage(const QString &message, int timeout)
        : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
        , m_message(message)
        , m_timeout((timeout < 3000) ? 3000 : timeout)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);

        QPalette pal = palette();
        bool isDark = (pal.color(QPalette::Window).lightness() < 128);
        pal.setBrush(QPalette::WindowText, isDark ? Qt::black : Qt::white);
        pal.setBrush(QPalette::Window, QColor(isDark ? "#dddddd" : "#444444"));
        setPalette(pal);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(20, 10, 20, 10);
        auto *label = new QLabel(u"<b>" % message % u"</b>");
        label->setTextFormat(Qt::AutoText);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label, 1);
    }

    void open(const QRect &area)
    {
        auto fadeIn = new QPropertyAnimation(this, "windowOpacity");
        fadeIn->setStartValue(0.);
        fadeIn->setEndValue(1.);
        fadeIn->setDuration(m_fadeTime);
        auto fadeOut = new QPropertyAnimation(this, "windowOpacity");
        fadeOut->setEndValue(0.);
        fadeOut->setDuration(m_fadeTime);
        auto seq = new QSequentialAnimationGroup(this);
        seq->addAnimation(fadeIn);
        seq->addPause(m_timeout);
        seq->addAnimation(fadeOut);
        connect(seq, &QAnimationGroup::finished, this, &ToastMessage::closed);
        m_animation = seq;

        resize(sizeHint());
        QPoint p = area.center() - rect().center();
        p.ry() += area.height() / 4;
        move(p);
        show();
        seq->start();
    }

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        qreal r = qreal(height()) / 3.;
        p.setBrush(palette().brush(QPalette::Window));
        p.drawRoundedRect(QRectF(rect().adjusted(0, 0, -1, -1)), r, r);
    }

    void mousePressEvent(QMouseEvent *) override
    {
        if (m_animation && (m_animation->state() == QAbstractAnimation::Running)) {
            int total = m_animation->totalDuration();
            if (m_animation->currentTime() < (total - m_fadeTime)) {
                m_animation->pause();
                m_animation->setCurrentTime(total - m_fadeTime);
                m_animation->resume();
            }
        }
    }

private:
    QString m_message;
    int m_timeout = -1;
    QAbstractAnimation *m_animation = nullptr;
    static constexpr int m_fadeTime = 1500;
};


class DesktopPDI : public UIHelpers_ProgressDialogInterface
{
    Q_OBJECT

public:
    DesktopPDI(const QString &title, const QString &message, QWidget *parent)
        : m_title(title)
        , m_message(message)
        , m_pd(new ForceableProgressDialog(parent))
    {
        m_pd->setWindowModality(Qt::ApplicationModal);
        m_pd->setWindowTitle(title);
        m_pd->setLabelText(message);
        m_pd->setAutoReset(false);
        m_pd->setAutoClose(false);
        m_pd->setMinimumWidth(m_pd->fontMetrics().averageCharWidth() * 60);

        connect(m_pd, &QProgressDialog::canceled,
                this, [this]() {
            emit cancel();
            m_pd->reject();
        });
    }

    ~DesktopPDI() override
    {
        m_pd->deleteLater();
    }

    QCoro::Task<bool> exec() override
    {
        emit start();
        m_pd->show();
        int result = co_await qCoro(m_pd, &ForceableProgressDialog::finished);
        co_return (result == QDialog::Accepted);
    }

    void progress(int done, int total) override
    {
        if (total <= 0) {
            m_pd->setMaximum(0);
            m_pd->setValue(0);
        } else {
            m_pd->setMaximum(total);
            m_pd->setValue(done);
        }
    }

    void finished(bool success, const QString &message) override
    {
        bool showMessage = !message.isEmpty();
        if (showMessage) {
            m_pd->setLabelText(message);
            m_pd->setCancelButtonText(QDialogButtonBox::tr("Ok"));
            m_pd->forceShow();
        }

        if (!showMessage) {
            m_pd->reset();
            m_pd->done(success ? QDialog::Accepted : QDialog::Rejected);
        }
    }

private:
    QString m_title;
    QString m_message;
    ForceableProgressDialog *m_pd;
};

