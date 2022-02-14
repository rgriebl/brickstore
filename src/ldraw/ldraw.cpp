/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <cfloat>

#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QStringBuilder>
#include <QDateTime>
#include <QDebug>
#include <QStringBuilder>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#  include <tchar.h>
#  include <shlobj.h>
#endif

#include "utility/utility.h"
#include "utility/exception.h"
#include "ldraw/ldraw.h"
#include "utility/stopwatch.h"
#include "minizip/minizip.h"


Q_LOGGING_CATEGORY(LogLDraw, "ldraw")


LDraw::Element *LDraw::Element::fromString(const QString &line, const QString &dir)
{
    Element *e = nullptr;

    static auto parseVectors = []<typename T, const int N>(const QStringList &list) {
        QVector3D v[N];

        for (int i = 0; i < N; ++i)
            v[i] = QVector3D(list[3*i + 1].toFloat(), list[3*i + 2].toFloat(), list[3*i + 3].toFloat());
        return T::create(list[0].toInt(), v);
    };

    static const int element_count_lut[] = {
         0,
        14,
         7,
        10,
        13,
        13,
    };

    auto list = line.simplified().split(' '_l1);

    if (!list.isEmpty()) {
        int t = list.at(0).toInt();
        list.removeFirst();

        if (t >= 0 && t <= 5) {
            int count = element_count_lut[t];
            if ((count == 0) || (list.size() == count)) {
                switch (t) {
                case 0:
                    e = CommentElement::create(line.mid(1).trimmed());
                    break;

                case 1: {
                    QMatrix4x4 m {
                        list[4].toFloat(), list[5].toFloat(), list[6].toFloat(), list[1].toFloat(),
                        list[7].toFloat(), list[8].toFloat(), list[9].toFloat(), list[2].toFloat(),
                        list[10].toFloat(), list[11].toFloat(), list[12].toFloat(), list[3].toFloat(),
                        0, 0, 0, 1
                    };
                    m.optimize();
                    e = PartElement::create(list[0].toInt(), m, list[13], dir);
                    break;
                }
                case 2:
                    e = parseVectors.template operator()<LineElement, 2>(list);
                    break;
                case 3:
                    e = parseVectors.template operator()<TriangleElement, 3>(list);
                    break;
                case 4:
                    e = parseVectors.template operator()<QuadElement, 4>(list);
                    break;
                case 5:
                    e = parseVectors.template operator()<CondLineElement, 4>(list);
                    break;
                }
            }
        }
    }
    return e;
}



LDraw::CommentElement::CommentElement(const QString &text)
    : CommentElement(Comment, text)
{ }

LDraw::CommentElement::CommentElement(LDraw::Element::Type t, const QString &text)
    : Element(t)
    , m_comment(text)
{ }

LDraw::CommentElement *LDraw::CommentElement::create(const QString &text)
{
    if (text.startsWith("BFC "_l1))
        return new BfcCommandElement(text);
    else
        return new CommentElement(text);
}


LDraw::BfcCommandElement::BfcCommandElement(const QString &text)
    : CommentElement(BfcCommand, text)
{
    auto c = text.split(' '_l1);
    if ((c.count() >= 2) && (c.at(0) == "BFC"_l1)) {
        for (int i = 1; i < c.length(); ++i) {
            QString bfcCommand = c.at(i);

            if (bfcCommand == "INVERTNEXT"_l1)
                m_invertNext = true;
            else if (bfcCommand == "CW"_l1)
                m_cw = true;
            else if (bfcCommand == "CCW"_l1)
                m_ccw = true;
        }
    }
}

LDraw::BfcCommandElement *LDraw::BfcCommandElement::create(const QString &text)
{
    return new BfcCommandElement(text);
}


LDraw::LineElement::LineElement(int color, const QVector3D *v)
    : Element(Line), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LDraw::LineElement *LDraw::LineElement::create(int color, const QVector3D *v)
{
    return new LineElement(color, v);
}


