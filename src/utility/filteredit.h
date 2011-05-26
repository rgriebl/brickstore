/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __FILTEREDIT_H__
#define __FILTEREDIT_H__

#include <QLineEdit>

class QMenu;
class FilterEditButton;

class FilterEdit : public QLineEdit {
    Q_OBJECT

public:
    FilterEdit(QWidget *parent = 0);

    void setMenu(QMenu *);
    QMenu *menu() const;

protected:
    void resizeEvent(QResizeEvent *e);

private slots:
    void checkText(const QString &);

private:
    void doLayout();

    FilterEditButton *w_menu;
    FilterEditButton *w_clear;
    int               m_left;
    int               m_top;
    int               m_right;
    int               m_bottom;

#if (QT_VERSION < 0x407000) && !defined(Q_WS_MAEMO_5)
public:
    QString placeholderText() const;

public slots:
    void setPlaceholderText(const QString &str);

protected:
    void paintEvent(QPaintEvent *e);
    void focusInEvent(QFocusEvent *e);
    void focusOutEvent(QFocusEvent *e);

private:
    QString m_placeholdertext;
#endif
};

#endif
