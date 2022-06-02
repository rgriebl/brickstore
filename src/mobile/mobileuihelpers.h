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

#include <QPointer>

#include "common/uihelpers.h"

QT_FORWARD_DECLARE_CLASS(QQmlApplicationEngine)
QT_FORWARD_DECLARE_CLASS(QQuickDialog)


class MobileUIHelpers : public UIHelpers
{
    Q_OBJECT

public:
    static void create(QQmlApplicationEngine *engine);

protected:
    QCoro::Task<StandardButton> showMessageBox(QString msg, UIHelpers::Icon icon,
                                               StandardButtons buttons, StandardButton defaultButton,
                                               QString title = { }) override;

    QCoro::Task<std::optional<QString>> getInputString(QString text,
                                                       QString initialValue,
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
    MobileUIHelpers();

    static QPointer<QQmlApplicationEngine> s_engine;
};
