#ifndef __STOPWATCH_H__
#define __STOPWATCH_H__

#include <QtDebug>
#include <ctime>

class stopwatch {
public:
    stopwatch(const char *desc)
    {
        m_label = desc;
        m_start = std::clock();
    }
    ~stopwatch()
    {
        uint msec = uint(std::clock() - m_start) * 1000 / CLOCKS_PER_SEC;
        qWarning("%s: %d'%d [sec]", m_label, msec / 1000, msec % 1000);
    }
private:
    const char *m_label;
    clock_t m_start;
};

#endif
