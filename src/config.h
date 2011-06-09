/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <QSettings>
#include <QDateTime>
#include <QNetworkProxy>
#include <QLocale>


class Config : public QSettings {
    Q_OBJECT

private:
    Config();
    static Config *s_inst;

    static QString scramble(const QString &);

public:
    ~Config();
    static Config *inst();

    void upgrade(int vmajor, int vminor, int vrev);

    QString language() const;
    QLocale::MeasurementSystem measurementSystem() const;
    inline bool isMeasurementMetric()    { return measurementSystem() == QLocale::MetricSystem;   }
    inline bool isMeasurementImperial()  { return measurementSystem() == QLocale::ImperialSystem; }

    QString defaultCurrencyCode() const;

    bool closeEmptyDocuments() const;
    QString documentDir() const;
    QString lDrawDir() const;
    QString dataDir() const;

    bool showInputErrors() const;
    bool onlineStatus() const;
    QNetworkProxy proxy() const;

    QDateTime lastDatabaseUpdate() const;

    QPair<QString, QString> loginForBrickLink() const;
    QMap<QByteArray, int> updateIntervals() const;
    QMap<QByteArray, int> updateIntervalsDefault() const;

    struct Translation {
        QString language;
        QString author;
        QString authorEMail;
        QMap<QString, QString> languageName;
    };

    QList<Translation> translations() const;

public slots:
    void setLanguage(const QString &lang);
    void setMeasurementSystem(QLocale::MeasurementSystem ms);
    void setDefaultCurrencyCode(const QString &ccode);

    void setCloseEmptyDocuments(bool b);
    void setDocumentDir(const QString &dir);
    void setLDrawDir(const QString &dir);
    void setDataDir(const QString &dir);

    void setShowInputErrors(bool b);
    void setOnlineStatus(bool b);
    void setProxy(const QNetworkProxy &proxy);

    void setLastDatabaseUpdate(const QDateTime &dt);

    void setLoginForBrickLink(const QString &user, const QString &pass);
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

signals:
    void languageChanged();
    void measurementSystemChanged(QLocale::MeasurementSystem ms);
    void defaultCurrencyCodeChanged(const QString &ccode);
    void showInputErrorsChanged(bool b);
    void updateIntervalsChanged(const QMap<QByteArray, int> &intervals);
    void onlineStatusChanged(bool b);
    void proxyChanged(const QNetworkProxy &proxy);

protected:
    bool parseTranslations() const;

private:
    bool                       m_show_input_errors;
    QLocale::MeasurementSystem m_measurement;
    mutable bool               m_translations_parsed;
    mutable QList<Translation> m_translations;
};

#endif
