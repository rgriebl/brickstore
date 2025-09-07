// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QSettings>
#include <QDateTime>
#include <QLocale>
#include <QVector>
#include <QSet>


class Config : public QSettings
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QLocale::MeasurementSystem measurementSystem READ measurementSystem WRITE setMeasurementSystem NOTIFY measurementSystemChanged)
    Q_PROPERTY(QString defaultCurrencyCode READ defaultCurrencyCode WRITE setDefaultCurrencyCode NOTIFY defaultCurrencyCodeChanged)
    Q_PROPERTY(QString documentDir READ documentDir WRITE setDocumentDir NOTIFY documentDirChanged)
    Q_PROPERTY(Config::PartOutMode partOutMode READ partOutMode WRITE setPartOutMode NOTIFY partOutModeChanged)
    Q_PROPERTY(bool openBrowserOnExport READ openBrowserOnExport WRITE setOpenBrowserOnExport NOTIFY openBrowserOnExportChanged)
    Q_PROPERTY(bool restoreLastSession READ restoreLastSession WRITE setRestoreLastSession NOTIFY restoreLastSessionChanged)
    Q_PROPERTY(bool showInputErrors READ showInputErrors WRITE setShowInputErrors NOTIFY showInputErrorsChanged)
    Q_PROPERTY(bool showDifferenceIndicators READ showDifferenceIndicators WRITE setShowDifferenceIndicators NOTIFY showDifferenceIndicatorsChanged)
    Q_PROPERTY(QString brickLinkAccessToken READ brickLinkAccessToken WRITE setBrickLinkAccessToken NOTIFY brickLinkAccessTokenChanged)
    Q_PROPERTY(Config::UITheme uiTheme READ uiTheme WRITE setUITheme NOTIFY uiThemeChanged)
    Q_PROPERTY(Config::UISize mobileUISize READ mobileUISize WRITE setMobileUISize NOTIFY mobileUISizeChanged)
    Q_PROPERTY(int rowHeightPercent READ rowHeightPercent WRITE setRowHeightPercent NOTIFY rowHeightPercentChanged)
    Q_PROPERTY(int fontSizePercent READ fontSizePercent WRITE setFontSizePercent NOTIFY fontSizePercentChanged)
    Q_PROPERTY(int iconSizePercent READ iconSizePercent WRITE setIconSizePercent NOTIFY iconSizePercentChanged)
    Q_PROPERTY(int columnSpacing READ columnSpacing WRITE setColumnSpacing NOTIFY columnSpacingChanged)
    Q_PROPERTY(bool liveEditRowHeight READ liveEditRowHeight WRITE setLiveEditRowHeight NOTIFY liveEditRowHeightChanged)

private:
    Config();
    static Config *s_inst;

