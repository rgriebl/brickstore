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
#include <QStringBuilder>
#include <QStandardPaths>
#include <QMessageBox>

#include "qcoro/qcoro.h"
#include "utility/utility.h"
#include "checkforupdates.h"
#include "progressdialog.h"


CheckForUpdates::CheckForUpdates(const QString &baseUrl, QWidget *parent)
    : QObject(parent)
    , m_currentVersion(QVersionNumber::fromString(QCoreApplication::applicationVersion()))
    , m_parent(parent)
{
    m_checkUrl = baseUrl;
    m_checkUrl.replace("github.com"_l1, "https://api.github.com/repos"_l1);
    m_checkUrl.append("/releases/latest"_l1);

    m_changelogUrl = baseUrl;
    m_changelogUrl.replace("github.com"_l1, "https://raw.githubusercontent.com"_l1);
    m_changelogUrl.append("/main/CHANGELOG.md"_l1);

    m_downloadUrl = baseUrl;
    m_downloadUrl.prepend("https://"_l1);
    m_downloadUrl.append("/releases/tag/v%1"_l1);

    m_updatesPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) % u"/updates/";
    QDir d(m_updatesPath);
    d.mkpath('.'_l1);
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
        QString tag = doc["tag_name"_l1].toString();
        if (tag.startsWith('v'_l1))
            tag.remove(0, 1);
        latestVersion = QVersionNumber::fromString(tag);
        if (latestVersion.isNull())
            qWarning() << "Cannot parse GitHub's latest tag_name:" << tag;
        auto assets = doc["assets"_l1].toArray();
        m_installerUrl.clear();
        for (const QJsonValue asset : assets) {
            QString name = asset["name"_l1].toString();
#if defined(Q_OS_MACOS)
            if (name.startsWith("macOS-"_l1)) {
#elif defined(Q_OS_WIN64)
            if (name.startsWith("Windows-x64-"_l1)) {
#elif defined(Q_OS_WIN)
            if (name.startsWith("Windows-x86-"_l1)) {
#else
            if (false) {
#endif
                m_installerName = name;
                m_installerUrl = asset["browser_download_url"_l1].toString();
                break;
            }
        }
    }
    m_checking = false;

    if (!m_silent || (latestVersion > m_currentVersion)) {
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
    QRegularExpression header(R"(^## \[([0-9.]+)\] - \d{4}-\d{2}-\d{2}$)"_l1);
    header.setPatternOptions(QRegularExpression::MultilineOption);

    int fromHeader = 0;
    int toHeader = 0;
    int nextHeader = 0;

    while (!fromHeader || !toHeader) {
        QRegularExpressionMatch match = header.match(md, nextHeader);
        if (!match.hasMatch())
            break;

        // Qt cannot format links in headers
        QString s = QString::fromLatin1("\n##  \n## Version %1  \n").arg(match.captured(1));
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
        const QString top = QString::fromLatin1("## %1 %2\n---\n## %3\n")
                .arg(s1, latestVersion.toString(), s2);
        const QString url = m_downloadUrl.arg(latestVersion.toString());

        md = top % md.mid(toHeader, fromHeader - toHeader);

        QTimer t;
        t.start(0);
        co_await t;

        QDialog d(m_parent);
        d.setWindowTitle(m_title);
        auto layout = new QVBoxLayout(&d);
        auto browser = new QTextBrowser();
        browser->setReadOnly(true);
        browser->setMarkdown(md);
        browser->setOpenLinks(true);
        browser->setOpenExternalLinks(true);
        browser->zoomIn();
        layout->addWidget(browser);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        auto showDL = new QPushButton(tr("Show"));
        QObject::connect(showDL, &QPushButton::clicked, &d, [&d, url]() {
            QDesktopServices::openUrl(url);
            d.close();
        });
        bool canInstall = !m_installerUrl.isEmpty();
        showDL->setDefault(!canInstall);
        buttons->addButton(showDL, QDialogButtonBox::ActionRole);

        if (canInstall) {
            auto install = new QPushButton(tr("Install"));
            install->setDefault(true);
            buttons->addButton(install, QDialogButtonBox::ActionRole);
            QObject::connect(install, &QPushButton::clicked, &d, [this, &d]() {
                downloadInstaller();
                d.close();
            });
        }
        QObject::connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        layout->addWidget(buttons);
        d.resize(d.fontMetrics().averageCharWidth() * 100, d.fontMetrics().height() * 30);
        d.exec();
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
        QFile f(m_updatesPath % m_installerName);
        if (f.open(QIODevice::WriteOnly))
            f.write(reply->readAll());
        f.close();
        QDesktopServices::openUrl(QUrl::fromLocalFile(f.fileName()));
    }
}

#include "moc_checkforupdates.cpp"
