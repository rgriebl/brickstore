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
#include <QStringList>
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

LDraw::Element *LDraw::Element::fromString(const QString &line, ParseContext *cache)
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

    QStringList sl = line.simplified().split(' ');

    if (!sl.isEmpty()) {
        t = sl[0].toInt();
        sl.removeFirst();

        if (t >= 0 && t <= 5) {
            if (!t || element_count_lut[t] == sl.size()) {
                switch (t) {
                case 0: {
                    e = CommentElement::create(sl.join(QLatin1String(" ")));
                    break;
                }
                case 1: {
                    int color = sl[0].toInt();
                    matrix_t m;
                    m[0][0] = sl[4].toDouble();
                    m[0][1] = sl[7].toDouble();
                    m[0][2] = sl[10].toDouble();
                    m[0][3] = 0;
                    m[1][0] = sl[5].toDouble();
                    m[1][1] = sl[8].toDouble();
                    m[1][2] = sl[11].toDouble();
                    m[1][3] = 0;
                    m[2][0] = sl[6].toDouble();
                    m[2][1] = sl[9].toDouble();
                    m[2][2] = sl[12].toDouble();
                    m[2][3] = 0;
                    m[3][0] = sl[1].toDouble();
                    m[3][1] = sl[2].toDouble();
                    m[3][2] = sl[3].toDouble();
                    m[3][3] = 1;

                    e = PartElement::create(color, m, sl[13], cache);
                    break;
                }
                case 2: {
                    int color = sl[0].toInt();
                    vector_t v[2];
                    
                    for (int i = 0; i < 2; ++i) {
                        for (int j = 0; j < 3; ++j)
                            v[i][j] = sl[3*i + j + 1].toDouble();
                    }
                    e = LineElement::create(color, v);
                    break;
                }
                case 3: {
                    int color = sl[0].toInt();
                    vector_t v[3];
                    
                    for (int i = 0; i < 3; ++i) {
                        for (int j = 0; j < 3; ++j)
                            v[i][j] = sl[3*i + j + 1].toDouble();
                    }
                    e = TriangleElement::create(color, v);
                    break;
                }
                case 4: {
                    int color = sl[0].toInt();
                    vector_t v[4];
                    
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 3; ++j)
                            v[i][j] = sl[3*i + j + 1].toDouble();
                    }
                    e = QuadElement::create(color, v);
                    break;
                }
                case 5: {
                    int color = sl[0].toInt();
                    vector_t v[4];
                    
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 3; ++j)
                            v[i][j] = sl[3*i + j + 1].toDouble();
                    }
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



LDraw::CommentElement::CommentElement(const QString &text)
    : Element(Comment), m_comment(text)
{ }

LDraw::CommentElement *LDraw::CommentElement::create(const QString &text)
{
    return new CommentElement(text);
}

void LDraw::CommentElement::dump() const
{
    printf("0 %s\n", m_comment.toLatin1().constData());
}


LDraw::LineElement::LineElement(int color, const vector_t *v)
    : Element(Line), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LDraw::LineElement *LDraw::LineElement::create(int color, const vector_t *v)
{
    return new LineElement(color, v);
}

void LDraw::LineElement::dump() const
{
    printf("2 %d %.f %.f %.f %.f %.f %.f\n", m_color, m_points[0][0], m_points[0][1], m_points[0][2], m_points[1][0], m_points[1][1], m_points[1][2]);
}


LDraw::CondLineElement::CondLineElement(int color, const vector_t *v)
    : Element(CondLine), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LDraw::CondLineElement *LDraw::CondLineElement::create(int color, const vector_t *v)
{
    return new CondLineElement(color, v);
}

void LDraw::CondLineElement::dump() const
{
    printf("5 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f\n", m_color, m_points[0][0], m_points[0][1], m_points[0][2], m_points[1][0], m_points[1][1], m_points[1][2], m_points[2][0], m_points[2][1], m_points[2][2], m_points[3][0], m_points[3][1], m_points[3][2]);
}


LDraw::TriangleElement::TriangleElement(int color, const vector_t *v)
    : Element(Triangle), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LDraw::TriangleElement *LDraw::TriangleElement::create(int color, const vector_t *v)
{
    return new TriangleElement(color, v);
}

void LDraw::TriangleElement::dump() const
{
    printf("3 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f\n", m_color, m_points[0][0], m_points[0][1], m_points[0][2], m_points[1][0], m_points[1][1], m_points[1][2], m_points[2][0], m_points[2][1], m_points[2][2]);
}


LDraw::QuadElement::QuadElement(int color, const vector_t *v)
    : Element(Quad), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
#if 0
    //fix CCW order
    // getting vector_ts from points
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

LDraw::QuadElement *LDraw::QuadElement::create(int color, const vector_t *v)
{
    return new QuadElement(color, v);
}

void LDraw::QuadElement::dump() const
{
    printf("4 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f\n", m_color, m_points[0][0], m_points[0][1], m_points[0][2], m_points[1][0], m_points[1][1], m_points[1][2], m_points[2][0], m_points[2][1], m_points[2][2], m_points[3][0], m_points[3][1], m_points[3][2]);
}


LDraw::PartElement::PartElement(int color, const matrix_t &matrix, const QString &filename)
    : Element(Part), m_color(color), m_file(filename), m_matrix(matrix), m_no_partcache(true), m_part(0)
{
}

LDraw::PartElement::~PartElement()
{
    if (m_part && m_no_partcache)
        delete m_part;
}

LDraw::PartElement *LDraw::PartElement::create(int color, const matrix_t &matrix, const QString &filename, ParseContext *cache)
{
    PartElement *e = new PartElement(color, matrix, filename);
    e->m_no_partcache = (cache == 0);
    e->m_part = Part::parse(filename, cache);
    if (!e->m_part) {
        delete e;
        e = 0;
    }
    return e;
}

void LDraw::PartElement::dump() const
{
    if (!m_part) {
        printf("1 %d %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f %.f\n", m_color, m_matrix[3][0], m_matrix[3][1], m_matrix[3][2], m_matrix[0][0], m_matrix[1][0], m_matrix[2][0], m_matrix[0][1], m_matrix[1][1], m_matrix[2][1], m_matrix[0][2], m_matrix[1][2], m_matrix[2][2]);
    }
    else {
        m_part->dump();
    }
}


LDraw::Part::Part()
    : m_bounding_calculated(false)
{
}


LDraw::Part *LDraw::Part::fromFile(const QString &file, ParseContext *cache)
{
    return parse(file, cache);
}

LDraw::Part *LDraw::Part::parse(const QString &xfilename, ParseContext *ctx)
{
    QString filename = xfilename;
    filename.replace(QLatin1Char('\\'), QLatin1Char('/'));

    Part *p = ctx ? ctx->m_cache->value(filename) : 0;

    if (!p) {
        QFile f;

        if (ctx) {
            foreach (QDir sp, ctx->m_searchpath) {
                if (sp.exists(filename)) {
                    f.setFileName(sp.absoluteFilePath(filename));
                    break;
                }
            }
        }
        else
            f.setFileName(filename);
            
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            p = new Part();

            QString line;
            int lineno = 0;
            while (!ts.atEnd()) {
                line = ts.readLine();
                lineno++;
                Element *e = Element::fromString(line, ctx);
                if (e)
                    p->m_elements.append(e);
                else
                    qWarning("Could not parse line #%d: %s", lineno, qPrintable(line));
            } 

            if (p->m_elements.isEmpty()) {
                delete p;
                p = 0;
            }
        }
        if (p && ctx)
            ctx->m_cache->insert(filename, p);        
    }
    return p;
}

