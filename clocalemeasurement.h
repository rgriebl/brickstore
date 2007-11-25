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

	static QString weightToString(double gramm, bool optimize = false, bool show_unit = false);
	static double stringToWeight(const QString &s);

private:
	static MeasurementSystem s_ms;
};

#endif
