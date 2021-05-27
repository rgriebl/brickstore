/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include <QSettings>
#include <QDateTime>
#include <QLocale>
#include <QVector>


class Config : public QSettings
{
    Q_OBJECT
private:
    Config();
    static Config *s_inst;

    static QString scramble(const QString &);

public:
    ~Config() override;
    static Config *inst();

    void upgrade(int vmajor, int vminor, int vpatch);

    QString language() const;
    QLocale::MeasurementSystem measurementSystem() const;

    bool areFiltersInFavoritesMode() const;

    QPair<QString, double> legacyCurrencyCodeAndRate() const;
    QString defaultCurrencyCode() const;

    QString documentDir() const;
    QString ldrawDir() const;
    QString brickLinkCacheDir() const;

    bool showInputErrors() const;
    bool showDifferenceIndicators() const;
    bool onlineStatus() const;

    enum class PartOutMode {
        Ask,
        InPlace,
        NewDocument
    };

    PartOutMode partOutMode() const;
    void setPartOutMode(PartOutMode pom);

    bool visualChangesMarkModified() const;
    void setVisualChangesMarkModified(bool b);

    bool restoreLastSession() const;
    void setRestoreLastSession(bool b);

    bool openBrowserOnExport() const;
    void setOpenBrowserOnExport(bool b);

    QStringList recentFiles() const;

    static constexpr int MaxRecentFiles = 18;
    static constexpr int MaxFilterHistory = 20;

    QPair<QString, QString> brickLinkCredentials() const;
    QMap<QByteArray, int> updateIntervals() const;
    QMap<QByteArray, int> updateIntervalsDefault() const;

    struct Translation {
        QString language;
        QString author;
        QString authorEMail;
        QMap<QString, QString> languageName;
    };

    QVector<Translation> translations() const;

    QSize iconSize() const;
    int fontSizePercent() const;
    int itemImageSizePercent() const;

    QByteArray columnLayout(const QString &id) const;
    QString columnLayoutName(const QString &id) const;
    int columnLayoutOrder(const QString &id) const;
    QStringList columnLayoutIds() const;

    QVariantMap shortcuts() const;
    void setShortcuts(const QVariantMap &list);

    QStringList toolBarActions() const;
    void setToolBarActions(const QStringList &actions);

    QString lastDirectory() const;
    void setLastDirectory(const QString &dir);

    enum class SentryConsent {
        Unknown,
        Given,
        Revoked,
    };
    SentryConsent sentryConsent() const;
    void setSentryConsent(SentryConsent consent);

    enum class UiTheme {
        SystemDefault,
        Light,
        Dark
    };

    UiTheme uiTheme() const;
    void setUiTheme(UiTheme theme);

public slots:
    void setLanguage(const QString &lang);
    void setMeasurementSystem(QLocale::MeasurementSystem ms);
    void setFiltersInFavoritesMode(bool b);
    void setDefaultCurrencyCode(const QString &ccode);

    void setDocumentDir(const QString &dir);
    void setLDrawDir(const QString &dir);

    void setShowInputErrors(bool b);
    void setShowDifferenceIndicators(bool b);
    void setOnlineStatus(bool b);

    void setRecentFiles(const QStringList &recent);
    void addToRecentFiles(const QString &file);

    void setBrickLinkCredentials(const QString &user, const QString &pass);
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

    void setIconSize(const QSize &iconSize);
    void setFontSizePercent(int p);
    void setItemImageSizePercent(int p);

    QString setColumnLayout(const QString &id, const QByteArray &layout);
    bool deleteColumnLayout(const QString &id);
    bool renameColumnLayout(const QString &id, const QString &name);
    bool reorderColumnLayouts(const QStringList &ids);

signals:
    void languageChanged();
    void measurementSystemChanged(QLocale::MeasurementSystem ms);
    void filtersInFavoritesModeChanged(bool favoritesMode);
    void defaultCurrencyCodeChanged(const QString &ccode);
    void showInputErrorsChanged(bool b);
    void showDifferenceIndicatorsChanged(bool b);
    void visualChangesMarkModifiedChanged(bool b);
    void updateIntervalsChanged(const QMap<QByteArray, int> &intervals);
    void onlineStatusChanged(bool b);
    void recentFilesChanged(const QStringList &recent);
    void iconSizeChanged(const QSize &iconSize);
    void fontSizePercentChanged(int p);
    void itemImageSizePercentChanged(int p);
    void columnLayoutChanged(const QString &id, const QByteArray &layout);
    void columnLayoutNameChanged(const QString &id, const QString &name);
    void columnLayoutIdsOrderChanged(const QStringList &ids);
    void columnLayoutIdsChanged(const QStringList &ids);
    void shortcutsChanged(const QVariantMap &list);
    void sentryConsentChanged(Config::SentryConsent consent);
    void toolBarActionsChanged(const QStringList &actions);
    void uiThemeChanged(Config::UiTheme theme);

protected:
    bool parseTranslations() const;

private:
    bool                       m_show_input_errors = false;
    bool                       m_show_difference_indicators = false;
    QLocale::MeasurementSystem m_measurement = QLocale::MetricSystem;
    mutable bool               m_translations_parsed = false;
    mutable QVector<Translation> m_translations;
    QString                    m_lastDirectory;
    mutable QPair<QString, QString> m_bricklinkCredentials;
};