public:
    ~Config() override;
    static Config *inst();

    void upgrade(int vmajor, int vminor, int vpatch);

    QVariantList availableLanguages() const;
    QString language() const;
    void setLanguage(const QString &lang);
    QLocale::MeasurementSystem measurementSystem() const;
    void setMeasurementSystem(QLocale::MeasurementSystem ms);

    struct Translation {
        QString language;
        QString name;
        QString localName;
        QString flagPath;
        QString author;
        QString authorEmail;
    };

    QVector<Translation> translations() const;

    QString defaultCurrencyCode() const;
    void setDefaultCurrencyCode(const QString &ccode);

    QString documentDir() const;
    void setDocumentDir(const QString &dir);
    QString ldrawDir() const;
    void setLDrawDir(const QString &dir);
    QString cacheDir() const;

    bool showInputErrors() const;
    void setShowInputErrors(bool b);
    bool showDifferenceIndicators() const;
    void setShowDifferenceIndicators(bool b);

    enum class PartOutMode {
        Ask,
        InPlace,
        NewDocument
    };
    Q_ENUM(PartOutMode)

    PartOutMode partOutMode() const;
    void setPartOutMode(PartOutMode pom);

    bool visualChangesMarkModified() const;
    void setVisualChangesMarkModified(bool b);

    bool restoreLastSession() const;
    void setRestoreLastSession(bool b);

    bool openBrowserOnExport() const;
    void setOpenBrowserOnExport(bool b);

    static constexpr int MaxFilterHistory = 20;

    QString brickLinkAccessToken() const;
    void setBrickLinkAccessToken(const QString &accessToken);
    QMap<QByteArray, int> updateIntervals() const;
    QMap<QByteArray, int> updateIntervalsDefault() const;
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

    enum class UISize {
        System,
        Small,
        Large,
    };
    Q_ENUM(UISize)

    UISize toolBarSize() const;
    void setToolBarSize(UISize tbSize);
    int iconSizePercent() const;
    void setIconSizePercent(int p);
    int fontSizePercent() const;
    void setFontSizePercent(int p);
    int rowHeightPercent() const;
    void setRowHeightPercent(int p);
    bool liveEditRowHeight() const;
    void setLiveEditRowHeight(bool newLiveEditRowHeight);
    int columnSpacing() const;
    void setColumnSpacing(int newColumnSpacing);

    QByteArray columnLayout(const QString &id) const;
    QString columnLayoutName(const QString &id) const;
    int columnLayoutOrder(const QString &id) const;
    QStringList columnLayoutIds() const;
    QString setColumnLayout(const QString &id, const QByteArray &layout);
    bool deleteColumnLayout(const QString &id);
    bool renameColumnLayout(const QString &id, const QString &name);
    bool reorderColumnLayouts(const QStringList &ids);

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

    enum class UITheme {
        SystemDefault,
        Light,
        Dark
    };
    Q_ENUM(UITheme)

    UITheme uiTheme() const;
    void setUITheme(Config::UITheme theme);
    UISize mobileUISize() const;
    void setMobileUISize(Config::UISize size);

    QSet<uint> pinnedColorIds() const;
    void setPinnedColorIds(const QSet<uint> &colors);
    QSet<uint> pinnedCategoryIds() const;
    void setPinnedCategoryIds(const QSet<uint> &categories);

signals:
    void languageChanged();
    void measurementSystemChanged(QLocale::MeasurementSystem ms);
    void documentDirChanged(const QString &dir);
    void ldrawDirChanged(const QString &dir);
    void restoreLastSessionChanged(bool b);
    void partOutModeChanged(Config::PartOutMode partMode);
    void defaultCurrencyCodeChanged(const QString &ccode);
    void openBrowserOnExportChanged(bool b);
    void showInputErrorsChanged(bool b);
    void showDifferenceIndicatorsChanged(bool b);
    void visualChangesMarkModifiedChanged(bool b);
    void updateIntervalsChanged(const QMap<QByteArray, int> &intervals);
    void toolBarSizeChanged(Config::UISize iconSize);
    void iconSizePercentChanged(int p);
    void fontSizePercentChanged(int p);
    void rowHeightPercentChanged(int p);
    void columnLayoutChanged(const QString &id, const QByteArray &layout);
    void columnLayoutNameChanged(const QString &id, const QString &name);
    void columnLayoutIdsOrderChanged(const QStringList &ids);
    void columnLayoutIdsChanged(const QStringList &ids);
    void shortcutsChanged(const QVariantMap &list);
    void sentryConsentChanged(Config::SentryConsent consent);
    void toolBarActionsChanged(const QStringList &actions);
    void uiThemeChanged(Config::UITheme theme);
    void mobileUISizeChanged(Config::UISize size);
    void brickLinkAccessTokenChanged();
    void columnSpacingChanged(int spacing);
    void liveEditRowHeightChanged(bool liveEdit);
    void pinnedColorIdsChanged();
    void pinnedCategoryIdsChanged();
    void pinnedRecentFilesChanged();

protected:
    bool parseTranslations() const;

private:
    bool                       m_show_input_errors = false;
    bool                       m_show_difference_indicators = false;
    int                        m_columnSpacing = 0;
    bool                       m_liveEditRowHeight = false;
    int                        m_rowHeightPercent = 100;
    QLocale::MeasurementSystem m_measurement = QLocale::MetricSystem;
    mutable bool               m_translations_parsed = false;
    mutable QVector<Translation> m_translations;
    QString                    m_lastDirectory;
    mutable QString            m_brickLinkAccessToken;
};

Q_DECLARE_METATYPE(Config *)