LDraw::CondLineElement::CondLineElement(int color, const QVector3D *v)
    : Element(CondLine), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LDraw::CondLineElement *LDraw::CondLineElement::create(int color, const QVector3D *v)
{
    return new CondLineElement(color, v);
}


LDraw::TriangleElement::TriangleElement(int color, const QVector3D *v)
    : Element(Triangle), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LDraw::TriangleElement *LDraw::TriangleElement::create(int color, const QVector3D *v)
{
    return new TriangleElement(color, v);
}


LDraw::QuadElement::QuadElement(int color, const QVector3D *v)
    : Element(Quad), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LDraw::QuadElement *LDraw::QuadElement::create(int color, const QVector3D *v)
{
    return new QuadElement(color, v);
}



LDraw::PartElement::PartElement(int color, const QMatrix4x4 &matrix, LDraw::Part *p)
    : Element(Part), m_color(color), m_matrix(matrix), m_part(p)
{
    if (m_part)
        m_part->addRef();
}

LDraw::PartElement::~PartElement()
{
    if (m_part)
        m_part->release();
}

LDraw::PartElement *LDraw::PartElement::create(int color, const QMatrix4x4 &matrix,
                                               const QString &filename, const QString &parentdir)
{
    PartElement *e = nullptr;
    if (LDraw::Part *p = Core::inst()->findPart(filename, parentdir))
        e = new PartElement(color, matrix, p);
    return e;
}


LDraw::Part::Part()
    : m_boundingCalculated(false)
{
}

LDraw::Part::~Part()
{
    qDeleteAll(m_elements);
}

LDraw::Part *LDraw::Part::parse(const QByteArray &data, const QString &dir)
{
    Part *p = new Part();
    QTextStream ts(data);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ts.setCodec("UTF-8");
#else
    ts.setEncoding(QStringConverter::Utf8);
#endif

    QString line;
    int lineno = 0;
    while (!ts.atEnd()) {
        line = ts.readLine();
        lineno++;
        if (line.isEmpty())
            continue;
        if (Element *e = Element::fromString(line, dir)) {
            p->m_elements.append(e);
            p->m_cost += e->size();
        } else {
            qCWarning(LogLDraw) << "Could not parse line" << lineno << ":" << line;
        }
    }

    if (p->m_elements.isEmpty()) {
        delete p;
        p = nullptr;
    }
    return p;
}

bool LDraw::Part::boundingBox(QVector3D &vmin, QVector3D &vmax)
{
    if (!m_boundingCalculated) {
        QMatrix4x4 matrix;
        m_boundingMin = QVector3D(FLT_MAX, FLT_MAX, FLT_MAX);
        m_boundingMax = QVector3D(FLT_MIN, FLT_MIN, FLT_MIN);

        calculateBoundingBox(this, matrix, m_boundingMin, m_boundingMax);
        m_boundingCalculated = true;
    }
    vmin = m_boundingMin;
    vmax = m_boundingMax;
    return true;
}

uint LDraw::Part::cost() const
{
    return m_cost;
}


void LDraw::Part::calculateBoundingBox(const Part *part, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax)
{
    auto extend = [&matrix, &vmin, &vmax](int cnt, const QVector3D *v) {
        while (cnt--) {
            QVector3D vm = matrix.map(*v++);
            vmin = QVector3D(qMin(vmin.x(), vm.x()), qMin(vmin.y(), vm.y()), qMin(vmin.z(), vm.z()));
            vmax = QVector3D(qMax(vmax.x(), vm.x()), qMax(vmax.y(), vm.y()), qMax(vmax.z(), vm.z()));
        }
    };

    const auto elements = part->elements();
    for (const Element *e : elements) {
        switch (e->type()) {
        case Element::Line:
            extend(2, static_cast<const LineElement *>(e)->points());
            break;

        case Element::Triangle:
            extend(3, static_cast<const TriangleElement *>(e)->points());
            break;

        case Element::Quad:
            extend(4, static_cast<const QuadElement *>(e)->points());
            break;

        case Element::Part: {
            const auto *pe = static_cast<const PartElement *>(e);
            calculateBoundingBox(pe->part(), matrix * pe->matrix(), vmin, vmax);
            break;
        }
        default:
            break;
        }
    }
}



