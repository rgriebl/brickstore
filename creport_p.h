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
#ifndef __CREPORT_P_H__
#define __CREPORT_P_H__

#include <time.h>

class QScriptEngine;

class CReportPrivate {
public:
    time_t              m_loaded;
    QString             m_name;
    QString             m_code;
    QString             m_label;
    QScriptEngine *     m_engine;
};

#endif
