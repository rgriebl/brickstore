// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include <QPixmap>

#include <QCoro/QCoroTask>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QTextBrowser)

class Announcements;

class AnnouncementsDialog : public QDialog
{
    Q_OBJECT
public:
    AnnouncementsDialog(const QString &markdown, QWidget *parent);

    static QCoro::Task<> showNewAnnouncements(Announcements *announcements, QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *pe) override;

private:
    void paletteChange();

    QPixmap m_stripes;
    QLabel *m_header;
    QTextBrowser *m_browser;
};