LDraw::Part *LDraw::Core::findPart(const QString &_filename, const QString &_parentdir)
{
    QString filename = _filename;
    filename.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString parentdir = _parentdir;
    if (!parentdir.isEmpty() && !parentdir.startsWith("!ZIP!"_l1))
        parentdir = QDir(parentdir).canonicalPath();

    bool inZip = false;
    bool found = false;

    if (QFileInfo(filename).isRelative()) {
        // search order is parentdir => p => parts => models

        QStringList searchpath = m_searchpath;
        if (!parentdir.isEmpty() && !searchpath.contains(parentdir))
            searchpath.prepend(parentdir);

        for (const QString &sp : qAsConst(searchpath)) {

            if (sp.startsWith("!ZIP!"_l1)) {
                filename = filename.toLower();
                QString testname = sp.mid(5) % u'/' % filename;
                if (m_zip->contains(testname)) {
                    QFileInfo fi(filename);
                    parentdir = sp;
                    if (fi.path() != "."_l1)
                        parentdir = parentdir % u'/' % fi.path();
                    filename = testname;
                    inZip = true;
                    found = true;
                    break;
                }
            } else {
                QString testname = sp % u'/' % filename;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
                if (!QFile::exists(testname))
                    testname = testname.toLower();
#endif
                if (QFile::exists(testname)) {
                    filename = testname;
                    parentdir = QFileInfo(testname).path();
                    found = true;
                    break;
                }
            }
        }
    } else {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (!QFile::exists(filename))
            filename = filename.toLower();
#endif
        if (QFile::exists(filename)) {
            parentdir = QFileInfo(filename).path();
            found = true;
        }
    }

    if (!found)
        return nullptr;
    if (!inZip)
        filename = QFileInfo(filename).canonicalFilePath();

    Part *p = m_cache[filename];
    if (!p) {
        QByteArray data;

        if (inZip) {
            try {
                data = m_zip->readFile(filename);
            } catch (const Exception &e) {
                qCWarning(LogLDraw) << "Failed to read from LDraw ZIP:" << e.error();
            }
        } else {
            QFile f(filename);

            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qCWarning(LogLDraw) << "Failed to open file" << filename << ":" << f.errorString();
            } else {
                data = f.readAll();
                if (f.error() != QFile::NoError)
                    qCWarning(LogLDraw) << "Failed to read file" << filename << ":" << f.errorString();
                f.close();
            }
        }
        if (!data.isEmpty()) {
            p = Part::parse(data, parentdir);
            if (p) {
                if (!m_cache.insert(filename, p, p->cost())) {
                    qCWarning(LogLDraw) << "Unable to cache file" << filename;
                    p = nullptr;
                }

                //qCInfo(LogLDraw) << "Cache at" << m_cache.totalCost() << "/" <<  m_cache.maxCost() << "with" << m_cache.size() << "parts";
            }
        }
    }
    return p;
}

LDraw::Part *LDraw::Core::partFromFile(const QString &file)
{
    return findPart(file, QFileInfo(file).path());
}

LDraw::Part *LDraw::Core::partFromId(const QByteArray &id)
{
    QString filename = QLatin1String(id) % u".dat";
    return findPart(filename);
}

LDraw::Core::~Core()
{
    delete m_zip;

    // the parts in cache are referencing each other, so a plain clear will not work
    m_cache.clearRecursive();
}

QString LDraw::Core::dataPath() const
{
    return m_datadir;
}

