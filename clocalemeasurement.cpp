#include "clocalemeasurement.h"


CLocaleMeasurement::MeasurementSystem CLocaleMeasurement::s_ms = CLocaleMeasurement::MetricSystem;

CLocaleMeasurement::MeasurementSystem CLocaleMeasurement::measurementSystemForLocale(const QLocale &l)
{
    if ((l.country() == QLocale::UnitedStates) || (l.country() == QLocale::UnitedStatesMinorOutlyingIslands))
        return ImperialSystem;
    else
        return MetricSystem;
}

CLocaleMeasurement::MeasurementSystem CLocaleMeasurement::measurementSystem()
{
    return s_ms;
}

void CLocaleMeasurement::setMeasurementSystem(MeasurementSystem ms)
{
    s_ms = ms;
}

QString CLocaleMeasurement::weightToString(double w, bool optimize, bool show_unit)
{
    static QLocale loc(QLocale::system());

    int prec = 0;
    const char *unit = 0;

    if (s_ms == ImperialSystem) {
        w *= 0.035273961949580412915675808215204;

        prec = 4;
        unit = "oz";

        if (optimize && (w >= 32)) {
            unit = "lb";
            w /= 16.;
        }
    }
    else {
        prec = 3;
        unit = "g";

        if (optimize && (w >= 1000.)) {
            unit = "kg";
            w /= 1000.;

            if (w >= 1000.) {
                unit = "t";
                w /= 1000.;
            }
        }
    }
    QString s = loc.toString(w, 'f', prec);

    if (show_unit) {
        s += " ";
        s += unit;
    }
    return s;
}

double CLocaleMeasurement::stringToWeight(const QString &s)
{
    static QLocale loc(QLocale::system());

    double w = loc.toDouble(s);

    if (s_ms == ImperialSystem)
        w *= 28.349523125000000000000000000164;

    return w;
}

