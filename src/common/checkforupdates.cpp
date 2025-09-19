// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QEvent>
#include <QNetworkReply>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTimer>

#include <QCoro/QCoroSignal>
#include <QCoro/QCoroNetworkReply>
#include <QCoro/QCoroTimer>

#include "common/config.h"
#include "common/uihelpers.h"
#include "checkforupdates.h"

using namespace std::chrono_literals;


CheckForUpdates *CheckForUpdates::s_inst = nullptr;

CheckForUpdates::~CheckForUpdates()
{
    s_inst = nullptr;
}

CheckForUpdates *CheckForUpdates::inst()
{
    if (!s_inst)
        s_inst = new CheckForUpdates();
    return s_inst;
}

CheckForUpdates::CheckForUpdates(QObject *parent)
    : QObject(parent)
    , m_currentVersion(QVersionNumber::fromString(QCoreApplication::applicationVersion()))
{
    //TESTING m_currentVersion = QVersionNumber::fromString(u"2023.11.2"_qs);
}

void CheckForUpdates::initialize(const QString &baseUrl, Mode mode)
{
    if (m_initialized)
        return;
    m_initialized = true;
    m_mode = mode;

    m_checkUrl = baseUrl;
    m_checkUrl.replace(u"github.com"_qs, u"https://api.github.com/repos"_qs);
    m_checkUrl.append(u"/releases/latest"_qs);

    m_changelogUrl = baseUrl;
    m_changelogUrl.replace(u"github.com"_qs, u"https://raw.githubusercontent.com"_qs);
    m_changelogUrl.append(u"/main/CHANGELOG.md"_qs);

    m_releaseUrl = baseUrl;
    m_releaseUrl.prepend(u"https://"_qs);
    m_releaseUrl.append(u"/releases/tag/v%1"_qs);

    if (m_mode == Mode::NotifyAfterUpdate) {
        m_lastRunVersion = QVersionNumber::fromString(
            Config::inst()->value(u"General/LastRunVersion"_qs).toString());
        Config::inst()->setValue(u"General/LastRunVersion"_qs, m_currentVersion.toString());

        //TESTING m_lastRunVersion = QVersionNumber::fromString(u"2023.11.2"_qs);

        if (m_lastRunVersion < m_currentVersion) {
            QTimer::singleShot(2s, this, [this]() -> QCoro::Task<> {
                QString md = co_await versionChangeLog(m_lastRunVersion, m_currentVersion);
                if (!md.isEmpty()) {
                    md.prepend(changeLogHeader(tr("BrickStore was updated:"), m_currentVersion));
                    const auto releaseUrl = m_releaseUrl.arg(m_currentVersion.toString());

                    emit versionWasUpdated(m_currentVersion, md, releaseUrl);
                }
            });
        }
    } else { // Mode::NotifyBeforeUpdate
        QTimer::singleShot(2s, this, [this]() { check(true /*silent*/); });

        auto checkTimer = new QTimer(this);
        checkTimer->callOnTimeout(this, [this]() { check(true /*silent*/); });
        checkTimer->start(24h);
    }
}

QString CheckForUpdates::changeLogHeader(const QString &title, const QVersionNumber &version)
{
    const QString changes = tr("Changes:");
    const QString top = u"## %1 %2\n---\n## %3\n"_qs.arg(title, version.toString(), changes);
    return top;
}

