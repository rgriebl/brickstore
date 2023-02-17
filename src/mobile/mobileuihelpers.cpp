// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QQmlApplicationEngine>
#include <QEventLoop>
#include <QStandardPaths>
#if defined(Q_CC_MSVC)
#  pragma warning(push)
#  pragma warning(disable: 4458)
#  pragma warning(disable: 4201)
#endif
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickdialog_p.h>
#include <QtQuickDialogs2/private/qquickfiledialog_p.h>
#include <QtQuickDialogs2/private/qquickmessagedialog_p.h>
#include <QtQuickDialogs2/private/qquickcolordialog_p.h>
#if defined(Q_CC_MSVC)
#  pragma warning(pop)
#endif

#include "mobileuihelpers.h"
#include "mobileuihelpers_p.h"


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
                                                               { u"title"_qs, title },
                                                               { u"text"_qs, msg },
                                                               { u"buttons"_qs, int(buttons) }
                                                           });

    if (!messageDialog)
        co_return defaultButton;

    auto [button, role] = co_await qCoro(messageDialog, &QQuickMessageDialog::buttonClicked);

    Q_UNUSED(role)
    messageDialog->deleteLater();
    co_return static_cast<StandardButton>(button);
}

QCoro::Task<std::optional<QString>> MobileUIHelpers::getInputString(QString text,
                                                                    QString initialValue,
                                                                    bool isPassword,
                                                                    QString title)
{
    auto dialog = createPopup<QQuickDialog>(s_engine, u"Mobile/InputDialog.qml"_qs, {
                                                { u"title"_qs, title },
                                                { u"text"_qs, text },
                                                { u"mode"_qs, u"string"_qs },
                                                { u"textValue"_qs, initialValue },
                                                { u"isPassword"_qs, isPassword },
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
    auto dialog = createPopup<QQuickDialog>(s_engine, u"Mobile/InputDialog.qml"_qs, {
                                                { u"title"_qs, title },
                                                { u"text"_qs, text },
                                                { u"mode"_qs, u"double"_qs },
                                                { u"unit"_qs, unit },
                                                { u"doubleValue"_qs, initialValue },
                                                { u"doubleMinimum"_qs, minValue },
                                                { u"doubleMaximum"_qs, maxValue },
                                                { u"doubleDecimals"_qs, decimals },
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
    auto dialog = createPopup<QQuickDialog>(s_engine, u"Mobile/InputDialog.qml"_qs, {
                                                { u"title"_qs, title },
                                                { u"text"_qs, text },
                                                { u"mode"_qs, u"int"_qs },
                                                { u"unit"_qs, unit },
                                                { u"intValue"_qs, initialValue },
                                                { u"intMinimum"_qs, minValue },
                                                { u"intMaximum"_qs, maxValue },
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
    auto colorDialog = createDialog<QQuickColorDialog>(s_engine, {
                                                           { u"title"_qs, title },
                                                           { u"selectedColor"_qs, initialColor },
                                                       });
    if (!colorDialog)
        co_return { };

    co_await qCoro(colorDialog, &QQuickColorDialog::resultChanged);
    colorDialog->deleteLater();
    qWarning() << colorDialog->result() << QQuickMessageDialog::Accepted << colorDialog->selectedColor();
    if (colorDialog->result() == QQuickMessageDialog::Accepted)
        co_return colorDialog->selectedColor();
    else
        co_return { };
}

QCoro::Task<std::optional<QString>> MobileUIHelpers::getFileName(bool doSave, QString fileName,
                                                                 QStringList filters,
                                                                 QString title)
{
#if defined(Q_OS_IOS)
    if (doSave) {  // QTBUG-101301
        fileName = QFileInfo(fileName).fileName();

        if (auto fn = co_await getInputString(tr("Save File as"), fileName, false, title))
           co_return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + u'/' + *fn;
        co_return { };
    }
#endif

    auto fileDialog = createDialog<QQuickFileDialog>(s_engine, {
                                                         { u"fileMode"_qs, doSave ? QQuickFileDialog::SaveFile : QQuickFileDialog::OpenFile },
#if defined(Q_OS_ANDROID)
                                                         // QTBUG-96655
                                                         { u"title"_qs, fileName },
#else
                                                         { u"title"_qs, title },
                                                         { u"selectedFile"_qs, fileName },
#endif
                                                         { u"nameFilters"_qs, filters },
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

UIHelpers_ProgressDialogInterface *MobileUIHelpers::createProgressDialog(const QString &title, const QString &message)
{
    return new MobilePDI(title, message, s_engine);
}

void MobileUIHelpers::processToastMessages()
{
    if (m_toastMessageVisible || m_toastMessages.isEmpty())
        return;

    auto [message, timeout] = m_toastMessages.takeFirst();

    auto toast = createPopup<QQuickPopup>(s_engine, u"Mobile/ToastMessage.qml"_qs, {
                                             { u"message"_qs, message },
                                             { u"timeout"_qs, timeout },
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
#include "moc_mobileuihelpers_p.cpp"
