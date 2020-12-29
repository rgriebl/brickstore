/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QString>
#include <QList>
#include <QScopedPointer>

#include "document.h"

class ReportPrivate;
class QPrinter;


class Report : public QObject
{
    Q_OBJECT
public:
    ~Report() override;
    static Report *load(const QString &file);

    QString name() const;
    QString label() const;

    void print(QPaintDevice *pd, const Document *doc, const Document::ItemList &items) const;

private:
    Q_DISABLE_COPY(Report)
    Report();

    QScopedPointer<ReportPrivate> d;
};

class ReportManager
{
private:
    ReportManager();
    static ReportManager *s_inst;

public:
    ~ReportManager();
    static ReportManager *inst();

    bool reload();

    QList<Report *> reports() const;

private:
    Q_DISABLE_COPY(ReportManager)

    QList<Report *> m_reports;
};

