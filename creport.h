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
#ifndef __CREPORT_H__
#define __CREPORT_H__

#include <QObject>
#include <QString>
#include <QList>

#include "cdocument.h"

class CReportPrivate;
class QPrinter;

class CReport : public QObject {
    Q_OBJECT
public:
    static CReport *load(const QString &file);
    virtual ~CReport();
    
    QString name() const;

    void print(QPaintDevice *pd, const CDocument *doc, const CDocument::ItemList &items) const;

private:
    CReport();

    CReportPrivate *d;
};

class CReportManager {
private:
    CReportManager();
    static CReportManager *s_inst;

public:
    ~CReportManager();
    static CReportManager *inst();

    bool reload();

    QPrinter *printer() const;

    QList<CReport *> reports() const;

private:
    QList<CReport *> m_reports;

    mutable QPrinter *m_printer; // mutable for delayed initialization
};

#endif

