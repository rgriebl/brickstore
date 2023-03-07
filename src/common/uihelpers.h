// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <limits>

#include <QObject>
#include <QColor>

#include <QCoro/QCoroTask>


inline QString CMB_BOLD(const QString &str)
{
    return u"<b>" + str + u"</b>";
}

class UIHelpers_ProgressDialogInterface : public QObject
{
    Q_OBJECT

public:
    virtual QCoro::Task<bool> exec() = 0;

    virtual void progress(int, int) = 0;
    virtual void finished(bool, const QString &) = 0;

signals:
    void cancel();
    void start();
};


class UIHelpers : public QObject
{
    Q_OBJECT
public:
    enum Icon {
        // keep this in sync with QMessageDialogOptions::Icon
        NoIcon = 0,
        Information = 1,
        Warning = 2,
        Critical = 3,
        Question = 4
    };
    Q_ENUM(Icon)

    enum StandardButton {
        // keep this in sync with QDialogButtonBox::StandardButton and QPlatformDialogHelper::StandardButton
        NoButton           = 0x00000000,
        Ok                 = 0x00000400,
        Save               = 0x00000800,
        SaveAll            = 0x00001000,
        Open               = 0x00002000,
        Yes                = 0x00004000,
        YesToAll           = 0x00008000,
        No                 = 0x00010000,
        NoToAll            = 0x00020000,
        Abort              = 0x00040000,
        Retry              = 0x00080000,
        Ignore             = 0x00100000,
        Close              = 0x00200000,
        Cancel             = 0x00400000,
        Discard            = 0x00800000,
        Help               = 0x01000000,
        Apply              = 0x02000000,
        Reset              = 0x04000000,
        RestoreDefaults    = 0x08000000,

        FirstButton        = Ok,                // internal
        LastButton         = RestoreDefaults,   // internal

        YesAll             = YesToAll,          // obsolete
        NoAll              = NoToAll,           // obsolete

        Default            = 0x00000100,        // obsolete
        Escape             = 0x00000200,        // obsolete
        FlagMask           = 0x00000300,        // obsolete
        ButtonMask         = ~FlagMask          // obsolete
    };

    Q_DECLARE_FLAGS(StandardButtons, StandardButton)
    Q_FLAG(StandardButtons)


    static QString defaultTitle();

    static QCoro::Task<StandardButton> information(QString text, StandardButtons buttons = Ok,
                                                   StandardButton defaultButton = NoButton,
                                                   QString title = { }) {
        return inst()->showMessageBoxHelper(text, Information, buttons, defaultButton, title);
    }

    static QCoro::Task<StandardButton> question(QString text,
                                                StandardButtons buttons = StandardButtons(Yes | No),
                                                StandardButton defaultButton = NoButton,
                                                QString title = { }) {
        return inst()->showMessageBoxHelper(text, Question, buttons, defaultButton, title);
    }
    static QCoro::Task<StandardButton> warning(QString text, StandardButtons buttons = Ok,
                                               StandardButton defaultButton = NoButton,
                                               QString title = { }) {
        return inst()->showMessageBoxHelper(text, Warning, buttons, defaultButton, title);
    }
    static QCoro::Task<StandardButton> critical(QString text, StandardButtons buttons = Ok,
                                                StandardButton defaultButton = NoButton,
                                                QString title = { }) {
        return inst()->showMessageBoxHelper(text, Critical, buttons, defaultButton, title);
    }

    static QCoro::Task<std::optional<QString>> getString(QString text,
                                                         QString initialValue = { },
                                                         QString title = { }) {
        return inst()->getInputString(text, initialValue, false, title);
    }
    static QCoro::Task<std::optional<QString>> getPassword(QString text,
                                                           QString title = { }) {
        return inst()->getInputString(text, { }, true, title);
    }
    static QCoro::Task<std::optional<double>> getDouble(QString text, QString unit,
                                                        double initialValue,
                                                        double minValue = std::numeric_limits<double>::min(),
                                                        double maxValue = std::numeric_limits<double>::max(),
                                                        int decimals = 1, QString title = { }) {
        return inst()->getInputDouble(text, unit, initialValue, minValue, maxValue, decimals, title);
    }
    static QCoro::Task<std::optional<int>> getInteger(QString text, QString unit,
                                                      int initialValue,
                                                      int minValue = std::numeric_limits<int>::min(),
                                                      int maxValue = std::numeric_limits<int>::max(),
                                                      QString title = { }) {
        return inst()->getInputInteger(text, unit, initialValue, minValue, maxValue, title);
    }

