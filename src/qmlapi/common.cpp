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
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QCoreApplication>

#include "common.h"


void redirectQmlEngineWarnings(QQmlEngine *engine, const QLoggingCategory &cat)
{
    engine->setOutputWarningsToStandardError(false);

    QObject::connect(engine, &QQmlEngine::warnings,
                     qApp, [&cat](const QList<QQmlError> &list) {
        if (!cat.isWarningEnabled())
            return;

        for (auto &err : list) {
            QByteArray func;
            if (err.object())
                func = err.object()->objectName().toLocal8Bit();
            QByteArray file;
            if (err.url().scheme() == QLatin1String("file"))
                file = err.url().toLocalFile().toLocal8Bit();
            else
                file = err.url().toDisplayString().toLocal8Bit();

            QMessageLogger ml(file, err.line(), func, cat.categoryName());
            ml.warning().nospace().noquote() << err.description();
        }
    });

}
