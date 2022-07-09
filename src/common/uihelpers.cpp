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
#include <QCoreApplication>
#include <QFileInfo>

#include "utility/utility.h"
#include "common/config.h"
#include "uihelpers.h"


static QString sanitizeFileName(const QString &name)
{
    static QVector<char> illegal { '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
    QString result;

    for (int i = 0; i < name.size(); ++i) {
        auto c = name.at(i);
        auto u = c.unicode();

        if ((u <= 31) || ((u < 128) && illegal.contains(char(u))))
            c = u'_';

        result.append(c);
    }
    return result.simplified();
}



UIHelpers *UIHelpers::s_inst = nullptr;


QString UIHelpers::defaultTitle()
{
    return QCoreApplication::applicationName();
}

QCoro::Task<UIHelpers::StandardButton> UIHelpers::showMessageBoxHelper(QString msg, Icon icon,
                                                                       StandardButtons buttons,
                                                                       StandardButton defaultButton,
                                                                       QString title)
{
    // we don't want more than 1 message box visible at any time
    while (m_messageBoxCount > 0)
        co_await qCoro(this, &UIHelpers::messageBoxClosed);

    ++m_messageBoxCount;
    auto result = co_await showMessageBox(msg, icon, buttons, defaultButton, title);
    --m_messageBoxCount;
    QMetaObject::invokeMethod(this, &UIHelpers::messageBoxClosed);
    co_return result;
}

UIHelpers::UIHelpers()

{ }

UIHelpers *UIHelpers::inst()
{
    return s_inst;
}

QCoro::Task<std::optional<QString> > UIHelpers::getFileNameHelper(bool doSave, QString fileName,
                                                                  QString fileTitle,
                                                                  QStringList filters,
                                                                  QString title)
{
    QString fn = Config::inst()->lastDirectory();

    if (doSave) {
        if (!fileName.isEmpty())
            fn = fileName;
        else if (!fileTitle.isEmpty())
            fn = fn % u'/' % sanitizeFileName(fileTitle);
    }

    auto result = co_await getFileName(doSave, fn, filters, title);

    if (result && !result->isEmpty()) {
        fn = *result;
        Config::inst()->setLastDirectory(QFileInfo(fn).absolutePath());
        co_return fn;
    }
    co_return { };
}

void UIHelpers::showToastMessageHelper(const QString &message, int timeout)
{
    m_toastMessages.push_back(qMakePair(message, timeout));
    processToastMessages();
}

#include "moc_uihelpers.cpp"
