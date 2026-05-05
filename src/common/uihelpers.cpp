// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QCoreApplication>
#include <QFileInfo>

#include <QCoro/QCoroCore>

#include "common/config.h"
#include "uihelpers.h"


static QString sanitizeFileName(const QString &name)
{
    static QVector<char> illegal { '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
    QString result;

    for (auto c : name) {
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

UIHelpers *UIHelpers::inst()
{
    return s_inst;
}

QCoro::Task<std::optional<QString> > UIHelpers::getFileNameHelper(bool doSave, QString fileName,
                                                                  QString fileTitle,
                                                                  QList<QPair<QString, QStringList>> filters,
                                                                  QString title)
{
    QString fn = Config::inst()->lastDirectory();

    QStringList nameFilters;
    nameFilters.reserve(filters.size());
    for (const auto &[filterName, filterSuffixes] : std::as_const(filters))
        nameFilters.append(filterName + u" (*." + filterSuffixes.join(u" *.") + u')');
    if (!doSave)
        nameFilters.append(tr("All Files") + u" (*)");

    if (doSave) {
        if (!fileName.isEmpty())
            fn = fileName;
        else if (!fileTitle.isEmpty())
            fn = fn + u'/' + sanitizeFileName(fileTitle);
    }

    auto result = co_await getFileName(doSave, fn, nameFilters, title);

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
