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

#include <QQmlApplicationEngine>
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickdialog_p.h>

#include "common/uihelpers.h"


class MobilePDI : public UIHelpers_ProgressDialogInterface
{
    Q_OBJECT

public:
    MobilePDI(const QString &title, const QString &message, QQmlApplicationEngine *engine)
        : m_title(title)
        , m_message(message)
        , m_engine(engine)
    {
    }

    ~MobilePDI() override
    {
    }

    QCoro::Task<bool> exec() override
    {
        auto root = qobject_cast<QQuickApplicationWindow *>(m_engine->rootObjects().constFirst());

        QQmlComponent component(m_engine, m_engine->baseUrl().resolved(QUrl(u"Mobile/ProgressDialog.qml"_qs)));
        if (!component.isReady()) {
            qWarning() << component.errorString();
            co_return false;
        }
        QVariantMap properties = {
            { u"title"_qs, m_title },
            { u"text"_qs, m_message },
        };
        m_dialog = qobject_cast<QQuickDialog *>(
                    component.createWithInitialProperties(properties, qmlContext(root)));
        if (!m_dialog) {
            qWarning() << "Component create failed" << component.errors();
            co_return false;
        }

        m_dialog->setParentItem(root->contentItem());

        emit start();

        connect(m_dialog.get(), SIGNAL(requestCancel()), this, SIGNAL(cancel()));

        m_dialog->open();
        co_await qCoro(m_dialog.get(), &QQuickDialog::closed);
        m_dialog->deleteLater();
        co_return (m_dialog->result() == QQuickDialog::Accepted);
    }

    void progress(int done, int total) override
    {
        if (total <= 0) {
            m_dialog->setProperty("total", 0);
            m_dialog->setProperty("done", 0);
        } else {
            m_dialog->setProperty("total", total);
            m_dialog->setProperty("done", done);
        }
    }

    void finished(bool success, const QString &message) override
    {
        bool showMessage = !message.isEmpty();
        if (showMessage) {
            m_dialog->setProperty("text", message);
            m_dialog->setProperty("total", 100);
            m_dialog->setProperty("done", 100);
            // adding new buttons to a QDialogButtonBox crashes Qt 6.2.1
            m_dialog->setStandardButtons(QPlatformDialogHelper::Ok);
            disconnect(m_dialog.get(), SIGNAL(requestCancel()), this, nullptr);
            connect(m_dialog.get(), SIGNAL(requestCancel()), m_dialog.get(), SLOT(reject()));
        } else {
            m_dialog->done(success ? QQuickDialog::Accepted : QQuickDialog::Rejected);
        }
    }

private:
    QString m_title;
    QString m_message;
    QQmlApplicationEngine *m_engine;
    QPointer<QQuickDialog> m_dialog;
};

