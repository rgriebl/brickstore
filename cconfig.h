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
#ifndef __CCONFIG_H__
#define __CCONFIG_H__

#include <QSettings>
#include <QDateTime>
#include <QNetworkProxy>
#include <QLocale>


class CConfig : public QSettings {
    Q_OBJECT

private:
    CConfig();
    static CConfig *s_inst;

    static QString scramble(const QString &);

public:
    ~CConfig();
    static CConfig *inst();

    void upgrade(int vmajor, int vminor, int vrev);

    QString language() const;
    QLocale::MeasurementSystem measurementSystem() const;
    inline bool isMeasurementMetric()    { return measurementSystem() == QLocale::MetricSystem;   }
    inline bool isMeasurementImperial()  { return measurementSystem() == QLocale::ImperialSystem; }

    bool closeEmptyDocuments() const;
    QString documentDir() const;
    QString lDrawDir() const;
    QString dataDir() const;

    bool showInputErrors() const;
    bool simpleMode() const;
    bool onlineStatus() const;
    QNetworkProxy proxy() const;

    QDateTime lastDatabaseUpdate() const;

    QPair<QString, QString> loginForBrickLink() const;
    QMap<QByteArray, int> updateIntervals() const;
    QMap<QByteArray, int> updateIntervalsDefault() const;

    struct Translation {
        QString m_langid;
        QString m_translator;
        QString m_contact;
        bool    m_default;
        QMap<QString, QString> m_names;
    };

    QList<Translation> translations() const;

    enum Registration {
        None,
        Personal,
        Demo,
        Full,
        OpenSource
    };

    Registration registration() const;
    Registration setRegistration(const QString &name, const QString &key);
    QString registrationName() const;
    QString registrationKey() const;
    bool checkRegistrationKey(const QString &name, const QString &key);

public slots:
    void setLanguage(const QString &lang);
    void setMeasurementSystem(QLocale::MeasurementSystem ms);

    void setCloseEmptyDocuments(bool b);
    void setDocumentDir(const QString &dir);
    void setLDrawDir(const QString &dir);
    void setDataDir(const QString &dir);

    void setShowInputErrors(bool b);
    void setSimpleMode(bool sm);
    void setOnlineStatus(bool b);
    void setProxy(const QNetworkProxy &proxy);

    void setLastDatabaseUpdate(const QDateTime &dt);

    void setLoginForBrickLink(const QString &user, const QString &pass);
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

signals:
    void simpleModeChanged(bool);
    void languageChanged();
    void measurementSystemChanged(QLocale::MeasurementSystem ms);
    void showInputErrorsChanged(bool b);
    void updateIntervalsChanged(const QMap<QByteArray, int> &intervals);
    void onlineStatusChanged(bool b);
    void proxyChanged(const QNetworkProxy &proxy);
    void registrationChanged(CConfig::Registration r);

protected:
    bool parseTranslations() const;

private:
    bool         m_show_input_errors;
    QLocale::MeasurementSystem m_measurement;
    bool         m_simple_mode;
    Registration m_registration;
    mutable bool m_translations_parsed;
    mutable QList<Translation> m_translations;
};

#endif
