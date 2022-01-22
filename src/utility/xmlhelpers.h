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
#pragma once

#include <functional>

#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QIODevice)


namespace XmlHelpers {

QString decodeEntities(const QString &src);

char firstCharInString(const QString &str);


class ParseXML
{
public:
    ParseXML(const QString &path, const char *rootNodeName, const char *elementNodeName);
    ParseXML(QIODevice *file, const char *rootNodeName, const char *elementNodeName);
    ~ParseXML();

    void parse(std::function<void(QDomElement)> callback,
               std::function<void(QDomElement)> rootCallback = { });
    static QString elementText(QDomElement parent, const char *tagName);
    static QString elementText(QDomElement parent, const char *tagName, const char *defaultText);

private:
    static QIODevice *openFile(const QString &fileName);

    QString m_rootNodeName;
    QString m_elementNodeName;
    QIODevice *m_file;
    QDomElement m_root;
};


class CreateXML
{
public:
    CreateXML(const char *rootNodeName, const char *elementNodeName);

    void createElement();
    void createText(const char *tagName, QStringView value);
    void createEmpty(const char *tagName);

    QString toString() const;
    QByteArray toUtf8() const;

private:
    QDomDocument m_domDoc;
    QDomElement m_domRoot;
    QDomElement m_domItem;
    QString m_elementNodeName;
};

}
