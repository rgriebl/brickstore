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
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickdialog_p.h>
#include <QtQuickDialogs2/private/qquickfiledialog_p.h>

#include "utility/utility.h"
#include "mobileuihelpers.h"


QPointer<QQmlApplicationEngine> MobileUIHelpers::s_engine;

void MobileUIHelpers::create(QQmlApplicationEngine *engine)
{
    s_inst = new MobileUIHelpers();
    s_engine = engine;
}

MobileUIHelpers::MobileUIHelpers()
{ }

QObject *MobileUIHelpers::createItem(const QString &qmlFile, const QVariantMap &properties)
{
    auto root = qobject_cast<QQuickApplicationWindow *>(s_engine->rootObjects().constFirst());

    QQmlComponent component(s_engine, s_engine->baseUrl().resolved(QUrl(qmlFile)));
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

QQuickDialog *MobileUIHelpers::createDialog(const QString &qmlFile, const QVariantMap &properties)
{
    if (auto item = createItem(qmlFile, properties)) {
        if (auto dialog = qobject_cast<QQuickDialog *>(item)) {
            auto root = qobject_cast<QQuickApplicationWindow *>(s_engine->rootObjects().constFirst());

            dialog->setParentItem(root->contentItem());
            dialog->open();
            return dialog;
        } else {
            delete item;
            qWarning() << "Component did not create a Dialog";
        }
    }
    return nullptr;
}

QCoro::Task<UIHelpers::StandardButton> MobileUIHelpers::showMessageBox(QString msg,
                                                                       UIHelpers::Icon icon,
                                                                       StandardButtons buttons,
                                                                       StandardButton defaultButton,
                                                                       QString title)
{
    Q_UNUSED(icon)

    auto dialog = createDialog("uihelpers/MessageBox.qml"_l1, {
                                   { "title"_l1, title },
                                   { "text"_l1, msg },
                                   { "standardButtons"_l1, int(buttons) },
                               });
    if (!dialog)
        co_return defaultButton;

    co_await qCoro(dialog, &QQuickDialog::closed);
    dialog->deleteLater();
    co_return static_cast<StandardButton>(dialog->property("clickedButton").toInt());
}

QCoro::Task<std::optional<QString>> MobileUIHelpers::getInputString(QString text,
                                                                    QString initialValue,
                                                                    QString title)
{
    auto dialog = createDialog("uihelpers/InputDialog.qml"_l1, {
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
    auto dialog = createDialog("uihelpers/InputDialog.qml"_l1, {
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
    auto dialog = createDialog("uihelpers/InputDialog.qml"_l1, {
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
    //TODO: port Qt5's DefaultColorDialog.qml to QQC2 and use it here
    co_return { };
}

QCoro::Task<std::optional<QString>> MobileUIHelpers::getFileName(bool doSave, QString fileName,
                                                                 QStringList filters,
                                                                 QString title)
{    
    auto item = createItem("uihelpers/FileDialog.qml"_l1, {
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
    if (!item)
        co_return { };

    auto fileDialog = qobject_cast<QQuickFileDialog *>(item);
    if (!fileDialog) {
        delete item;
        qWarning() << "Component did not create a FileDialog";
        co_return { };
    }

    auto root = qobject_cast<QQuickApplicationWindow *>(s_engine->rootObjects().constFirst());
    fileDialog->setParentWindow(root);
    fileDialog->open();

    co_await qCoro(fileDialog, &QQuickFileDialog::resultChanged);
    fileDialog->deleteLater();
    qWarning() << fileDialog->result() << fileDialog->selectedFile() << fileDialog->selectedFiles();
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
            // m_dialog->setStandardButtons(QPlatformColorDialogHelper::Ok);
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


#include "moc_mobileuihelpers.cpp"
#include "mobileuihelpers.moc"