QCoro::Task<> CheckForUpdates::check(bool silent)
{
    if (!m_initialized)
        co_return;

    if (m_mode == Mode::NotifyAfterUpdate)
        co_return;

    if (m_checking) {
        m_silent = silent;
        co_return;
    }
    m_checking = true;
    m_silent = silent;

    QNetworkReply *reply = co_await m_nam.get(QNetworkRequest(m_checkUrl));

    reply->deleteLater();

    QByteArray data = reply->readAll();
    QVersionNumber latestVersion;

    QJsonParseError jsonError;
    auto doc = QJsonDocument::fromJson(data, &jsonError);
    if (doc.isNull()) {
        qWarning() << data.constData();
        qWarning() << "\nCould not parse GitHub JSON reply:" << jsonError.errorString();
    } else {
        QString tag = doc[u"tag_name"].toString();
        if (tag.startsWith(u'v'))
            tag.remove(0, 1);
        latestVersion = QVersionNumber::fromString(tag);
        if (latestVersion.isNull())
            qWarning() << "Cannot parse GitHub's latest tag_name:" << tag;
        const auto assets = doc[u"assets"].toArray();
        m_installerUrl.clear();
        for (const QJsonValueConstRef asset : assets) {
            QString name = asset[u"name"].toString();
            bool match = false;
#if defined(Q_OS_MACOS)
            if (name.startsWith(u"macOS-", Qt::CaseInsensitive)) {
                bool dlLegacy = name.startsWith(u"macOS-10-Legacy-", Qt::CaseInsensitive);
                bool osLegacy = QSysInfo::productVersion().startsWith(u"10.");
                match = (dlLegacy == osLegacy);
            }
#elif defined(Q_OS_WINDOWS) && defined(_M_AMD64)
            match = name.startsWith(u"Windows-x64-", Qt::CaseInsensitive)
                    || name.startsWith(u"Windows-Intel64-", Qt::CaseInsensitive);
#elif defined(Q_OS_WINDOWS) && defined(_M_ARM64)
            match = name.startsWith(u"Windows-ARM64-", Qt::CaseInsensitive);
#endif
            if (match) {
                m_installerName = name;
                m_installerUrl = asset[u"browser_download_url"].toString();
                break;
            }
        }
    }

    const auto ignoreVersion = QVersionNumber::fromString(
        Config::inst()->value(u"General/IgnoreUpdateVersion"_qs).toString());

    if (!m_silent || ((latestVersion > m_currentVersion)
                      && (ignoreVersion != latestVersion))) {
        QTimer t;
        t.start(0);
        co_await t;

        if (latestVersion.isNull()) {
            co_await UIHelpers::warning(tr("Version information is not available."));
        } else if (latestVersion <= m_currentVersion) {
            co_await UIHelpers::information(tr("Your currently installed version is up-to-date."));
        } else {
            QString md = co_await versionChangeLog(m_currentVersion, latestVersion);
            if (!md.isEmpty()) {
                md.prepend(changeLogHeader(tr("A newer version than the one currently installed is available:"),
                                           latestVersion));
                const auto releaseUrl = m_releaseUrl.arg(latestVersion.toString());

                emit versionCanBeUpdated(latestVersion, md, releaseUrl,
                                         m_installerName, m_installerUrl);
            }
        }
    }
    m_checking = false;
}

QCoro::Task<QString> CheckForUpdates::versionChangeLog(QVersionNumber fromVersion,
                                                       QVersionNumber toVersion)
{
    QNetworkReply *reply = co_await m_nam.get(QNetworkRequest(m_changelogUrl));

    reply->deleteLater();

    QString md = QString::fromUtf8(reply->readAll());
    static const QRegularExpression header(uR"(^## \[([0-9.]+)\] - \d{4}-\d{2}-\d{2}$)"_qs,
                                           QRegularExpression::MultilineOption);

    qsizetype fromHeader = 0;
    qsizetype toHeader = 0;
    qsizetype nextHeader = 0;

    while (!fromHeader || !toHeader) {
        QRegularExpressionMatch match = header.match(md, nextHeader);
        if (!match.hasMatch())
            break;

        // Qt cannot format links in headers
        QString s = u"\n##  \n## Version %1  \n"_qs.arg(match.captured(1));
        md.replace(match.capturedStart(), match.capturedLength(), s);

        if (match.captured(1) == fromVersion.toString())
            fromHeader = match.capturedStart();
        else if (match.captured(1) == toVersion.toString())
            toHeader = match.capturedStart();

        nextHeader = match.capturedEnd();
    }
    if ((toHeader > 0) && (fromHeader > toHeader))
        co_return md.mid(toHeader, fromHeader - toHeader);

    co_return { };
}

#include "moc_checkforupdates.cpp"
