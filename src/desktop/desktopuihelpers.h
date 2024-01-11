// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QPointer>
#include <QKeyEvent>
#include <QRect>

#include "common/uihelpers.h"
#include "common/eventfilter.h"

QT_FORWARD_DECLARE_CLASS(QWidget)

class ToastMessage;


class DesktopUIHelpers : public UIHelpers
{
    Q_OBJECT

public:
    static void create();
    static void setDefaultParent(QWidget *defaultParent);

    static void setPopupPos(QWidget *w, const QRect &pos);

    static EventFilter::Result selectAllFilter(QObject *o, QEvent *e);

protected:
    QCoro::Task<StandardButton> showMessageBox(QString msg, UIHelpers::Icon icon,
                                               StandardButtons buttons, StandardButton defaultButton,
                                               QString title) override;

    QCoro::Task<std::optional<QString>> getInputString(QString text,
                                                       QString initialValue, bool isPassword,
                                                       QString title) override;
    QCoro::Task<std::optional<double>> getInputDouble(QString text, QString unit,
                                                      double initialValue,  double minValue,
                                                      double maxValue, int decimals,
                                                      QString title) override;
    QCoro::Task<std::optional<int>> getInputInteger(QString text, QString unit,
                                                    int initialValue, int minValue,
                                                    int maxValue, QString title) override;

    QCoro::Task<std::optional<QColor>> getInputColor(QColor initialcolor,
                                                     QString title) override;

    QCoro::Task<std::optional<QString>> getFileName(bool doSave, QString fileName,
                                                    QStringList filters,
                                                    QString title = { }) override;

    UIHelpers_ProgressDialogInterface *createProgressDialog(const QString &title,
                                                            const QString &message) override;

    void processToastMessages() override;

private:
    DesktopUIHelpers() = default;

    std::unique_ptr<ToastMessage> m_currentToastMessage;

    static QPointer<QWidget> s_defaultParent;

    friend class DesktopPDI;
};
