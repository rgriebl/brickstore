/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __REPORT_H__
#define __REPORT_H__

#include <QObject>
#include <QString>
#include <QList>

#include "document.h"

class ReportPrivate;
class QPrinter;

class Report : public QObject {
    Q_OBJECT
public:
    static Report *load(const QString &file);
    virtual ~Report();

    QString name() const;

    void print(QPaintDevice *pd, const Document *doc, const Document::ItemList &items) const;

private:
    Report();

    ReportPrivate *d;
};

class ReportManager {
private:
    ReportManager();
    static ReportManager *s_inst;

public:
    ~ReportManager();
    static ReportManager *inst();

    bool reload();

    QPrinter *printer() const;

    QList<Report *> reports() const;

private:
    QList<Report *> m_reports;

    mutable QPrinter *m_printer; // mutable for delayed initialization
};

#endif

