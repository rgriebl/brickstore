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
#include <QQmlApplicationEngine>
#include <QEventLoop>
#include <QStandardPaths>
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickdialog_p.h>
#include <QtQuickDialogs2/private/qquickfiledialog_p.h>
#include <QtQuickDialogs2/private/qquickmessagedialog_p.h>
//#include <QtQuickDialogs2/private/qquickcolordialog_p.h>

#include "utility/utility.h"
#include "mobileuihelpers.h"


static QObject *createItem(QQmlApplicationEngine *engine, const QString &qmlFile, const QVariantMap &properties)
{
    auto root = qobject_cast<QQuickApplicationWindow *>(engine->rootObjects().constFirst());

    QQmlComponent component(engine, engine->baseUrl().resolved(QUrl(qmlFile)));
    if (!component.isReady()) {
        qWarning() << component.errorString();
        return nullptr;
    }
    if (auto item = component.createWithInitialProperties(properties, qmlContext(root)))
        return item;
    else
        qWarning() << "Component create failed" << component.errors();
    return nullptr;
}

template <typename T> T *createPopup(QQmlApplicationEngine *engine, const QString &qmlFile, const QVariantMap &properties)
{
    if (auto item = createItem(engine, qmlFile, properties)) {
        if (auto popup = qobject_cast<T *>(item)) {
            auto root = qobject_cast<QQuickApplicationWindow *>(engine->rootObjects().constFirst());

            popup->setParentItem(root->contentItem());
            popup->open();
            return popup;
        } else {
            delete item;
            qWarning() << "Component did not create a Popup derived type";
        }
    }
    return nullptr;
}

template <typename T> T *createDialog(QQmlApplicationEngine *engine, const QVariantMap &properties)
{
    const QMetaObject *mo = &T::staticMetaObject;
    QByteArray name = mo->classInfo(mo->indexOfClassInfo("QML.Element")).value();
    if (name.isEmpty()) {
        qWarning() << "Requested Dialog class does not have a QML name";
        return { };
    }

    auto root = qobject_cast<QQuickApplicationWindow *>(engine->rootObjects().constFirst());
    if (!root) {
        qWarning() << "No root object to attach Dialog to";
        return { };
    }

    static QQmlComponent *component = nullptr;
    if (!component) {
        component = new QQmlComponent(engine);
        component->setData("import QtQuick.Dialogs as QD\nQD." + name + " { }\n", engine->baseUrl());
    }
    if (!component->isReady()) {
        qWarning() << component->errorString();
        return { };
    }

    auto item = component->createWithInitialProperties(properties, qmlContext(root));

    if (!item) {
        qWarning() << "Component create failed" << component->isError() << component->errors();
        return { };
    }

    auto dialog = qobject_cast<T *>(item);
    if (!dialog) {
        delete item;
        qWarning() << "Component did not create a" << name.constData();
        return { };
    }

    dialog->setParentWindow(root);
    dialog->open();
    return dialog;
}

QPointer<QQmlApplicationEngine> MobileUIHelpers::s_engine;

void MobileUIHelpers::create(QQmlApplicationEngine *engine)
{
    s_inst = new MobileUIHelpers();
    s_engine = engine;
}

MobileUIHelpers::MobileUIHelpers()
{ }


QCoro::Task<UIHelpers::StandardButton> MobileUIHelpers::showMessageBox(QString msg,
                                                                       UIHelpers::Icon icon,
                                                                       StandardButtons buttons,
                                                                       StandardButton defaultButton,
                                                                       QString title)
{
    Q_UNUSED(icon)

    auto messageDialog = createDialog<QQuickMessageDialog>(s_engine, {
                                                               { "title"_l1, title },
                                                               { "text"_l1, msg },
                                                               { "buttons"_l1, int(buttons) }
                                                           });

    if (!messageDialog)
        co_return defaultButton;

    auto [button, role] = co_await qCoro(messageDialog, &QQuickMessageDialog::buttonClicked);
    Q_UNUSED(role)
    messageDialog->deleteLater();
    if (messageDialog->result() == QQuickMessageDialog::Accepted)
        co_return static_cast<StandardButton>(button);
    else
        co_return defaultButton;
}

QCoro::Task<std::optional<QString>> MobileUIHelpers::getInputString(QString text,
                                                                    QString initialValue,
                                                                    QString title)
{
    auto dialog = createPopup<QQuickDialog>(s_engine, "uihelpers/InputDialog.qml"_l1, {
                                                { "title"_l1, title },
                                                { "text"_l1, text },
                                                { "mode"_l1, "string"_l1 },
                                                { "textValue"_l1, initialValue },
                                            });
    if (!dialog)
        co_return { };
    co_await qCoro(dialog, &QQuickDialog::closed);
    dialog->deleteLater();
    if (dialog->result() == QQuickDialog::Accepted)
        co_return dialog->property("textValue").toString();
    else
        co_return { };
}

