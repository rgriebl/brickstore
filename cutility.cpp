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

#include <math.h>

#include <QColor>
#include <QDesktopWidget>
#include <QApplication>
#include <QDir>
#include <QWidget>

#if defined ( Q_WS_X11 ) || defined ( Q_WS_MACX )
#include <stdlib.h>
#endif

#include "cutility.h"


QString CUtility::ellipsisText(const QString &org, const QFontMetrics &fm, int width, int align)
{
    int ellWidth = fm.width("...");
    QString text = QString::fromLatin1("");
    int i = 0;
    int len = org.length();
    int offset = (align & Qt::AlignRight) ? (len - 1) - i : i;

    while (i < len && fm.width(text + org [offset]) + ellWidth < width) {
        if (align & Qt::AlignRight)
            text.prepend(org [offset]);
        else
            text += org [offset];
        offset = (align & Qt::AlignRight) ? (len - 1) - ++i : ++i;
    }

    if (text.isEmpty())
        text = (align & Qt::AlignRight) ? org.right(1) : text = org.left(1);

    if (align & Qt::AlignRight)
        text.prepend("...");
    else
        text += "...";
    return text;
}

float CUtility::colorDifference(const QColor &c1, const QColor &c2)
{
    qreal r1, g1, b1, a1, r2, g2, b2, a2;
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    return (qAbs(r1-r2) + qAbs(g1-g2) + qAbs(b1-b2)) / 3.f;
}

QColor CUtility::gradientColor(const QColor &c1, const QColor &c2, qreal f)
{
    qreal r1, g1, b1, a1, r2, g2, b2, a2;
    c1.getRgbF(&r1, &g1, &b1, &a1);
    c2.getRgbF(&r2, &g2, &b2, &a2);

    f = qMin(qMax(f, 0.0), 1.0);
    qreal e = 1.0 - f;

    return QColor::fromRgbF(r1 * e + r2 * f, g1 * e + g2 * f, b1 * e + b2 * f, a1 * e + a2 * f);
}

QColor CUtility::contrastColor(const QColor &c, qreal f)
{
    qreal h, s, v, a;
    c.getHsvF(&h, &s, &v, &a);

    f = qMin(qMax(f, 0.0), 1.0);

    v += f * ((v < 0.5) ? 1.0 : -1.0);
    v = qMin(qMax(v, 0.0), 1.0);

    return QColor::fromHsvF(h, s, v, a);
}

void CUtility::setPopupPos(QWidget *w, const QRect &pos)
{
    QSize sh = w->sizeHint();
    QSize desktop = qApp->desktop()->size();

    int x = pos.x() + (pos.width() - sh.width()) / 2;
    int y = pos.y() + pos.height();

    if ((y + sh.height()) > desktop.height()) {
        int d = w->frameSize().height() - w->size().height();
#if defined( Q_WS_X11 )
        if (d <= 0) {
            foreach (QWidget *tlw, qApp->topLevelWidgets()) {
                if (qobject_cast<QMainWindow *>(tlw)) {
                    d = tlw->frameSize().height() - tlw->size().height();
                    break;
                }
            }
        }     
#endif
        y = pos.y() - sh.height() - d;
    }
    if (y < 0)
        y = 0;

    if ((x + sh.width()) > desktop.width())
        x = desktop.width() - sh.width();
    if (x < 0)
        x = 0;

    w->move(x, y);
    w->resize(sh);
}



QString CUtility::safeRename(const QString &basepath)
{
    QString error;
    QString newpath = basepath + ".new";
    QString oldpath = basepath + ".old";

    QDir cwd;

    if (cwd.exists(newpath)) {
        if (!cwd.exists(oldpath) || cwd.remove(oldpath)) {
            if (!cwd.exists(basepath) || cwd.rename(basepath, oldpath)) {
                if (cwd.rename(newpath, basepath))
                    error = QString();
                else
                    error = qApp->translate("CUtility", "Could not rename %1 to %2.").arg(newpath).arg(basepath);
            }
            else
                error = qApp->translate("CUtility", "Could not backup %1 to %2.").arg(basepath).arg(oldpath);
        }
        else
            error = qApp->translate("CUtility", "Could not delete %1.").arg(oldpath);
    }
    else
        error = qApp->translate("CUtility", "Could not find %1.").arg(newpath);

    return error;
}

