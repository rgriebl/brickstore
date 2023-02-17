// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>

#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QIODevice)


namespace XmlHelpers {

class ParseXML
{
public:
    ParseXML(const QString &path, const char *rootNodeName, const char *elementNodeName);
    ParseXML(QIODevice *file, const char *rootNodeName, const char *elementNodeName);
    ~ParseXML();

    void parse(std::function<void (const QDomElement &)> callback,
               std::function<void (const QDomElement &)> rootCallback = { });
    static QString elementText(const QDomElement &parent, const char *tagName);
    static QString elementText(const QDomElement &parent, const char *tagName, const char *defaultText);

private:
    static QIODevice *openFile(const QString &fileName);

    QString m_rootNodeName;
    QString m_elementNodeName;
    QIODevice *m_file;
    QDomElement m_root;

    Q_DISABLE_COPY(ParseXML)
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

    Q_DISABLE_COPY(CreateXML)
};

}