QCoro::Task<std::optional<double>> MobileUIHelpers::getInputDouble(QString text,
                                                                   QString unit,
                                                                   double initialValue,
                                                                   double minValue, double maxValue,
                                                                   int decimals, QString title)
{
    auto dialog = createPopup<QQuickDialog>(s_engine, "uihelpers/InputDialog.qml"_l1, {
                                                { "title"_l1, title },
                                                { "text"_l1, text },
                                                { "mode"_l1, "double"_l1 },
                                                { "unit"_l1, unit },
                                                { "doubleValue"_l1, initialValue },
                                                { "doubleMinimum"_l1, minValue },
                                                { "doubleMaximum"_l1, maxValue },
                                                { "doubleDecimals"_l1, decimals },
                                            });
    if (!dialog)
        co_return { };
    co_await qCoro(dialog, &QQuickDialog::closed);
    dialog->deleteLater();
    if (dialog->result() == QQuickDialog::Accepted)
        co_return dialog->property("doubleValue").toDouble();
    else
        co_return { };
}

QCoro::Task<std::optional<int>> MobileUIHelpers::getInputInteger(QString text,
                                                                 QString unit,
                                                                 int initialValue, int minValue,
                                                                 int maxValue, QString title)
{
    auto dialog = createPopup<QQuickDialog>(s_engine, "uihelpers/InputDialog.qml"_l1, {
                                                { "title"_l1, title },
                                                { "text"_l1, text },
                                                { "mode"_l1, "int"_l1 },
                                                { "unit"_l1, unit },
                                                { "intValue"_l1, initialValue },
                                                { "intMinimum"_l1, minValue },
                                                { "intMaximum"_l1, maxValue },
                                            });
    if (!dialog)
        co_return { };
    co_await qCoro(dialog, &QQuickDialog::closed);
    dialog->deleteLater();
    if (dialog->result() == QQuickDialog::Accepted)
        co_return dialog->property("intValue").toInt();
    else
        co_return { };
}


QCoro::Task<std::optional<QColor>> MobileUIHelpers::getInputColor(QColor initialColor,
                                                                  QString title)
{
//    auto colorDialog = createDialog<QQuickColorDialog>(s_engine, {
//                                                           { "title"_l1, title },
//                                                           { "selectedColor"_l1, initialColor },
//                                                       });

//    if (!colorDialog)
//        co_return { };

//    co_await qCoro(colorDialog, &QQuickColorDialog::selectedColorChanged);
//    colorDialog->deleteLater();
//    if (colorDialog->result() == QQuickMessageDialog::Accepted)
//        co_return colorDialog->selectedColor();
//    else
        co_return { };
}

QCoro::Task<std::optional<QString>> MobileUIHelpers::getFileName(bool doSave, QString fileName,
                                                                 QStringList filters,
                                                                 QString title)
{
#if defined(Q_OS_IOS)
    if (doSave) {  // QTBUG-101301
        fileName = QFileInfo(fileName).fileName();

        if (auto fn = co_await getInputString(tr("Filename to save to"), fileName, title))
           co_return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) % u'/' % *fn;
        co_return { };
    }
#endif

    auto fileDialog = createDialog<QQuickFileDialog>(s_engine, {
                                                         { "fileMode"_l1, doSave ? QQuickFileDialog::SaveFile : QQuickFileDialog::OpenFile },
#if defined(Q_OS_ANDROID)
                                                         // QTBUG-96655
                                                         { "title"_l1, fileName },
#else
                                                         { "title"_l1, title },
                                                         { "currentFile"_l1, fileName },
#endif
                                                         { "nameFilters"_l1, filters },
                                                     });

    if (!fileDialog)
        co_return { };

    co_await qCoro(fileDialog, &QQuickFileDialog::resultChanged);
    fileDialog->deleteLater();
    qWarning() << fileDialog->result() << fileDialog->selectedFile() << fileDialog->selectedFiles() << fileDialog->selectedFile().toDisplayString();
    if (fileDialog->result() == QQuickFileDialog::Accepted) {
#if defined(Q_OS_ANDROID)
        co_return fileDialog->selectedFile().toString(); // content:// urls cannot be converted
#else
        co_return fileDialog->selectedFile().toLocalFile();
#endif
    } else {
        co_return { };
    }
}

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

        QQmlComponent component(m_engine, m_engine->baseUrl().resolved(QUrl("uihelpers/ProgressDialog.qml"_l1)));
        if (!component.isReady()) {
            qWarning() << component.errorString();
            co_return false;
        }
        QVariantMap properties = {
            { "title"_l1, m_title },
            { "text"_l1, m_message },
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
            // adding new buttons to a QDialogButtonBox crashes Qt 6.2.1
            // m_dialog->setStandardButtons(QPlatformDialogHelper::Ok);
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

UIHelpers_ProgressDialogInterface *MobileUIHelpers::createProgressDialog(const QString &title, const QString &message)
{
    return new MobilePDI(title, message, s_engine);
}

void MobileUIHelpers::processToastMessages()
{
    if (m_toastMessageVisible || m_toastMessages.isEmpty())
        return;

    auto [message, timeout] = m_toastMessages.takeFirst();

    auto toast = createPopup<QQuickPopup>(s_engine, "uihelpers/ToastMessage.qml"_l1, {
                                             { "message"_l1, message },
                                             { "timeout"_l1, timeout },
                                         });
    if (!toast)
        return;
    m_toastMessageVisible = true;

    connect(toast, &QQuickPopup::closed,
            this, [this, toast]() {
        toast->deleteLater();
        m_toastMessageVisible = false;
        processToastMessages();
    }, Qt::QueuedConnection);
}


#include "moc_mobileuihelpers.cpp"
#include "mobileuihelpers.moc"
