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
#include <cfloat>

#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QtDebug>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#  include <tchar.h>
#  include <shlobj.h>
#endif

#include "ldraw.h"
#include "stopwatch.h"

LDraw::Element *LDraw::Element::fromByteArray(const QByteArray &line, const QDir &dir)
{
    Element *e = nullptr;
    int t = -1;

    static const int element_count_lut[] = {
        -1,
        14,
         7,
        10,
        13,
        13,
    };

    QList<QByteArray> bal = line.simplified().split(' ');

    if (!bal.isEmpty()) {
        t = bal[0].toInt();
        bal.removeFirst();

        if (t >= 0 && t <= 5) {
            if (!t || element_count_lut[t] == bal.size()) {
                switch (t) {
                case 0: {
                    e = CommentElement::create(line.trimmed().mid(1).trimmed());
                    break;
                }
                case 1: {
                    int color = bal[0].toInt();
                    QMatrix4x4 m(
                        bal[4].toFloat(),
                        bal[5].toFloat(),
                        bal[6].toFloat(),
                        bal[1].toFloat(),

                        bal[7].toFloat(),
                        bal[8].toFloat(),
                        bal[9].toFloat(),
                        bal[2].toFloat(),

                        bal[10].toFloat(),
                        bal[11].toFloat(),
                        bal[12].toFloat(),
                        bal[3].toFloat(),

                        0, 0, 0, 1
                    );
                    m.optimize();

                    e = PartElement::create(color, m, bal[13], dir);
                    break;
                }
                case 2: {
                    int color = bal[0].toInt();
                    QVector3D v[2];

                    for (int i = 0; i < 2; ++i)
                        v[i] = QVector3D(bal[3*i + 1].toFloat(), bal[3*i + 2].toFloat(), bal[3*i + 3].toFloat());
                    e = LineElement::create(color, v);
                    break;
                }
                case 3: {
                    int color = bal[0].toInt();
                    QVector3D v[3];

                    for (int i = 0; i < 3; ++i)
                        v[i] = QVector3D(bal[3*i + 1].toFloat(), bal[3*i + 2].toFloat(), bal[3*i + 3].toFloat());
                    e = TriangleElement::create(color, v);
                    break;
                }
                case 4: {
                    int color = bal[0].toInt();
                    QVector3D v[4];

                    for (int i = 0; i < 4; ++i)
                        v[i] = QVector3D(bal[3*i + 1].toFloat(), bal[3*i + 2].toFloat(), bal[3*i + 3].toFloat());
                    e = QuadElement::create(color, v);
                    break;
                }
                case 5: {
                    int color = bal[0].toInt();
                    QVector3D v[4];

                    for (int i = 0; i < 4; ++i)
                        v[i] = QVector3D(bal[3*i + 1].toFloat(), bal[3*i + 2].toFloat(), bal[3*i + 3].toFloat());
                    e = CondLineElement::create(color, v);
                    break;
                }
                default: break;
                }
            }
        }
    }
    return e;
}


void LDraw::Element::dump() const
{ }


LDraw::CommentElement::CommentElement(const QByteArray &text)
    : Element(Comment), m_comment(text)
{ }

LDraw::CommentElement *LDraw::CommentElement::create(const QByteArray &text)
{
    return new CommentElement(text);
}

