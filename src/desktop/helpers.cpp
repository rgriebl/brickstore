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
#include <QWidget>
#include <QWindow>
#include <QScreen>
#include <QApplication>

#include "utility/utility.h"
#include "helpers.h"


int Helpers::shouldSwitchViews(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) {
        if ((e->modifiers() & ~Qt::ShiftModifier) ==
        #if defined(Q_OS_MACOS)
                Qt::AltModifier
        #else
                Qt::ControlModifier
        #endif
                ) {
            return (e->key() == Qt::Key_Backtab) || (e->modifiers() & Qt::ShiftModifier) ? -1 : 1;
        }
    }
    return 0;
}

void Helpers::setPopupPos(QWidget *w, const QRect &pos)
{
    QSize sh = w->sizeHint();
    QRect screenRect = w->window()->windowHandle()->screen()->availableGeometry();

    // center below pos
    int x = pos.x() + (pos.width() - sh.width()) / 2;
    int y = pos.y() + pos.height();

    if ((y + sh.height()) > (screenRect.bottom() + 1)) {
        int frameHeight = (w->frameSize().height() - w->size().height());
        y = pos.y() - sh.height() - frameHeight;
    }
    if (y < screenRect.top())
        y = screenRect.top();

    if ((x + sh.width()) > (screenRect.right() + 1))
        x = (screenRect.right() + 1) - sh.width();
    if (x < screenRect.left())
        x = screenRect.left();

    w->move(x, y);
    w->resize(sh);
}


QString Helpers::toolTipLabel(const QString &label, QKeySequence shortcut, const QString &extended)
{
    return toolTipLabel(label, shortcut.isEmpty() ? QList<QKeySequence> { }
                                                  : QList<QKeySequence> { shortcut }, extended);
}

QString Helpers::toolTipLabel(const QString &label, const QList<QKeySequence> &shortcuts, const QString &extended)
{
#if defined(QT_WIDGETS_LIB)
    static const auto fmt = QString::fromLatin1(R"(<table><tr style="white-space: nowrap;"><td>%1</td><td align="right" valign="middle"><span style="color: %2; font-size: small;">&nbsp; &nbsp;%3</span></td></tr>%4</table>)");
    static const auto fmtExt = QString::fromLatin1(R"(<tr><td colspan="2">%1</td></tr>)");

    QColor color = Utility::gradientColor(Utility::premultiplyAlpha(QApplication::palette("QLabel").color(QPalette::Inactive, QPalette::ToolTipBase)),
                                          Utility::premultiplyAlpha(QApplication::palette("QLabel").color(QPalette::Inactive, QPalette::ToolTipText)),
                                          0.8);
    QString extendedTable;
    if (!extended.isEmpty())
        extendedTable = fmtExt.arg(extended);
    QString shortcutText;
    if (!shortcuts.isEmpty())
        shortcutText = QKeySequence::listToString(shortcuts, QKeySequence::NativeText);

    return fmt.arg(label, color.name(), shortcutText, extendedTable);
#else
    Q_UNUSED(shortcuts)
    Q_UNUSED(extended)
    return label;
#endif
}
