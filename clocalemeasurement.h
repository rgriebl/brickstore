/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __CLOCALEMEASUREMENT__
#define __CLOCALEMEASUREMENT__

#include <QLocale>

class CLocaleMeasurement {
public:
    enum MeasurementSystem {
        MetricSystem = 0,
        ImperialSystem
    };

    static MeasurementSystem measurementSystemForLocale(const QLocale &l = QLocale());

    static MeasurementSystem measurementSystem();
    static void setMeasurementSystem(MeasurementSystem ms);
    
    static bool isMetric()    { return measurementSystem() == MetricSystem; }
    static bool isImperial()  { return measurementSystem() == ImperialSystem; }

    static QString weightToString(double gramm, bool optimize = false, bool show_unit = false);
    static double stringToWeight(const QString &s);

private:
    static MeasurementSystem s_ms;
};

#endif
