// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QEvent>
#include <QNetworkReply>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QDir>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QMessageBox>

#include <QCoro/QCoroSignal>
#include <QCoro/QCoroNetworkReply>
#include <QCoro/QCoroTimer>

#include "common/config.h"
#include "checkforupdates.h"


CheckForUpdates::CheckForUpdates(const QString &baseUrl, QWidget *parent)
    : QObject(parent)
    , m_currentVersion(QVersionNumber::fromString(QCoreApplication::applicationVersion()))
    , m_parent(parent)
{
    m_checkUrl = baseUrl;
    m_checkUrl.replace(u"github.com"_qs, u"https://api.github.com/repos"_qs);
    m_checkUrl.append(u"/releases/latest"_qs);

    m_changelogUrl = baseUrl;
    m_changelogUrl.replace(u"github.com"_qs, u"https://raw.githubusercontent.com"_qs);
    m_changelogUrl.append(u"/main/CHANGELOG.md"_qs);

    m_downloadUrl = baseUrl;
    m_downloadUrl.prepend(u"https://"_qs);
    m_downloadUrl.append(u"/releases/tag/v%1"_qs);

    m_updatesPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/updates/";
    QDir d(m_updatesPath);
    d.mkpath(u"."_qs);
    const auto leftOvers = d.entryList(QDir::Files);
    for (const auto &leftOver: leftOvers)
        QFile::remove(d.absoluteFilePath(leftOver));

    languageChange();
}

QCoro::Task<> CheckForUpdates::check(bool silent)
{
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
        auto assets = doc[u"assets"].toArray();
        m_installerUrl.clear();
        for (const QJsonValueRef &asset : assets) {
            QString name = asset[u"name"].toString();
#if defined(Q_OS_MACOS)
            if (name.startsWith(u"macOS-", Qt::CaseInsensitive)) {
#elif defined(Q_OS_WINDOWS) && defined(_M_AMD64)
            if (name.startsWith(u"Windows-x64-", Qt::CaseInsensitive)
                    || name.startsWith(u"Windows-Intel64-", Qt::CaseInsensitive)) {
#elif defined(Q_OS_WINDOWS) && defined(_M_ARM64)
            if (name.startsWith(u"Windows-ARM64-", Qt::CaseInsensitive)) {
#else
            if (false) {
#endif
                m_installerName = name;
                m_installerUrl = asset[u"browser_download_url"].toString();
                break;
            }
        }
    }
    m_checking = false;

    const auto ignoreVersion = QVersionNumber::fromString(
                Config::inst()->value(u"General/IgnoreUpdateVersion"_qs).toString());

    if (!m_silent || ((latestVersion > m_currentVersion)
                      && (ignoreVersion != latestVersion))) {
        QTimer t;
        t.start(0);
        co_await t;

        if (latestVersion.isNull()) {
            QMessageBox::warning(m_parent, m_title,
                                 tr("Version information is not available."));
        } else if (latestVersion <= m_currentVersion) {
            QMessageBox::information(m_parent, m_title,
                                     tr("Your currently installed version is up-to-date."));
        } else {
            showVersionChanges(latestVersion);
        }
    }
}

bool CheckForUpdates::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QObject::event(e);
}

void CheckForUpdates::languageChange()
{
    m_title = tr("Update");
}

QCoro::Task<> CheckForUpdates::showVersionChanges(QVersionNumber latestVersion)
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
        QRegularExpressionMatch match =
                header.match(md, nextHeader);
        if (!match.hasMatch())
            break;

        // Qt cannot format links in headers
        QString s = u"\n##  \n## Version %1  \n"_qs.arg(match.captured(1));
        md.replace(match.capturedStart(), match.capturedLength(), s);

        if (match.captured(1) == m_currentVersion.toString())
            fromHeader = match.capturedStart();
        else if (match.captured(1) == latestVersion.toString())
            toHeader = match.capturedStart();

        nextHeader = match.capturedEnd();
    }
    if ((toHeader > 0) && (fromHeader > toHeader)) {
        const QString s1 = tr("A newer version than the one currently installed is available:");
        const QString s2 = tr("Changes:");
        const QString top = u"## %1 %2\n---\n## %3\n"_qs.arg(s1, latestVersion.toString(), s2);
        const QString url = m_downloadUrl.arg(latestVersion.toString());

        md = top + md.mid(toHeader, fromHeader - toHeader);

        QTimer t;
        t.start(0);
        co_await t;

        auto *dlg = new QDialog(m_parent);
        dlg->setWindowTitle(m_title);
        auto layout = new QVBoxLayout(dlg);
        auto browser = new QTextBrowser();
        browser->setReadOnly(true);
        browser->setMarkdown(md);
        browser->setOpenLinks(true);
        browser->setOpenExternalLinks(true);
        browser->zoomIn();
        layout->addWidget(browser);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Ignore);
        auto showDL = new QPushButton(tr("Show"));
        QObject::connect(showDL, &QPushButton::clicked, dlg, [dlg, url]() {
            QDesktopServices::openUrl(url);
            dlg->close();
        });
        bool canInstall = !m_installerUrl.isEmpty();
        showDL->setDefault(!canInstall);
        buttons->addButton(showDL, QDialogButtonBox::ActionRole);

        if (canInstall) {
            auto install = new QPushButton(tr("Install"));
            install->setDefault(true);
            buttons->addButton(install, QDialogButtonBox::ActionRole);
            QObject::connect(install, &QPushButton::clicked, dlg, [this, dlg]() {
                downloadInstaller();
                dlg->close();
            });
        }
        QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, [latestVersion, dlg]() {
            Config::inst()->setValue(u"General/IgnoreUpdateVersion"_qs, latestVersion.toString());
            dlg->close();
        });
        layout->addWidget(buttons);
        dlg->resize(dlg->fontMetrics().averageCharWidth() * 100, dlg->fontMetrics().height() * 30);

        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowModality(Qt::ApplicationModal);
        dlg->show();
    }
}

QCoro::Task<> CheckForUpdates::downloadInstaller()
{
    QNetworkRequest request(m_installerUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto pd = new QProgressDialog();
    pd->setWindowModality(Qt::ApplicationModal);
    pd->setAttribute(Qt::WA_DeleteOnClose, true);
    pd->setMinimumDuration(0);
    pd->setLabelText(tr("Downloading installer"));

    auto *reply = m_nam.get(request);

    connect(reply, &QNetworkReply::downloadProgress,
            this, [pd](qint64 recv, qint64 total) {
        pd->setMaximum(int(total));
        pd->setValue(int(recv));
    });
    connect(pd, &QProgressDialog::canceled,
            reply, &QNetworkReply::abort);
    pd->reset();

    co_await reply;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(nullptr, m_title, tr("Download failed: %1").arg(reply->errorString()));
    } else {
        QFile f(m_updatesPath + m_installerName);
        if (f.open(QIODevice::WriteOnly))
            f.write(reply->readAll());
        f.close();
        QUrl startUrl = QUrl::fromLocalFile(f.fileName());

        QMetaObject::invokeMethod(qApp, [=]() {
            QDesktopServices::openUrl(startUrl);
            QCoreApplication::quit();
        }, Qt::QueuedConnection);
    }
}

#include "moc_checkforupdates.cpp"
