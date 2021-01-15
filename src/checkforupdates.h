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
#pragma once

#include <QObject>
#include <QVersionNumber>

class ProgressDialog;


class CheckForUpdates : public QObject
{
    Q_OBJECT
public:
    CheckForUpdates(ProgressDialog *pd);

private slots:
    void gotten();

private:
    ProgressDialog *m_progress;
    QVersionNumber m_current_version;
    QVersionNumber m_latest_version;
};
