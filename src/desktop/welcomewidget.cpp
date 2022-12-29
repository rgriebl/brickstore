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

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/document.h"
#include "common/humanreadabletimedelta.h"
#include "common/recentfiles.h"
#include "bettercommandbutton.h"
#include "flowlayout.h"
#include "welcomewidget.h"


WelcomeWidget::WelcomeWidget(QWidget *parent)
    : QWidget(parent)
    , m_docIcon(u":/assets/generated-app-icons/brickstore_doc"_qs)
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
    layout->setRowStretch(3, 5);
    layout->setRowStretch(4, 0);
    layout->setRowStretch(5, 1);
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
            auto [filePath, fileName] = RecentFiles::inst()->filePathAndName(i);
            auto b = new BetterCommandButton(fileName, QDir::toNativeSeparators(filePath));
            b->setFocusPolicy(Qt::NoFocus);
            b->setIcon(m_docIcon);
            recent_layout->addWidget(b);
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

    auto *sizeGrip = new QSizeGrip(this);
    layout->addWidget(sizeGrip, 5, 3, 1, 1, Qt::AlignBottom | Qt::AlignRight);

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

    QString s = QCoreApplication::applicationName() + u" " %
            tr("version %1 (build: %2)").arg(ver).arg(Application::inst()->buildNumber()) %
            u"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&middot;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" %
            tr("Using a database that was generated %1").arg(dbd);
    m_versions->setText(s);
}

void WelcomeWidget::languageChange()
{
    m_recent_frame->setTitle(tr("Open recent files"));
    m_document_frame->setTitle(tr("Document"));
    m_import_frame->setTitle(tr("Import items"));

    updateVersionsText();

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
