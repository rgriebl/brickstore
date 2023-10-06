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

    void parse(const std::function<void (const QDomElement &)> &callback,
               const std::function<void (const QDomElement &)> &rootCallback = { });
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

}
