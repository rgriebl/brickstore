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

#include <QDialog>
#include <QPixmap>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QTextBrowser)

class Announcements;

class AnnouncementsDialog : public QDialog
{
    Q_OBJECT
public:
    AnnouncementsDialog(const QString &markdown, QWidget *parent);

    static void showNewAnnouncements(Announcements *announcements, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *pe) override;

private:
    void sizeChange();
    void paletteChange();

    QPixmap m_stripes;
    QLabel *m_header;
    QTextBrowser *m_browser;
};
