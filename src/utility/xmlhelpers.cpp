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

#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDebug>

#include "utility.h"
#include "exception.h"
#include "xmlhelpers.h"



QString XmlHelpers::decodeEntities(const QString &src)
{
    // Regular Expressions would be easier, but too slow here

    QString decoded(src);

    int pos = decoded.indexOf("&#"_l1);
    if (pos < 0)
        return decoded;

    do {
        int endpos = decoded.indexOf(QLatin1Char(';'), pos + 2);
        if (endpos < 0) {
            pos += 2;
        } else {
            int unicode = decoded.mid(pos + 2, endpos - pos - 2).toInt();
            if (unicode > 0) {
                decoded.replace(pos, endpos - pos + 1, QChar(unicode));
                pos++;
            } else {
                pos = endpos + 1;
            }
        }
        pos = decoded.indexOf("&#"_l1, pos);
    } while (pos >= 0);

    return decoded;
}

char XmlHelpers::firstCharInString(const QString &str)
{
    return (str.size() == 1) ? str.at(0).toLatin1() : 0;
}



XmlHelpers::ParseXML::ParseXML(const QString &path, const char *rootNodeName, const char *elementNodeName)
    : ParseXML(openFile(path), rootNodeName, elementNodeName)
{ }


QIODevice *XmlHelpers::ParseXML::openFile(const QString &fileName)
{
    std::unique_ptr<QFile> f { new QFile(fileName) };
    if (!f->open(QFile::ReadOnly))
        throw ParseException(f.get(), "could not open file");
    return f.release();
}

XmlHelpers::ParseXML::ParseXML(QIODevice *file, const char *rootNodeName, const char *elementNodeName)
    : m_rootNodeName(QLatin1String(rootNodeName))
    , m_elementNodeName(QLatin1String(elementNodeName))
    , m_file(file)
{ }

XmlHelpers::ParseXML::~ParseXML()
{
    delete m_file;
}

void XmlHelpers::ParseXML::parse(std::function<void (QDomElement)> callback,
                                 std::function<void (QDomElement)> rootCallback)
{
    QDomDocument doc;
    QString emsg;
    int eline = 0;
    int ecolumn = 0;
    if (!doc.setContent(m_file, false, &emsg, &eline, &ecolumn)) {
        throw ParseException(m_file, "%1 at line %2, column %3")
                .arg(emsg).arg(eline).arg(ecolumn);
    }

    auto root = doc.documentElement().toElement();
    if (root.nodeName() != m_rootNodeName) {
        throw ParseException(m_file, "expected root node %1, but got %2")
                .arg(m_rootNodeName).arg(root.nodeName());
    }

    try {
        if (rootCallback)
            rootCallback(root.toElement());

        for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
            if (node.nodeName() == m_elementNodeName)
                callback(node.toElement());
        }
    } catch (const Exception &e) {
        throw ParseException(m_file, e.what());
    }
}

QString XmlHelpers::ParseXML::elementText(QDomElement parent, const char *tagName)
{
    auto dnl = parent.elementsByTagName(QString::fromLatin1(tagName));
    if (dnl.size() != 1) {
        throw ParseException("Expected a single %1 tag, but found %2")
                .arg(QLatin1String(tagName)).arg(dnl.size());
    }
    // the contents are double XML escaped. QDom unescaped once already, now have to do one more
    return decodeEntities(dnl.at(0).toElement().text().trimmed());
}

QString XmlHelpers::ParseXML::elementText(QDomElement parent, const char *tagName,
                                          const char *defaultText)
{
    try {
        return elementText(parent, tagName);
    } catch (...) {
        return QLatin1String(defaultText);
    }
}



XmlHelpers::CreateXML::CreateXML(const char *rootNodeName, const char *elementNodeName)
    : m_domDoc(QString { })
    , m_elementNodeName(QLatin1String(elementNodeName))
{
    m_domRoot = m_domDoc.createElement(QLatin1String(rootNodeName));
    m_domDoc.appendChild(m_domRoot);
}

void XmlHelpers::CreateXML::createElement()
{
    m_domItem = m_domDoc.createElement(m_elementNodeName);
    m_domRoot.appendChild(m_domItem);
}

void XmlHelpers::CreateXML::createText(const char *tagName, QStringView value)
{
    m_domItem.appendChild(m_domDoc.createElement(QLatin1String(tagName))
                          .appendChild(m_domDoc.createTextNode(value.toString())).parentNode());
}

void XmlHelpers::CreateXML::createEmpty(const char *tagName)
{
    m_domItem.appendChild(m_domDoc.createElement(QLatin1String(tagName)));
}

QString XmlHelpers::CreateXML::toString() const
{
    return m_domDoc.toString();
}

QByteArray XmlHelpers::CreateXML::toUtf8() const
{
    return m_domDoc.toByteArray();
}
