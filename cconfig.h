/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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


class CConfig : public QSettings {
    Q_OBJECT

private:
    CConfig();
    static CConfig *s_inst;

    static QString obscure(const QString &);

public:
    ~CConfig();
    static CConfig *inst();

    void upgrade(int vmajor, int vminor, int vrev);

    enum WeightSystem {
        WeightMetric,
        WeightImperial
    };

    QString language() const;
    WeightSystem weightSystem() const;

    bool closeEmptyDocuments() const;
    QString documentDir() const;
    QString lDrawDir() const;
    QString dataDir() const;

    bool showInputErrors() const;
    bool simpleMode() const;
    bool onlineStatus() const;
    QNetworkProxy proxy() const;

    QDateTime lastDatabaseUpdate() const;

    QString blLoginUsername() const;
    QString blLoginPassword() const;
    void blUpdateIntervals(int &pic, int &pg) const;
    void blUpdateIntervalsDefaults(int &picd, int &pgd) const;

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
    void setWeightSystem(WeightSystem ws);

    void setCloseEmptyDocuments(bool b);
    void setDocumentDir(const QString &dir);
    void setLDrawDir(const QString &dir);
    void setDataDir(const QString &dir);

    void setShowInputErrors(bool b);
    void setSimpleMode(bool sm);
    void setOnlineStatus(bool b);
    void setProxy(const QNetworkProxy &proxy);

    void setLastDatabaseUpdate(const QDateTime &dt);

    void setBlLoginUsername(const QString &name);
    void setBlLoginPassword(const QString &pass);
    void setBlUpdateIntervals(int pic, int pg);

signals:
    void simpleModeChanged(bool);
    void languageChanged();
    void weightSystemChanged(CConfig::WeightSystem ws);
    void showInputErrorsChanged(bool b);
    void blUpdateIntervalsChanged(int pic, int pg);
    void onlineStatusChanged(bool b);
    void proxyChanged(const QNetworkProxy &proxy);
    void registrationChanged(CConfig::Registration r);

protected:
    QString readPasswordEntry(const QString &key) const;
    void writePasswordEntry(const QString &key, const QString &password);

private:
    bool         m_show_input_errors;
    WeightSystem m_weight_system;
    bool         m_simple_mode;
    Registration m_registration;
};

#endif
