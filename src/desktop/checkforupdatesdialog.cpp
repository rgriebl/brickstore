// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QFile>
#include <QDir>
#include <QEvent>
#include <QNetworkReply>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QProgressDialog>
#include <QMessageBox>
#include <QStandardPaths>

#include <QCoro/QCoroSignal>
#include <QCoro/QCoroNetworkReply>
#include <QCoro/QCoroTimer>

#include "common/config.h"
#include "checkforupdatesdialog.h"


CheckForUpdatesDialog::CheckForUpdatesDialog(QWidget *parent)
    : QObject(parent)
    , m_parent(parent)
{
    m_updatesPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/updates/";
    QDir d(m_updatesPath);
    d.mkpath(u"."_qs);
    const auto leftOvers = d.entryList(QDir::Files);
    for (const auto &leftOver: leftOvers)
        QFile::remove(d.absoluteFilePath(leftOver));

    languageChange();
}

void CheckForUpdatesDialog::showVersionChanges(QVersionNumber version, QString changeLog,
                                               QUrl releaseUrl,
                                               QString installerName, QUrl installerUrl)
{
    auto dlg = new QDialog(m_parent);
    dlg->setWindowTitle(m_title);

    auto layout = new QVBoxLayout(dlg);
    auto browser = new QTextBrowser();
    browser->setReadOnly(true);
    browser->setMarkdown(changeLog);
    browser->setOpenLinks(true);
    browser->setOpenExternalLinks(true);
    browser->zoomIn();
    layout->addWidget(browser);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Ignore);
    auto showDL = new QPushButton(tr("Show"));
    QObject::connect(showDL, &QPushButton::clicked, dlg, [dlg, releaseUrl]() {
        QDesktopServices::openUrl(releaseUrl);
        dlg->close();
    });
    bool canInstall = !installerUrl.isEmpty();
    showDL->setDefault(!canInstall);
    buttons->addButton(showDL, QDialogButtonBox::ActionRole);

    if (canInstall) {
        auto install = new QPushButton(tr("Install"));
        install->setDefault(true);
        buttons->addButton(install, QDialogButtonBox::ActionRole);
        QObject::connect(install, &QPushButton::clicked, dlg, [=, this]() {
            downloadInstaller(installerName, installerUrl);
            dlg->close();
        });
    }
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, [version, dlg]() {
        Config::inst()->setValue(u"General/IgnoreUpdateVersion"_qs, version.toString());
        dlg->close();
    });
    layout->addWidget(buttons);
    dlg->resize(dlg->fontMetrics().averageCharWidth() * 100, dlg->fontMetrics().height() * 30);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::ApplicationModal);
    dlg->show();
}

QCoro::Task<> CheckForUpdatesDialog::downloadInstaller(QString installerName, QUrl installerUrl)
{
    QNetworkAccessManager nam;
    QNetworkRequest request(installerUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto pd = new QProgressDialog();
    pd->setWindowModality(Qt::ApplicationModal);
    pd->setAttribute(Qt::WA_DeleteOnClose, true);
    pd->setMinimumDuration(0);
    pd->setLabelText(tr("Downloading installer"));

    auto *reply = nam.get(request);

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
        QFile f(m_updatesPath + installerName);
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

bool CheckForUpdatesDialog::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QObject::event(e);
}

void CheckForUpdatesDialog::languageChange()
{
    m_title = tr("Update");
}


#include "moc_checkforupdatesdialog.cpp"