QDate LDraw::Core::lastUpdate() const
{
    return m_date;
}

bool LDraw::Core::isValidLDrawDir(const QString &ldir)
{
    QFileInfo fi(ldir);

    if (fi.exists() && fi.isDir() && fi.isReadable()) {
        QDir dir(ldir);

        if (dir.cd("p"_l1) || dir.cd("P"_l1)) {
            if (dir.exists("stud.dat"_l1) || dir.exists("STUD.DAT"_l1)) {
                if (dir.cd("../parts"_l1) || dir.cd("../PARTS"_l1)) {
                    if (dir.exists("3001.dat"_l1) || dir.exists("3001.DAT"_l1)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

QStringList LDraw::Core::potentialDrawDirs()
{
    QStringList dirs;
    dirs << QString::fromLocal8Bit(qgetenv("LDRAWDIR"));

#if defined(Q_OS_WINDOWS)
    {
        wchar_t inidir [MAX_PATH];

        DWORD l = GetPrivateProfileStringW(L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini");
        if (l >= 0) {
            inidir [l] = 0;
            dirs << QString::fromUtf16(reinterpret_cast<ushort *>(inidir));
        }
    }

    {
        HKEY skey, lkey;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
            if (RegOpenKeyExW(skey, L"LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                wchar_t regdir [MAX_PATH + 1];
                DWORD regdirsize = MAX_PATH * sizeof(wchar_t);

                if (RegQueryValueExW(lkey, L"InstallDir", nullptr, nullptr, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                    regdir [regdirsize / sizeof(WCHAR)] = 0;
                    dirs << QString::fromUtf16(reinterpret_cast<ushort *>(regdir));
                }
                RegCloseKey(lkey);
            }
            RegCloseKey(skey);
        }
    }
#elif defined(Q_OS_UNIX)
    {
        QStringList unixdirs;
        unixdirs << "~/ldraw"_l1;

#  if defined(Q_OS_MACOS)
        unixdirs << "~/Library/ldraw"_l1
                 << "~/Library/LDRAW"_l1
                 << "/Library/LDRAW"_l1
                 << "/Library/ldraw"_l1;
#  else
        unixdirs << "/usr/share/ldraw"_l1;
        unixdirs << "/var/lib/ldraw"_l1;
        unixdirs << "/var/ldraw"_l1;
#  endif

        QString homepath = QDir::homePath();

        foreach (QString d, unixdirs) {
            d.replace("~"_l1, homepath);
            dirs << d;
        }
    }
#endif
    dirs.removeAll(QString()); // remove empty strings
    return dirs;
}


LDraw::Core *LDraw::Core::s_inst = nullptr;

LDraw::Core *LDraw::Core::create(const QString &datadir, QString *errstring)
{
    if (!s_inst) {
        QString error;
        QString ldrawdir = datadir;

        if (ldrawdir.isEmpty()) {
            const auto ldawDirs = potentialDrawDirs();
            for (auto &ld : ldawDirs) {
                if (isValidLDrawDir(ld)) {
                    ldrawdir = ld;
                    break;
                }
            }
        }

        if (!ldrawdir.isEmpty()) {
            s_inst = new Core(ldrawdir);

            if (!s_inst->m_zip || s_inst->m_zip->open()) {
                if (s_inst->parseLDconfig("LDConfig.ldr")) {
                    s_inst->parseLDconfig("LDConfig_missing.ldr");
                    qInfo().noquote() << "Found LDraw at" << ldrawdir << "\n  Last updated:"
                                      << s_inst->lastUpdate().toString(Qt::RFC2822Date);
                } else {
                    error = qApp->translate("LDraw", "LDraws's ldcondig.ldr is not readable.");
                }
            } else {
                error = qApp->translate("LDraw", "The LDraw installation at \'%1\' is not usable.").arg(ldrawdir);
            }
        } else {
            error = qApp->translate("LDraw", "No usable LDraw installation available.");
        }

        if (!error.isEmpty()) {
            delete s_inst;
            s_inst = nullptr;

            if (errstring)
                *errstring = error;
        }
    }

    return s_inst;
}

LDraw::Core::Core(const QString &datadir)
    : m_datadir(datadir)
{
    m_cache.setMaxCost(50 * 1024 * 1024); // 50MB

    bool caseInsensitive =
#if !defined(Q_OS_UNIX) || defined(Q_OS_MACOS)
            false;
#else
            true;
#endif

    if (QFileInfo(datadir).isFile() && datadir.endsWith(".zip"_l1)) {
        caseInsensitive = true;
        m_zip = new MiniZip(datadir);
    }

    static const char *subdirs[] = { "p/48", "p", "parts", "models" };

    for (auto subdir : subdirs) {
        if (m_zip) {
            m_searchpath << QString(u"!ZIP!ldraw/" % QLatin1String(subdir));
        } else {
            QDir sdir(m_datadir);
            QString s = QLatin1String(subdir);

            if (sdir.cd(s))
                m_searchpath << sdir.canonicalPath();
            else if (!caseInsensitive && sdir.cd(s.toLower()))
                m_searchpath << sdir.canonicalPath();
        }
    }
}

QColor LDraw::Core::parseColorString(const QString &cstr)
{
    QString str(cstr);

    if (str.startsWith("0x"_l1))
        str.replace(0, 2, "#"_l1);
    if (str.startsWith("#"_l1))
        return { str };
    else
        return {};
}

QByteArray LDraw::Core::readLDrawFile(const QString &filename)
{
    QByteArray data;
    if (m_zip) {
        QString zipFilename = "ldraw/"_l1 % filename;
        if (m_zip->contains(zipFilename))
            data = m_zip->readFile(zipFilename);
    } else {
        QFile f(dataPath() % u'/' % filename);

        if (!f.open(QIODevice::ReadOnly))
            throw Exception(&f, "Failed to read file");

        data = f.readAll();
        if (f.error() != QFileDevice::NoError)
            throw Exception(&f, "Failed to read file");
    }
    return data;
}

bool LDraw::Core::parseLDconfig(const char *filename)
{
    //stopwatch sw("LDraw: parseLDconfig");

    QByteArray data;
    try {
        data = readLDrawFile(QLatin1String(filename));
    } catch (const Exception &) {
        //qCWarning(LogLDraw) << "LDraw ZIP:" << e.error();
        return false;
    }
    QTextStream ts(data);
    QMap<int, int> edge_ids;

    QString line;
    int lineno = 0;
    while (!ts.atEnd()) {
        line = ts.readLine();
        lineno++;

        QStringList sl = line.simplified().split(' '_l1);

        if (sl.count() >= 9 &&
                sl[0].toInt() == 0 &&
                sl[1] == "!COLOUR"_l1 &&
                sl[3] == "CODE"_l1 &&
                sl[5] == "VALUE"_l1 &&
                sl[7] == "EDGE"_l1) {
            // 0 !COLOUR name CODE x VALUE v EDGE e [ALPHA a] [LUMINANCE l] [ CHROME | PEARLESCENT | RUBBER | MATTE_METALLIC | METAL | MATERIAL <params> ]</params>

            Color c;

            c.id = sl[4].toInt();
            c.name = sl[2];
            c.color = parseColorString(sl[6]);
            c.edgeColor = parseColorString(sl[8]);

            // edge is not a RGB color, but a color index (which could be not parsed yet...)
            if (!c.edgeColor.isValid()) {
                edge_ids.insert(c.id, sl[8].toInt());
                c.edgeColor = Qt::green;
            }

            for (int idx = 9; idx < sl.count(); ++idx) {
                if (sl[idx] == "ALPHA"_l1) {
                    int alpha = sl[++idx].toInt();
                    c.color.setAlpha(alpha);
                    c.edgeColor.setAlpha(alpha);
                } else if (sl[idx] == "LUMINANCE"_l1) {
                    c.luminance = sl[++idx].toInt();
                } else if (sl[idx] == "CHROME"_l1) {
                    c.chrome = true;
                } else if (sl[idx] == "PEARLESCENT"_l1) {
                    c.pearlescent = true;
                } else if (sl[idx] == "RUBBER"_l1) {
                    c.rubber = true;
                } else if (sl[idx] == "MATTE_METALLIC"_l1) {
                    c.matteMetallic = true;
                } else if (sl[idx] == "METAL"_l1) {
                    c.metal = true;
                }
            }
            if (c.color.isValid() && c.edgeColor.isValid()) {
                m_colors.insert(c.id, c);

                //qDebug() << "Got Color " << id << " : " << main << " // " << edge;
            }
        } else if (sl.count() >= 5 &&
                   sl[0].toInt() == 0 &&
                   sl[1] == "!LDRAW_ORG"_l1 &&
                   sl[2] == "Configuration"_l1 &&
                   sl[3] == "UPDATE"_l1) {
            // 0 !LDRAW_ORG Configuration UPDATE yyyy-MM-dd
            m_date = QDate::fromString(sl[4], "yyyy-MM-dd"_l1);
        }
    }
    for (auto it = edge_ids.constBegin(); it != edge_ids.constEnd(); ++it) {
        auto set_edge = m_colors.find(it.key());
        auto get_main = m_colors.constFind(it.value());

        if ((set_edge != m_colors.end()) && (get_main != m_colors.constEnd())) {
            set_edge.value().edgeColor = get_main.value().color;
            // qDebug() << "Fixed edge color of" << it.key() << "to" << get_main.value().color;
        }
    }

    return true;
}


QColor LDraw::Core::edgeColor(int id) const
{
    if (m_colors.contains(id)) {
        return m_colors.value(id).edgeColor;
    }
    else if (id >= 0 && id <= 15) {
        // legacy ldraw mapping

        if (id <= 5)
            return color(id + 8);
        else if (id == 6)
            return color(0);
        else if (id == 7 || id == 14 || id == 15)
            return color(8);
        else
            return color(id - 8);
    }
    else if (id > 256) {
        // edge color of dithered colors is derived from color 1

        return edgeColor((id - 256) & 0x0f);
    }
    else {
        // calculate a contrasting color

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        qreal h, s, v, a;
#else
        float h, s, v, a;
#endif
        color(id).getHsvF(&h, &s, &v, &a);

        v += qreal(0.5) * ((v < qreal(0.5)) ? qreal(1) : qreal(-1));
        v = qBound(qreal(0), v, qreal(1));

        return QColor::fromHsvF(h, s, v, a);
    }
}


QColor LDraw::Core::color(int id, int baseid) const
{
    if (baseid < 0 && (id == 16 || id == 24)) {
        qWarning("Called Core::color() with meta color id 16 or 24 without specifing the meta color");
        return {};
    } else if (id == 16) {
        return color(baseid);
    } else if (id == 24) {
        return edgeColor(baseid);
    } else if (m_colors.contains(id)) {
        return m_colors.value(id).color;
    } else if (id >= 256) {
        // dithered color (do a normal blend - DOS times are over by now)

        QColor c1 = color((id - 256) & 0x0f);
        QColor c2 = color(((id - 256) >> 4) & 0x0f);

        int r1, g1, b1, a1, r2, g2, b2, a2;
        c1.getRgb(&r1, &g1, &b1, &a1);
        c2.getRgb(&r2, &g2, &b2, &a2);

        return QColor::fromRgb((r1+r2) >> 1, (g1+g2) >> 1, (b1+b2) >> 1, (a1+a2) >> 1);
    } else {
        //qWarning("Called Core::color() with an invalid LDraw color id: %d", id);
        return Qt::yellow;
    }
}
