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
#include <cfloat>

#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QtDebug>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <tchar.h>
#  include <shlobj.h>
#endif

#include "ldraw.h"
#include "stopwatch.h"

LDraw::Element *LDraw::Element::fromByteArray(const QByteArray &line, const QDir &dir)
{
    Element *e = 0;
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
                        bal[4].toDouble(),
                        bal[5].toDouble(),
                        bal[6].toDouble(),
                        bal[1].toDouble(),

                        bal[7].toDouble(),
                        bal[8].toDouble(),
                        bal[9].toDouble(),
                        bal[2].toDouble(),

                        bal[10].toDouble(),
                        bal[11].toDouble(),
                        bal[12].toDouble(),
                        bal[3].toDouble(),

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
                        v[i] = QVector3D(bal[3*i + 1].toDouble(), bal[3*i + 2].toDouble(), bal[3*i + 3].toDouble());
                    e = LineElement::create(color, v);
                    break;
                }
                case 3: {
                    int color = bal[0].toInt();
                    QVector3D v[3];

                    for (int i = 0; i < 3; ++i)
                        v[i] = QVector3D(bal[3*i + 1].toDouble(), bal[3*i + 2].toDouble(), bal[3*i + 3].toDouble());
                    e = TriangleElement::create(color, v);
                    break;
                }
                case 4: {
                    int color = bal[0].toInt();
                    QVector3D v[4];

                    for (int i = 0; i < 4; ++i)
                        v[i] = QVector3D(bal[3*i + 1].toDouble(), bal[3*i + 2].toDouble(), bal[3*i + 3].toDouble());
                    e = QuadElement::create(color, v);
                    break;
                }
                case 5: {
                    int color = bal[0].toInt();
                    QVector3D v[4];

                    for (int i = 0; i < 4; ++i)
                        v[i] = QVector3D(bal[3*i + 1].toDouble(), bal[3*i + 2].toDouble(), bal[3*i + 3].toDouble());
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



LDraw::CommentElement::CommentElement(const QByteArray &text)
    : Element(Comment), m_comment(text)
{ }

LDraw::CommentElement *LDraw::CommentElement::create(const QByteArray &text)
{
    return new CommentElement(text);
}

void LDraw::CommentElement::dump() const
{
    printf("0 %s\n", m_comment.constData());
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
    printf("2 %d %.f %.f %.f %.f %.f %.f\n",
        m_color,
        m_points[0].x(), m_points[0].y(), m_points[0].z(),
        m_points[1].x(), m_points[1].y(), m_points[1].z());
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
    printf("5 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f\n",
        m_color,
        m_points[0].x(), m_points[0].y(), m_points[0].z(),
        m_points[1].x(), m_points[1].y(), m_points[1].z(),
        m_points[2].x(), m_points[2].y(), m_points[2].z(),
        m_points[3].x(), m_points[3].y(), m_points[3].z());
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
    printf("3 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f\n",
        m_color,
        m_points[0].x(), m_points[0].y(), m_points[0].z(),
        m_points[1].x(), m_points[1].y(), m_points[1].z(),
        m_points[2].x(), m_points[2].y(), m_points[2].z());
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
    printf("4 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f\n",
        m_color,
        m_points[0].x(), m_points[0].y(), m_points[0].z(),
        m_points[1].x(), m_points[1].y(), m_points[1].z(),
        m_points[2].x(), m_points[2].y(), m_points[2].z(),
        m_points[3].x(), m_points[3].y(), m_points[3].z());
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
    PartElement *e = 0;
    if (LDraw::Part *p = Core::inst()->findPart(filename, parentdir))
        e = new PartElement(color, matrix, p);
    return e;
}

void LDraw::PartElement::dump() const
{
    printf("1 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f\n",
        m_color,
        m_matrix(3, 0), m_matrix(3, 1), m_matrix(3, 2),
        m_matrix(0, 0), m_matrix(1, 0), m_matrix(2, 0),
        m_matrix(0, 1), m_matrix(1, 1), m_matrix(2, 1),
        m_matrix(0, 2), m_matrix(1, 2), m_matrix(2, 2));

    if (m_part) {
        printf(">> start of sub-part >>\n");
        m_part->dump();
        printf("<< end of sub-part <<\n");
    } else {
        printf(">> invalid sub-part <<\n");
    }
}


LDraw::Part::Part()
    : m_bounding_calculated(false)
{
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
        p = 0;
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
    foreach (Element *e, part->elements()) {
        switch (e->type()) {
        case Element::Line: {
            const LineElement *le = static_cast<const LineElement *>(e);

            check_bounding(2, le->points(), matrix, vmin, vmax);
            break;
        }
        case Element::Triangle: {
            const TriangleElement *te = static_cast<const TriangleElement *>(e);

            check_bounding(3, te->points(), matrix, vmin, vmax);
            break;
        }
        case Element::Quad: {
            const QuadElement *qe = static_cast<const QuadElement *>(e);

            check_bounding(4, qe->points(), matrix, vmin, vmax);
            break;
        }
        case Element::Part: {
            const PartElement *pe = static_cast<const PartElement *>(e);

            calc_bounding_box(pe->part(), pe->matrix() * matrix, vmin, vmax);
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
    foreach (Element *e, m_elements) {
        e->dump();
    }
}

LDraw::Part *LDraw::Core::findPart(const QString &xfilename, const QDir &parentdir)
{
    QString filename = QDir::fromNativeSeparators(xfilename);
    bool found = false;

    if (QFileInfo(filename).isRelative()) {
        // search order is parentdir => p => parts => models

        QList<QDir> searchpath = m_searchpath;
        searchpath.prepend(parentdir);

        foreach (QDir sp, searchpath) {
            if (sp.exists(filename)) {
                filename = sp.absoluteFilePath(filename);
                found = true;
                break;
            }
        }
    } else {
        if (QFile::exists(filename))
            found = true;
    }

    if (!found)
        return 0;

    filename = QFileInfo(filename).canonicalFilePath();

    Part *p = m_cache[filename];

    if (!p) {
        QFile f(filename);

        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            p = Part::parse(f, QFileInfo(f).dir());
        if (p) {
            if (!m_cache.insert(filename, p)) {
                qWarning("Unable to cache LDraw file %s", qPrintable(filename));
                p = 0;
            }
        }
    }
    return p;
}

LDraw::Part *LDraw::Core::partFromFile(const QString &file)
{
    return findPart(file, QDir::current());
}

LDraw::Part *LDraw::Core::partFromId(const char *id)
{
    QDir parts(dataPath());
    if (parts.cd(QLatin1String("parts")) || parts.cd(QLatin1String("PARTS"))) {
        QString filename = QLatin1String(id) + QLatin1String(".dat");
        return findPart(parts.absoluteFilePath(filename), QDir::root());
    }
    return 0;
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
    QString dir = QString::fromLocal8Bit(::getenv("LDRAWDIR"));

#if defined( Q_WS_WIN )
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

                if (RegQueryValueExW(lkey, L"InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                    regdir [regdirsize / sizeof(WCHAR)] = 0;
                    dir = QString::fromUtf16(reinterpret_cast<ushort *>(regdir));
                }
                RegCloseKey(lkey);
            }
            RegCloseKey(skey);
        }
    }
#elif defined( Q_WS_MACX )
    if (dir.isEmpty() || !check_ldrawdir(dir)) {
        QStringList macdirs;
        macdirs << QLatin1String("/Library/LDRAW")
                << QLatin1String("/Library/ldraw")
                << QLatin1String("~/Library/LDRAW")
                << QLatin1String("~/Library/ldraw");
        QString homepath = QDir::homePath();

        foreach (QString d, macdirs) {
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


LDraw::Core *LDraw::Core::s_inst = 0;

LDraw::Core *LDraw::Core::create(const QString &datadir, QString *errstring)
{
    if (!s_inst) {
        QString error;
        QString ldrawdir = datadir;

        if (ldrawdir.isEmpty())
            ldrawdir = get_platform_ldrawdir();

        if (check_ldrawdir(ldrawdir)) {
            s_inst = new Core(ldrawdir);

            if (s_inst->parse_ldconfig("ldconfig.ldr")) {
                s_inst->parse_ldconfig("ldconfig_missing.ldr");

                if (s_inst->create_part_list()) {
                    ;
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
            s_inst = 0;

            if (errstring)
                *errstring = error;
        }
    }

    return s_inst;
}

LDraw::Core::Core(const QString &datadir)
    : m_datadir(datadir)
{
    const char *subdirs[] =
#if defined( Q_OS_UNIX ) && !defined( Q_OS_MACX)
        { "P", "p", "PARTS", "parts", "MODELS", "models", 0 };
#else
        { "P", "PARTS", "MODELS", 0 };
#endif

    for (const char **ptr = subdirs; *ptr; ++ptr) {
        QDir sdir(m_datadir);

        if (sdir.cd(QLatin1String(*ptr)))
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
                char *nl = reinterpret_cast<char *>(memchr(p, '\n', l));
                if (nl)
                    l = nl - p;
                name = QByteArray(p + 2, l - 2).simplified();
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
        return QColor(str);
    else
        return QColor();
}

bool LDraw::Core::parse_ldconfig(const char *filename)
{
    stopwatch sw("LDraw: parse_ldconfig");

    QDir ldrawdir(dataPath());
    QFile f(ldrawdir.filePath(QLatin1String(filename)));

    qWarning("trying file: %s", qPrintable(f.fileName()));

    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("got file");

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

                int id = sl[4].toInt();
                QColor main = parse_color_string(sl[6]);
                QColor edge = parse_color_string(sl[8]);

                // edge is not a RGB color, but a color index (which could be not parsed yet...)
                if (!edge.isValid()) {
                    edge_ids.insert(id, sl[8].toInt());
                    edge = Qt::green;
                }

                for (int idx = 9; idx < sl.count(); ++idx) {
                    if (sl[idx] == QLatin1String("ALPHA")) {
                        int alpha = sl[idx+1].toInt();
                        main.setAlpha(alpha);
                        edge.setAlpha(alpha);
                        idx++;
                    }
                    else if (sl[idx] == QLatin1String("LUMINANCE")) {
                        int lum = sl[idx+1].toInt();
                        lum = lum;
                        idx++;
                    }
                    else if (sl[idx] == QLatin1String("CHROME")) {
                    }
                    else if (sl[idx] == QLatin1String("PEARLESCENT")) {
                    }
                    else if (sl[idx] == QLatin1String("RUBBER")) {
                    }
                    else if (sl[idx] == QLatin1String("MATTE_METALLIC")) {
                    }
                    else if (sl[idx] == QLatin1String("METAL")) {
                    }
                }
                if (main.isValid() && edge.isValid()) {
                    m_colors.insert(id, qMakePair<QColor, QColor>(main, edge));

                    //qDebug() << "Got Color " << id << " : " << main << " // " << edge;
                }
            }
        }
        for (QMap<int, int>::const_iterator it = edge_ids.begin(); it != edge_ids.end(); ++it) {
            if (m_colors.contains(it.key()) && m_colors.contains(it.value())) {
                m_colors[it.key()].second = m_colors[it.value()].first;
                //qDebug() << "Fixed Edge " << it.key() << " : " << m_colors[it.key()].first << " // " << m_colors[it.key()].second;
            }
        }

        return true;
    }
    return false;
}


QColor LDraw::Core::edgeColor(int id) const
{
    if (m_colors.contains(id)) {
        return m_colors.value(id).second;
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
    if (m_colors.contains(id)) {
        return m_colors.value(id).first;
    }
    else if (baseid < 0 && (id == 16 || id == 24)) {
        qWarning("Called Core::color() with meta color id 16 or 24 without specifing the meta color");
        return QColor();
    }
    else if (id == 16) {
        return color(baseid);
    }
    else if (id == 24) {
        return edgeColor(baseid);
    }
    else if (id >= 256) {
        // dithered color (do a normal blend - DOS times are over by now)

        QColor c1 = color((id - 256) & 0x0f);
        QColor c2 = color(((id - 256) >> 4) & 0x0f);

        int r1, g1, b1, a1, r2, g2, b2, a2;
        c1.getRgb(&r1, &g1, &b1, &a1);
        c2.getRgb(&r2, &g2, &b2, &a2);

        return QColor::fromRgb((r1+r2) >> 1, (g1+g2) >> 1, (b1+b2) >> 1, (a1+a2) >> 1);
    }
    else {
        //qWarning("Called Core::color() with an invalid LDraw color id: %d", id);
        return QColor::fromRgbF(qreal(qrand()) / RAND_MAX, qreal(qrand()) / RAND_MAX, qreal(qrand()) / RAND_MAX);
    }
}



