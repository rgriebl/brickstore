// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPushButton>
#include <QGroupBox>
#include <QTimer>
#include <QLayout>
#include <QEvent>
#include <QLabel>
#include <QFileInfo>
#include <QtMath>
#include <QStyle>
#include <QSizeGrip>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDir>
#include <QMenu>

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/humanreadabletimedelta.h"
#include "common/recentfiles.h"
#include "bettercommandbutton.h"
#include "flowlayout.h"
#include "welcomewidget.h"


WelcomeWidget::WelcomeWidget(QWidget *parent)
    : QWidget(parent)
    , m_docIcon(u":/assets/generated-app-icons/brickstore_doc"_qs)
    , m_pinIcon(QIcon::fromTheme(u"window-pin"_qs))
    , m_unpinIcon(QIcon::fromTheme(u"window-unpin"_qs))
{
    m_effect = new QGraphicsOpacityEffect(this);
    m_effect->setEnabled(false);
    setGraphicsEffect(m_effect);

    m_animation = new QPropertyAnimation(m_effect, "opacity");
    m_animation->setDuration(200);

    connect(m_animation, &QAbstractAnimation::stateChanged,
            this, [this](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
        if ((newState == QAbstractAnimation::Running) && (oldState == QAbstractAnimation::Stopped)) {
            m_effect->setEnabled(true);
            show();
        } else if (newState == QAbstractAnimation::Stopped) {
            if (m_effect->opacity() == 0)
                hide();
            m_effect->setEnabled(false);
        }
    });

    int spacing = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);

    auto *layout = new QGridLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setRowStretch(0, 1);
    layout->setRowStretch(1, 0);
    layout->setRowStretch(2, 0);
    layout->setRowStretch(3, 7);
    layout->setRowStretch(4, 0);
    layout->setRowStretch(5, 0);
    layout->setRowStretch(6, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 2);
    layout->setColumnStretch(2, 2);
    layout->setColumnStretch(3, 1);
    layout->setSpacing(2 * spacing);

    // recent

    m_recent_frame = new QGroupBox();
    auto recent_layout = new FlowLayout(FlowLayout::VerticalOnly);
    m_recent_frame->setLayout(recent_layout);
    layout->addWidget(m_recent_frame, 1, 2, 3, 1);

    auto recreateRecentGroup = [this, recent_layout]() {
        while (recent_layout->count() >= 1) {
            auto li = recent_layout->takeAt(0);
            delete li->widget();
            delete li;
        }

        if (RecentFiles::inst()->count() == 0) {
            if (!m_no_recent)
                m_no_recent = new QLabel();
            recent_layout->addWidget(m_no_recent);
        }

        for (int i = 0; i < RecentFiles::inst()->count(); ++i) {
            QModelIndex index = RecentFiles::inst()->index(i);
            const auto filePath = index.data(RecentFiles::FilePathRole).toString();
            const auto fileName = index.data(RecentFiles::FileNameRole).toString();
            const bool pinned = index.data(RecentFiles::PinnedRole).toBool();

            auto b = new BetterCommandButton(fileName, QDir::toNativeSeparators(filePath));
            b->setFocusPolicy(Qt::NoFocus);
            b->setIcon(pinned ? m_pinIcon : m_docIcon);
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            recent_layout->addWidget(b);

            connect(b, &BetterCommandButton::customContextMenuRequested,
                    this, [=](const QPoint &pos) {
                auto index = RecentFiles::inst()->index(i);
                const bool pinned = index.data(RecentFiles::PinnedRole).toBool();
                auto *m = new QMenu(this);
                m->setAttribute(Qt::WA_DeleteOnClose);
                connect(m->addAction(pinned ? m_unpinIcon : m_pinIcon,
                                     pinned ? tr("Unpin") : tr("Pin")),
                        &QAction::triggered, this, [=]() {
                    RecentFiles::inst()->setData(RecentFiles::inst()->index(i), !pinned,
                                                 RecentFiles::PinnedRole);
                });
                m->popup(b->mapToGlobal(pos));
            });
            connect(b, &BetterCommandButton::clicked,
                    this, [=]() { RecentFiles::inst()->open(i); });
        }
    };
    recreateRecentGroup();
    connect(RecentFiles::inst(), &RecentFiles::recentFilesChanged,
            this, recreateRecentGroup);

    // document

    m_document_frame = new QGroupBox();
    auto document_layout = new QVBoxLayout();
    for (const auto &name : { "document_new", "document_open" }) {
        auto b = new BetterCommandButton(ActionManager::inst()->qAction(name));
        b->setFocusPolicy(Qt::NoFocus);
        document_layout->addWidget(b);
    }
    m_document_frame->setLayout(document_layout);
    layout->addWidget(m_document_frame, 1, 1);

    // import

    m_import_frame = new QGroupBox();
    auto import_layout = new FlowLayout(FlowLayout::VerticalOnly);
    m_import_frame->setLayout(import_layout);
    for (const auto &name : {
         "document_import_bl_inv",
         "document_import_bl_store_inv",
         "document_import_bl_order",
         "document_import_bl_cart",
         "document_import_bl_wanted",
         "document_import_ldraw_model",
         "document_import_bl_xml" }) {
        auto b = new BetterCommandButton(ActionManager::inst()->qAction(name));
        b->setFocusPolicy(Qt::NoFocus);
        import_layout->addWidget(b);
    }
    layout->addWidget(m_import_frame, 2, 1);

    m_versions = new QLabel();
    m_versions->setAlignment(Qt::AlignCenter);
    connect(BrickLink::core()->database(), &BrickLink::Database::lastUpdatedChanged,
            this, &WelcomeWidget::updateVersionsText);
    layout->addWidget(m_versions, 4, 1, 1, 2);

    static const QVector<std::tuple<const char *, const char *, QString>> links {
        { "bootstrap-question-circle", QT_TR_NOOP("Tutorials"),   u"https://github.com/rgriebl/brickstore/wiki/Tutorials"_qs },
        { "bootstrap-chat-dots",       QT_TR_NOOP("Discussions"), u"https://github.com/rgriebl/brickstore/discussions"_qs },
        { "bootstrap-bug",             QT_TR_NOOP("Bug reports"), u"https://github.com/rgriebl/brickstore/issues"_qs },
        { "bootstrap-heart",           QT_TR_NOOP("Sponsor"),     u"https://brickforge.de/brickstore/support"_qs },
    };

    int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    auto linksLayout = new QHBoxLayout();
    linksLayout->setSpacing(iconSize / 3);

    m_links.reserve(links.size());
    for (const auto &[ icon, title, link ] : std::as_const(links)) {
        if (linksLayout->count()) {
            linksLayout->addWidget(new QLabel(u"     \u00b7     "_qs));
        }
        auto iconLabel = new QLabel();
        iconLabel->setPixmap(QIcon::fromTheme(QString::fromLatin1(icon)).pixmap(iconSize));
        linksLayout->addWidget(iconLabel);

        auto linkLabel = new QLabel();
        linkLabel->setProperty("formatText", QString { uR"(<b><a href=")" + link + uR"(">%1</a></b>)"_qs });
        linkLabel->setProperty("untranslatedText", QByteArray(title));
        linkLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
        linkLabel->setOpenExternalLinks(true);
        linksLayout->addWidget(linkLabel);

        m_links << linkLabel;
    }
    layout->addLayout(linksLayout, 5, 1, 1, 2, Qt::AlignCenter);

    auto *sizeGrip = new QSizeGrip(this);
    layout->addWidget(sizeGrip, 6, 0, 1, 4, Qt::AlignBottom | Qt::AlignRight);

    languageChange();
    setLayout(layout);
}