void LDraw::CommentElement::dump() const
{
    qDebug() << 0 << m_comment.constData();
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

void LDraw::LineElement::dump() const
{
    qDebug() << 2 << m_color
             << m_points[0].x() << m_points[0].y() << m_points[0].z()
             << m_points[1].x() << m_points[1].y() << m_points[1].z();
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

void LDraw::CondLineElement::dump() const
{
    qDebug() << 5 << m_color
             << m_points[0].x() << m_points[0].y() << m_points[0].z()
             << m_points[1].x() << m_points[1].y() << m_points[1].z()
             << m_points[2].x() << m_points[2].y() << m_points[2].z()
             << m_points[3].x() << m_points[3].y() << m_points[3].z();
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

void LDraw::TriangleElement::dump() const
{
    qDebug() << 3 << m_color
             << m_points[0].x() << m_points[0].y() << m_points[0].z()
             << m_points[1].x() << m_points[1].y() << m_points[1].z()
             << m_points[2].x() << m_points[2].y() << m_points[2].z();
}


LDraw::QuadElement::QuadElement(int color, const QVector3D *v)
    : Element(Quad), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
#if 0
    //fix CCW order
    // getting QVector3Ds from points
    a10 = p0 - p1;
    a12 = p2 - p1;
    a13 = p3 - p1;
    // building normals (with cross)
    n0 = a12 * a10;
    n1 = a10 * n0;
    n2 = n0 * a12;
    // checking skalar
    s1=a13.dot(n1);
    s2=a13.dot(n2);
    if (s1 < 0) switch(p1,p2); // switch 1-2
    if (s2 < 0) switch(p2,p3); // switch 2-3
#endif
}

LDraw::QuadElement *LDraw::QuadElement::create(int color, const QVector3D *v)
{
    return new QuadElement(color, v);
}

void LDraw::QuadElement::dump() const
{
    qDebug() << 4 << m_color
             << m_points[0].x() << m_points[0].y() << m_points[0].z()
             << m_points[1].x() << m_points[1].y() << m_points[1].z()
             << m_points[2].x() << m_points[2].y() << m_points[2].z()
             << m_points[3].x() << m_points[3].y() << m_points[3].z();
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

LDraw::PartElement *LDraw::PartElement::create(int color, const QMatrix4x4 &matrix, const QString &filename, const QDir &parentdir)
{
    PartElement *e = nullptr;
    if (LDraw::Part *p = Core::inst()->findPart(filename, parentdir))
        e = new PartElement(color, matrix, p);
    return e;
}

void LDraw::PartElement::dump() const
{
    qDebug() << 1 << m_color
             << m_matrix(3, 0) << m_matrix(3, 1) << m_matrix(3, 2)
             << m_matrix(0, 0) << m_matrix(1, 0) << m_matrix(2, 0)
             << m_matrix(0, 1) << m_matrix(1, 1) << m_matrix(2, 1)
             << m_matrix(0, 2) << m_matrix(1, 2) << m_matrix(2, 2);

    if (m_part) {
        qDebug(">> start of sub-part >>");
        m_part->dump();
        qDebug("<< end of sub-part <<\n");
    } else {
        qDebug(">> invalid sub-part <<\n");
    }
}


LDraw::Part::Part()
    : m_bounding_calculated(false)
{
}

LDraw::Part::~Part()
{
    qDeleteAll(m_elements);
}

LDraw::Part *LDraw::Part::parse(QFile &file, const QDir &dir)
{
    Part *p = new Part();
//    QTextStream ts(&file);

    QByteArray line;
    int lineno = 0;
    while (!file.atEnd()) {
        line = file.readLine();
        if (line.isEmpty() && file.error() != QFile::NoError) {
            qWarning("Read error in line #%d: %s", lineno, line.constData());
            break;
        }
        lineno++;
        Element *e = Element::fromByteArray(line, dir);
        if (e)
            p->m_elements.append(e);
        else
            qWarning("Could not parse line #%d: %s", lineno, line.constData());
    }

    if (p->m_elements.isEmpty()) {
        delete p;
        p = nullptr;
    }
    return p;
}

bool LDraw::Part::boundingBox(QVector3D &vmin, QVector3D &vmax)
{
    if (!m_bounding_calculated) {
        QMatrix4x4 matrix;
        m_bounding_min = QVector3D(FLT_MAX, FLT_MAX, FLT_MAX);
        m_bounding_max = QVector3D(FLT_MIN, FLT_MIN, FLT_MIN);

        calc_bounding_box(this, matrix, m_bounding_min, m_bounding_max);

        m_bounding_calculated = true;
    }
    vmin = m_bounding_min;
    vmax = m_bounding_max;

    return true;
}

void LDraw::Part::check_bounding(int cnt, const QVector3D *v, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax)
{
    while (cnt--) {
        QVector3D vm = matrix * (*v);

        vmin = QVector3D(qMin(vmin.x(), vm.x()), qMin(vmin.y(), vm.y()), qMin(vmin.z(), vm.z()));
        vmax = QVector3D(qMax(vmax.x(), vm.x()), qMax(vmax.y(), vm.y()), qMax(vmax.z(), vm.z()));

        v++;
    }
}



void LDraw::Part::calc_bounding_box(const Part *part, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax)
{
    const auto elements = part->elements();
    for (const Element *e : elements) {
        switch (e->type()) {
        case Element::Line: {
            const auto *le = static_cast<const LineElement *>(e);

            check_bounding(2, le->points(), matrix, vmin, vmax);
            break;
        }
        case Element::Triangle: {
            const auto *te = static_cast<const TriangleElement *>(e);

            check_bounding(3, te->points(), matrix, vmin, vmax);
            break;
        }
        case Element::Quad: {
            const auto *qe = static_cast<const QuadElement *>(e);

            check_bounding(4, qe->points(), matrix, vmin, vmax);
            break;
        }
        case Element::Part: {
            const auto *pe = static_cast<const PartElement *>(e);

            calc_bounding_box(pe->part(), matrix * pe->matrix(), vmin, vmax);
            break;
        }
        default: {
            break;
        }
        }
    }
}



void LDraw::Part::dump() const
{
    for (const Element *e : m_elements)
        e->dump();
}

LDraw::Part *LDraw::Core::findPart(const QString &_filename, const QDir &parentdir)
{
    QString filename = _filename;
    filename.replace(QLatin1Char('\\'), QLatin1Char('/'));
    bool found = false;

    if (QFileInfo(filename).isRelative()) {
        // search order is parentdir => p => parts => models

        QList<QDir> searchpath = m_searchpath;
        searchpath.prepend(parentdir);

        for (const QDir &sp : qAsConst(searchpath)) {
            QString testname = sp.absolutePath() + QLatin1Char('/') + filename;
#if defined(Q_OS_LINUX)
            if (!QFile::exists(testname))
                testname = testname.toLower();
#endif
            if (QFile::exists(testname)) {
                filename = testname;
                found = true;
                break;
            }
        }
    } else {
#if defined(Q_OS_LINUX)
        if (!QFile::exists(filename))
            filename = filename.toLower();
#endif
        if (QFile::exists(filename))
            found = true;
    }

    if (!found)
        return nullptr;

    filename = QFileInfo(filename).canonicalFilePath();

    Part *p = m_cache[filename];

    if (!p) {
        QFile f(filename);

        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            p = Part::parse(f, QFileInfo(f).dir());
        if (p) {
            if (!m_cache.insert(filename, p)) {
                qWarning("Unable to cache LDraw file %s", qPrintable(filename));
                p = nullptr;
            }
        }
    }
    return p;
}

LDraw::Part *LDraw::Core::partFromFile(const QString &file)
{
    return findPart(file, QDir::current());
}

LDraw::Part *LDraw::Core::partFromId(const QString &id)
{
    QDir parts(dataPath());
    if (parts.cd(QLatin1String("parts")) || parts.cd(QLatin1String("PARTS"))) {
        QString filename = id + QLatin1String(".dat");
        return findPart(parts.absoluteFilePath(filename), QDir::root());
    }
    return nullptr;
}

LDraw::Core::~Core()
{
    // the parts in cache are referencing each other, so a plain clear will not work
    m_cache.clearRecursive();
}

QString LDraw::Core::dataPath() const
{
    return m_datadir;
}

bool LDraw::Core::check_ldrawdir(const QString &ldir)
{
    QFileInfo fi(ldir);

    if (fi.exists() && fi.isDir() && fi.isReadable()) {
        QDir dir(ldir);

        if (dir.cd(QLatin1String("p")) || dir.cd(QLatin1String("P"))) {
            if (dir.exists(QLatin1String("stud.dat")) || dir.exists(QLatin1String("STUD.DAT"))) {
                if (dir.cd(QLatin1String("../parts")) || dir.cd(QLatin1String("../PARTS"))) {
                    if (dir.exists(QLatin1String("3001.dat")) || dir.exists(QLatin1String("3001.DAT"))) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

QString LDraw::Core::get_platform_ldrawdir()
{
    QString dir = QString::fromLocal8Bit(qgetenv("LDRAWDIR"));

#if defined(Q_OS_WINDOWS)
    if (dir.isEmpty() || !check_ldrawdir(dir)) {
        wchar_t inidir [MAX_PATH];

        DWORD l = GetPrivateProfileStringW(L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini");
        if (l >= 0) {
            inidir [l] = 0;
            dir = QString::fromUtf16(reinterpret_cast<ushort *>(inidir));
        }
    }

    if (dir.isEmpty() || !check_ldrawdir(dir)) {
        HKEY skey, lkey;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
            if (RegOpenKeyExW(skey, L"LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                wchar_t regdir [MAX_PATH + 1];
                DWORD regdirsize = MAX_PATH * sizeof(wchar_t);

                if (RegQueryValueExW(lkey, L"InstallDir", nullptr, nullptr, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                    regdir [regdirsize / sizeof(WCHAR)] = 0;
                    dir = QString::fromUtf16(reinterpret_cast<ushort *>(regdir));
                }
                RegCloseKey(lkey);
            }
            RegCloseKey(skey);
        }
    }
#elif defined(Q_OS_UNIX)
    if (dir.isEmpty() || !check_ldrawdir(dir)) {
        QStringList unixdirs;
#  if defined(Q_OS_MACOS)
        unixdirs << QLatin1String("/Library/LDRAW")
                 << QLatin1String("/Library/ldraw")
                 << QLatin1String("~/Library/LDRAW")
                 << QLatin1String("~/Library/ldraw");
#  else
        unixdirs << "~/ldraw"
                 << "/usr/share/ldraw";
#  endif
        QString homepath = QDir::homePath();

        foreach (QString d, unixdirs) {
            d.replace(QLatin1String("~"), homepath);

            if (check_ldrawdir(d)) {
                dir = d;
                break;
            }
        }
    }
#endif
    return dir;
}


LDraw::Core *LDraw::Core::s_inst = nullptr;

LDraw::Core *LDraw::Core::create(const QString &datadir, QString *errstring)
{
    if (!s_inst) {
        QString error;
        QString ldrawdir = datadir;

        if (ldrawdir.isEmpty())
            ldrawdir = get_platform_ldrawdir();

        if (check_ldrawdir(ldrawdir)) {
            s_inst = new Core(ldrawdir);

            if (s_inst->parse_ldconfig("LDConfig.ldr")) {
                s_inst->parse_ldconfig("LDConfig_missing.ldr");

                if (s_inst->create_part_list()) {
                    qWarning() << "Found" << s_inst->m_items.size() << "LDraw parts";
                    qt_noop();
                }
                else
                    error = qApp->translate("LDraw", "Can not create part list.");
            }
            else
                error = qApp->translate("LDraw", "The file ldcondig.ldr is not readable.");
        }
        else
            error = qApp->translate("LDraw", "Data directory \'%1\' is not readable.").arg(datadir);

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
    static const char *subdirs[] =
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        { "P", "p", "PARTS", "parts", "MODELS", "models", };
#else
        { "P", "PARTS", "MODELS" };
#endif

    for (auto subdir : subdirs) {
        QDir sdir(m_datadir);

        if (sdir.cd(QLatin1String(subdir)))
            m_searchpath << sdir;
    }
}


bool LDraw::Core::create_part_list()
{
    stopwatch sw("LDraw: create_part_list");

    QDir ldrawdir(dataPath());
    if (!ldrawdir.cd(QLatin1String("parts")) && !ldrawdir.cd(QLatin1String("PARTS")))
        return false;

    QDirIterator it(ldrawdir.path(), QDir::Files | QDir::Readable);

    m_items.clear();

    QDateTime lastmod;

    // TODO: caching !!!
    // set lastmod to cache file's mod time

    while (it.hasNext()) {
        it.next();
        QByteArray id = it.fileName().toLatin1().toLower();

        if (id.right(4) != ".dat")
            continue;
        if (lastmod.isValid() && it.fileInfo().lastModified() <= lastmod)
            continue;

        id.chop(4);
        QByteArray name;
#if 1
        // 0'970s on MacBookPro

        QFile f(it.filePath());

        if (f.open(QIODevice::ReadOnly)) {
            qint64 l = qMin(qint64(4096) /* get page size*/, f.size());
            char *p = reinterpret_cast<char *>(f.map(0, l));

            if (p && l >= 3 && p[0] == '0' && (p[1] == ' ' || p[1] == '\t')) {
                char *nl = reinterpret_cast<char *>(memchr(p, '\n', size_t(l)));
                if (nl)
                    l = nl - p;
                name = QByteArray(p + 2, int(l - 2)).simplified();
            }
        }

#else
        // 1'000s on MacBookPro

        QFile f(it.filePath());

        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray ba = f.readLine().simplified();

            if (ba.size() >= 3 && ba[0] == '0' && ba[1] == ' ') {
                name = ba.right(ba.size() - 2);
            }
        }
#endif

        if (!name.isEmpty() && name[0] != '~') {
            m_items[id] = name;
//            printf("%s\t->\t%s\n", id.constData(), name.constData());
        }
    }
    return !m_items.isEmpty();
}

QColor LDraw::Core::parse_color_string(const QString &cstr)
{
    QString str(cstr);

    if (str.startsWith(QLatin1String("0x")))
        str.replace(0, 2, QLatin1String("#"));
    if (str.startsWith(QLatin1String("#")))
        return { str };
    else
        return {};
}

bool LDraw::Core::parse_ldconfig(const char *filename)
{
    stopwatch sw("LDraw: parse_ldconfig");

    QDir ldrawdir(dataPath());
    QFile f(ldrawdir.filePath(QLatin1String(filename)));

    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        QMap<int, int> edge_ids;

        QString line;
        int lineno = 0;
        while (!ts.atEnd()) {
            line = ts.readLine();
            lineno++;

            QStringList sl = line.simplified().split(' ');
            if (sl.count() >= 9 &&
                sl[0].toInt() == 0 &&
                sl[1] == QLatin1String("!COLOUR") &&
                sl[3] == QLatin1String("CODE") &&
                sl[5] == QLatin1String("VALUE") &&
                sl[7] == QLatin1String("EDGE")) {
                // 0 !COLOUR name CODE x VALUE v EDGE e [ALPHA a] [LUMINANCE l] [ CHROME | PEARLESCENT | RUBBER | MATTE_METALLIC | METAL | MATERIAL <params> ]</params>

                Color c;

                c.id = sl[4].toInt();
                c.name = sl[2];
                c.color = parse_color_string(sl[6]);
                c.edgeColor = parse_color_string(sl[8]);

                // edge is not a RGB color, but a color index (which could be not parsed yet...)
                if (!c.edgeColor.isValid()) {
                    edge_ids.insert(c.id, sl[8].toInt());
                    c.edgeColor = Qt::green;
                }

                for (int idx = 9; idx < sl.count(); ++idx) {
                    if (sl[idx] == QLatin1String("ALPHA")) {
                        int alpha = sl[++idx].toInt();
                        c.color.setAlpha(alpha);
                        c.edgeColor.setAlpha(alpha);
                    } else if (sl[idx] == QLatin1String("LUMINANCE")) {
                        c.luminance = sl[++idx].toInt();
                    } else if (sl[idx] == QLatin1String("CHROME")) {
                        c.chrome = true;
                    } else if (sl[idx] == QLatin1String("PEARLESCENT")) {
                        c.pearlescent = true;
                    } else if (sl[idx] == QLatin1String("RUBBER")) {
                        c.rubber = true;
                    } else if (sl[idx] == QLatin1String("MATTE_METALLIC")) {
                        c.matteMetallic = true;
                    } else if (sl[idx] == QLatin1String("METAL")) {
                        c.metal = true;
                    }
                }
                if (c.color.isValid() && c.edgeColor.isValid()) {
                    m_colors.insert(c.id, c);

                    //qDebug() << "Got Color " << id << " : " << main << " // " << edge;
                }
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
    return false;
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

        qreal h, s, v, a;
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
