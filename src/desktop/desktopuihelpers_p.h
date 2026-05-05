// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QPropertyAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtCore/QPauseAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

#include <QCoro/QCoroSignal>

#include "common/uihelpers.h"


struct ForceableProgressDialog : public QProgressDialog {
    static void publicForceShow(QProgressDialog *pd) {
        (pd->*(&ForceableProgressDialog::forceShow))();
    }
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
        setAutoFillBackground(true);

        m_palette = palette();
        bool isDark = (m_palette.color(QPalette::Window).lightness() < 128);
        m_palette.setBrush(QPalette::WindowText, isDark ? Qt::black : Qt::white);
        m_palette.setBrush(QPalette::Window, QColor(isDark ? "#dddddd" : "#444444"));
        setPalette(m_palette);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(20, 10, 20, 10);
        auto *label = new QLabel(u"<b>" + message + u"</b>");
        label->setTextFormat(Qt::AutoText);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label, 1);
    }

    void open(const QRect &area)
    {
        auto fadeIn = new QVariantAnimation(this);
        fadeIn->setStartValue(0.);
        fadeIn->setEndValue(1.);
        fadeIn->setDuration(m_fadeTime);
        auto fadeOut = new QVariantAnimation(this);
        fadeOut->setStartValue(1.);
        fadeOut->setEndValue(0.);
        fadeOut->setDuration(m_fadeTime);

        auto fader = [this](const QVariant &v) {
            double f = v.toDouble();
            auto win = m_palette.color(QPalette::Window);
            auto txt = m_palette.color(QPalette::WindowText);
            win.setAlphaF(f);
            txt.setAlphaF(f);
            auto pal = m_palette;
            pal.setColor(QPalette::Window, win);
            pal.setColor(QPalette::WindowText, txt);
            setPalette(pal);
        };

        connect(fadeIn, &QVariantAnimation::valueChanged, this, fader);
        connect(fadeOut, &QVariantAnimation::valueChanged, this, fader);

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
        fader(0);
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
    QAbstractAnimation *m_animation = nullptr;
    int m_timeout = -1;
    QPalette m_palette;
    static constexpr int m_fadeTime = 1500;
};


class DesktopPDI : public UIHelpers_ProgressDialogInterface
{
    Q_OBJECT

public:
    DesktopPDI(const QString &title, const QString &message, QWidget *parent)
        : m_title(title)
        , m_message(message)
        , m_pd(new QProgressDialog(parent))
    {
        m_pd->setWindowModality(Qt::ApplicationModal);
        m_pd->setWindowTitle(title);
        m_pd->setLabelText(message);
        m_pd->setAutoReset(false);
        m_pd->setAutoClose(false);
        m_pd->setMinimumWidth(m_pd->fontMetrics().averageCharWidth() * 60);

        connect(m_pd, &QProgressDialog::canceled,
                this, [this]() {
            if (m_finishedSuccessfully) {
                m_pd->accept();
            } else {
                emit cancel();
                m_pd->reject();
            }
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
        int result = co_await qCoro(m_pd, &QProgressDialog::finished);
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
        if (!message.isEmpty()) {
            m_pd->setLabelText(message);
            m_pd->setCancelButtonText(QDialogButtonBox::tr("Ok"));
            m_finishedSuccessfully = success;
            ForceableProgressDialog::publicForceShow(m_pd);
            m_pd->setRange(0, 1);
            m_pd->setValue(success ? 1 : 0);
        } else {
            m_pd->reset();
            m_pd->done(success ? QDialog::Accepted : QDialog::Rejected);
        }
    }

private:
    QString m_title;
    QString m_message;
    QProgressDialog *m_pd;
    bool m_finishedSuccessfully = false;
};

