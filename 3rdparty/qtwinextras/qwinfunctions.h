/****************************************************************************
**
** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWinExtras module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWINFUNCTIONS_H
#define QWINFUNCTIONS_H

#if 0
#pragma qt_class(QtWin)
#endif

#include <QtCore/qobject.h>
#include <QtCore/qt_windows.h>
#ifdef QT_WIDGETS_LIB
#include <QtWidgets/qwidget.h>
#endif

QT_BEGIN_NAMESPACE

class QWindow;
class QString;
class QMargins;

namespace QtWin
{
    QString stringFromHresult(HRESULT hresult);
    QString errorStringFromHresult(HRESULT hresult);

    void taskbarActivateTab(QWindow *);
    void taskbarActivateTabAlt(QWindow *);
    void taskbarAddTab(QWindow *);
    void taskbarDeleteTab(QWindow *);

#ifdef QT_WIDGETS_LIB

    inline void taskbarActivateTab(QWidget *window)
    {
        window->createWinId();
        taskbarActivateTab(window->windowHandle());
    }

    inline void taskbarActivateTabAlt(QWidget *window)
    {
        window->createWinId();
        taskbarActivateTabAlt(window->windowHandle());
    }

    inline void taskbarAddTab(QWidget *window)
    {
        window->createWinId();
        taskbarAddTab(window->windowHandle());
    }

    inline void taskbarDeleteTab(QWidget *window)
    {
        window->createWinId();
        taskbarDeleteTab(window->windowHandle());
    }
#endif // QT_WIDGETS_LIB
}

QT_END_NAMESPACE

#endif // QWINFUNCTIONS_H