    static QCoro::Task<std::optional<QColor>> getColor(QColor initialColor, QString title = { }) {
        return inst()->getInputColor(initialColor, title);
    }

    static QCoro::Task<std::optional<QString>> getSaveFileName(QString fileName,
                                                               QStringList filters,
                                                               QString title = { },
                                                               QString fileTitle = { }) {
        return inst()->getFileNameHelper(true, fileName, fileTitle, filters, title);
    }

    static QCoro::Task<std::optional<QString>> getOpenFileName(QStringList filters,
                                                               QString title = { }) {
        return inst()->getFileNameHelper(false, { }, { }, filters, title);
    }

    static QCoro::Task<bool> progressDialog(QString title, QString message,
                                            auto context, auto progress, auto finished, auto start, auto cancel) {

        std::unique_ptr<UIHelpers_ProgressDialogInterface> pd(inst()->createProgressDialog(title, message));

        QObject::connect(context, progress, pd.get(), &UIHelpers_ProgressDialogInterface::progress);
        QObject::connect(context, finished, pd.get(), &UIHelpers_ProgressDialogInterface::finished);
        QObject::connect(pd.get(), &UIHelpers_ProgressDialogInterface::cancel, context, cancel);
        QObject::connect(pd.get(), &UIHelpers_ProgressDialogInterface::start, context, start);

        co_return co_await pd->exec();
    }

    static void toast(const QString &message, int timeout = 3000) {
        inst()->showToastMessageHelper(message, timeout);
    }

protected:
    virtual UIHelpers_ProgressDialogInterface *createProgressDialog(const QString &title,
                                                                    const QString &message) = 0;

    virtual QCoro::Task<StandardButton> showMessageBoxHelper(QString msg, UIHelpers::Icon icon,
                                                             StandardButtons buttons,
                                                             StandardButton defaultButton,
                                                             QString title);

    virtual QCoro::Task<StandardButton> showMessageBox(QString msg, UIHelpers::Icon icon,
                                                       StandardButtons buttons,
                                                       StandardButton defaultButton,
                                                       QString title = { }) = 0;

    virtual QCoro::Task<std::optional<QString>> getInputString(QString text,
                                                               QString initialValue,
                                                               bool isPassword,
                                                               QString title) = 0;
    virtual QCoro::Task<std::optional<double>> getInputDouble(QString text, QString unit,
                                                              double initialValue,  double minValue,
                                                              double maxValue, int decimals,
                                                              QString title) = 0;
    virtual QCoro::Task<std::optional<int>> getInputInteger(QString text, QString unit,
                                                            int initialValue, int minValue,
                                                            int maxValue, QString title) = 0;

    virtual QCoro::Task<std::optional<QColor>> getInputColor(QColor initialcolor,
                                                             QString title) = 0;

    virtual QCoro::Task<std::optional<QString>> getFileName(bool doSave, QString fileName,
                                                            QStringList filters,
                                                            QString title) = 0;

    QCoro::Task<std::optional<QString>> getFileNameHelper(bool doSave, QString fileName,
                                                          QString fileTitle,
                                                          QStringList filters,
                                                          QString title);

    void showToastMessageHelper(const QString &message, int timeout);

    virtual void processToastMessages() = 0;
    QVector<QPair<QString, int>> m_toastMessages;
    bool m_toastMessageVisible = false;

    UIHelpers() = default;
    static UIHelpers *inst();

    static UIHelpers *s_inst;

private:
    Q_SIGNAL void messageBoxClosed();
    QAtomicInt m_messageBoxCount;

};

Q_DECLARE_OPERATORS_FOR_FLAGS(UIHelpers::StandardButtons)