bool LDraw::Part::boundingBox(vector_t &vmin, vector_t &vmax)
{
    if (!m_bounding_calculated) {
        matrix_t matrix;
        m_bounding_min = vector_t(FLT_MAX, FLT_MAX, FLT_MAX);
        m_bounding_max = vector_t(FLT_MIN, FLT_MIN, FLT_MIN);

        calc_bounding_box(this, matrix, m_bounding_min, m_bounding_max);

        m_bounding_calculated = true;
    }
    vmin = m_bounding_min;
    vmax = m_bounding_max;
    
    return true;
}

void LDraw::Part::check_bounding(int cnt, const vector_t *v, const matrix_t &matrix, vector_t &vmin, vector_t &vmax)
{
    while (cnt--) {
        vector_t vm = matrix * (*v);

        vmin = vector_t(qMin(vmin[0], vm[0]), qMin(vmin[1], vm[1]), qMin(vmin[2], vm[2]));
        vmax = vector_t(qMax(vmax[0], vm[0]), qMax(vmax[1], vm[1]), qMax(vmax[2], vm[2]));
        
        v++;
    }
}



void LDraw::Part::calc_bounding_box(const Part *part, const matrix_t &matrix, vector_t &vmin, vector_t &vmax)
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

LDraw::Model::Model()
{ }