void WelcomeWidget::fadeIn()
{
    fade(true);
}

void WelcomeWidget::fadeOut()
{
    fade(false);
}

void WelcomeWidget::fade(bool in)
{
    auto direction = in ? QAbstractAnimation::Forward : QAbstractAnimation::Backward;

    if (!m_effect->isEnabled() && (in == isVisibleTo(parentWidget()))) {
        return;
    } else if (m_animation->state() == QAbstractAnimation::Running) {
        if (m_animation->direction() == direction) {
            return;
        } else {
            m_animation->pause();
            m_animation->setDirection(direction);
            m_animation->resume();
        }
    } else {
        m_animation->setDirection(direction);
        m_animation->setStartValue(0);
        m_animation->setEndValue(1);
        m_animation->start();
    }
}

void WelcomeWidget::updateVersionsText()
{
    auto delta = HumanReadableTimeDelta::toString(QDateTime::currentDateTime(),
                                                  BrickLink::core()->database()->lastUpdated());

    QString dbd = u"<b>" + delta + u"</b>";
    QString ver = u"<b>" + QCoreApplication::applicationVersion() + u"</b>";

    QString s = QCoreApplication::applicationName() + u" "
                + tr("version %1 (build: %2)").arg(ver).arg(Application::inst()->buildNumber())
                + u"\u00a0\u00a0\u00a0\u00a0\u00a0\u00b7\u00a0\u00a0\u00a0\u00a0\u00a0"
                + tr("Using a database that was generated %1").arg(dbd);
    m_versions->setText(s);
}

void WelcomeWidget::languageChange()
{
    m_recent_frame->setTitle(tr("Open recent files"));
    m_document_frame->setTitle(tr("Document"));
    m_import_frame->setTitle(tr("Import items"));

    updateVersionsText();
    for (auto &l : std::as_const(m_links)) {
        auto ut = l->property("untranslatedText").toByteArray();
        l->setText(l->property("formatText").toString().arg(tr(ut)));
    }

    if (m_no_recent)
        m_no_recent->setText(tr("No recent files"));
}

void WelcomeWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QWidget::changeEvent(e);
}


#include "moc_welcomewidget.cpp"
