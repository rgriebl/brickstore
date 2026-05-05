// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QWidget>
#include <QPointer>
#include <QIcon>

QT_FORWARD_DECLARE_CLASS(QGroupBox)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QGraphicsOpacityEffect)
QT_FORWARD_DECLARE_CLASS(QPropertyAnimation)

class BetterCommandButton;


class WelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    WelcomeWidget(QWidget *parent = nullptr);
    ~WelcomeWidget() override;

    void fadeIn();
    void fadeOut();

protected:
    void changeEvent(QEvent *e) override;

private:
    void updateVersionsText();

    void languageChange();

private:
    void fade(bool in);

    QGroupBox *m_recent_frame;
    QGroupBox *m_document_frame;
    QGroupBox *m_import_frame;
    QPointer<QLabel> m_no_recent;
    QLabel *m_versions;
    QVector<QLabel *> m_links;
    QIcon m_docIcon;
    QIcon m_pinIcon;
    QIcon m_unpinIcon;
    QGraphicsOpacityEffect *m_effect;
    QPropertyAnimation *m_animation;
};