LDraw::Model *LDraw::Model::fromFile(const QString &file)
{
    Model *m = new Model();
    
    QString ldir = LDraw::core()->dataPath();
    ParseContext ctx;
    ctx.m_cache = &m->m_parts;
    
    if (!ldir.isEmpty()) {
        QDir dir(ldir);
        
        const char *subdirs[] = 
#if defined( Q_OS_UNIX ) && !defined( Q_OS_MACX)
            { "PARTS", "parts", "P", "p", "MODELS", "models", 0 };
#else
            { "PARTS", "P", "MODELS", 0 };
#endif

        for (const char **ptr = subdirs; *ptr; ++ptr) {
            QDir sdir(dir);
            
            if (sdir.cd(QLatin1String(*ptr)))
                ctx.m_searchpath << sdir;
        }        
    }
    
    QFileInfo fi(file);
    if (fi.exists()) {
        ctx.m_searchpath << fi.absolutePath();
    
        m->m_root = Part::parse(file, &ctx);
    }
    
    if (!m->m_root) {
        delete m;
        m = 0;
    }

    return m;
}

LDraw::Model::~Model()
{
    qDeleteAll(m_parts);
}

QVector<LDraw::Part *> LDraw::Model::parts() const
{
    QVector<Part *> v;
    v.reserve(m_parts.size());
    int idx = 0;
    for (QHashIterator<QString, Part *> it(m_parts); it.hasNext(); )
        v[idx++] = it.next().value();
    return v;
}

bool LDraw::Core::setDataPath(const QString &dir)
{
    if (check_ldrawdir(dir)) {
        m_datadir = dir;
        return true;
    }
    else
        return false;
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
    QString dir = ::getenv("LDRAWDIR");

#if defined( Q_WS_WIN )
    if (dir.isEmpty() || !check_ldrawdir(dir)) {
        QT_WA({
            WCHAR inidir [MAX_PATH];

            DWORD l = GetPrivateProfileStringW(L"LDraw", L"BaseDirectory", L"", inidir, MAX_PATH, L"ldraw.ini");
            if (l >= 0) {
                inidir [l] = 0;
                dir = QString::fromUtf16(inidir);
            }
        }, {
            char inidir [MAX_PATH];

            DWORD l = GetPrivateProfileStringA("LDraw", "BaseDirectory", "", inidir, MAX_PATH, "ldraw.ini");
            if (l >= 0) {
                inidir [l] = 0;
                dir = QString::fromLocal8Bit(inidir);
            }
        })
    }

    if (dir.isEmpty() || !check_ldrawdir(dir)) {
        HKEY skey, lkey;

        QT_WA({
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
                if (RegOpenKeyExW(skey, L"LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                    WCHAR regdir [MAX_PATH + 1];
                    DWORD regdirsize = MAX_PATH * sizeof(WCHAR);

                    if (RegQueryValueExW(lkey, L"InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                        regdir [regdirsize / sizeof(WCHAR)] = 0;
                        dir = QString::fromUtf16(regdir);
                    }
                    RegCloseKey(lkey);
                }
                RegCloseKey(skey);
            }
        }, {
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software", 0, KEY_READ, &skey) == ERROR_SUCCESS) {
                if (RegOpenKeyExA(skey, "LDraw", 0, KEY_READ, &lkey) == ERROR_SUCCESS) {
                    char regdir [MAX_PATH + 1];
                    DWORD regdirsize = MAX_PATH;

                    if (RegQueryValueExA(lkey, "InstallDir", 0, 0, (LPBYTE) &regdir, &regdirsize) == ERROR_SUCCESS) {
                        regdir [regdirsize] = 0;
                        dir = QString::fromLocal8Bit(regdir);
                    }
                    RegCloseKey(lkey);
                }
                RegCloseKey(skey);
            }
        })
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
{ }


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

LDraw::Model *LDraw::Core::itemModel(const char *id)
{
    Model *p = 0;

    if (m_items.contains(QByteArray(id))) {
        QDir ldrawdir(dataPath());
        if (ldrawdir.cd(QLatin1String("parts")) || ldrawdir.cd(QLatin1String("PARTS"))) {
            QString fn = QLatin1String(id) + QLatin1String(".dat");

            p = Model::fromFile(ldrawdir.filePath(fn));
        }
    }
    return p;
}
