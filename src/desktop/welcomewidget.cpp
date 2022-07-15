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
#include <QTextLayout>
#include <QtMath>
#include <QStaticText>
#include <QMenu>
#include <QStyle>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QStyleFactory>
#include <QStringBuilder>
#include <QSizeGrip>

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/humanreadabletimedelta.h"
#include "flowlayout.h"
#include "welcomewidget.h"

// Based on QCommandLinkButton, but this one scales with font size, supports richtext and can be
// associated with a QAction

class WelcomeButton : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY(WelcomeButton)
public:
    explicit WelcomeButton(QAction *action, QWidget *parent = nullptr);

    explicit WelcomeButton(QWidget *parent = nullptr)
        : WelcomeButton(QString(), QString(), parent)
    { }
    explicit WelcomeButton(const QString &text, QWidget *parent = nullptr)
        : WelcomeButton(text, QString(), parent)
    { }
    explicit WelcomeButton(const QString &text, const QString &description, QWidget *parent = nullptr);

    QString description() const
    {
        return m_description.text();
    }

    void setDescription(const QString &desc)
    {
        if (desc != m_description.text()) {
            m_description.setText(desc);
            updateGeometry();
            update();
        }
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;

protected:
    void changeEvent(QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *) override;

private:
    void resetTitleFont()
    {
        m_titleFont = font();
        m_titleFont.setPointSizeF(m_titleFont.pointSizeF() * 1.25);
    }

    int textOffset() const;
    int descriptionOffset() const;
    QRect descriptionRect() const;
    QRect titleRect() const;
    int descriptionHeight(int widgetWidth) const;

    QFont m_titleFont;
    QStaticText m_description;
    int m_margin = 10;
};

WelcomeButton::WelcomeButton(QAction *action, QWidget *parent)
    : WelcomeButton(parent)
{
    if (!action)
        return;

    auto languageChange = [this](QAction *a) {
        setText(a->text());
        if (!a->shortcut().isEmpty()) {
            QString desc = u"<i>(" % tr("Shortcut:") % u" %1)</i>";
            setDescription(desc.arg(a->shortcut().toString(QKeySequence::NativeText)));
        }
        setToolTip(a->toolTip());

        if (!a->icon().isNull()) {
            setIcon(a->icon());
        } else {
            const auto containers = a->associatedWidgets();
            for (auto *widget : containers) {
                if (QMenu *menu = qobject_cast<QMenu *>(widget)) {
                    if (!menu->icon().isNull())
                        setIcon(menu->icon());
                }
            }
        }
    };

    connect(this, &WelcomeButton::clicked, action, &QAction::trigger);
    connect(action, &QAction::changed, this, [languageChange, action]() { languageChange(action); });
    languageChange(action);
}

WelcomeButton::WelcomeButton(const QString &text, const QString &description, QWidget *parent)
    : QPushButton(text, parent)
    , m_description(description)
{
    setAttribute(Qt::WA_Hover);

    // only Fusion seems to be able to draw QCommandLink buttons correctly
    if (auto s = QStyleFactory::create(u"fusion"_qs)) {
        s->setParent(this);
        setStyle(s);
    }

    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred, QSizePolicy::PushButton);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    setIconSize({ 32, 32 });
    setIcon(QIcon::fromTheme(u"go-next"_qs));

    resetTitleFont();
}

QRect WelcomeButton::titleRect() const
{
    QRect r = rect().adjusted(textOffset(), m_margin, -m_margin, 0);
    if (m_description.text().isEmpty()) {
        QFontMetrics fm(m_titleFont);
        r.setTop(r.top() + qMax(0, (icon().actualSize(iconSize()).height() - fm.height()) / 2));
    }
    return r;
}

QRect WelcomeButton::descriptionRect() const
{
    return rect().adjusted(textOffset(), descriptionOffset(), -m_margin, -m_margin);
}

int WelcomeButton::textOffset() const
{
    return m_margin + icon().actualSize(iconSize()).width() + m_margin;
}

int WelcomeButton::descriptionOffset() const
{
    QFontMetrics fm(m_titleFont);
    return m_margin + fm.height();
}

int WelcomeButton::descriptionHeight(int widgetWidth) const
{
    int lineWidth = widgetWidth - textOffset() - m_margin;
    QStaticText copy(m_description);
    copy.setTextWidth(lineWidth);
    return int(copy.size().height());
}

QSize WelcomeButton::sizeHint() const
{
    QSize size = QPushButton::sizeHint();
    QFontMetrics fm(m_titleFont);
    int textWidth = qMax(fm.horizontalAdvance(text()), 135);
    int buttonWidth = m_margin + icon().actualSize(iconSize()).width() + m_margin + textWidth + m_margin;
    int heightWithoutDescription = descriptionOffset() + m_margin;

    size.setWidth(qMax(size.width(), buttonWidth));
    size.setHeight(qMax(m_description.text().isEmpty() ? 41 : 60,
                        heightWithoutDescription + descriptionHeight(buttonWidth)));
    return size;
}

QSize WelcomeButton::minimumSizeHint() const
{
    QSize s = sizeHint();
    int l = QFontMetrics(m_titleFont).averageCharWidth() * 40;
    if (s.width() > l) {
        s.setWidth(l);
        s.setHeight(heightForWidth(l));
    }
    return s;
}

bool WelcomeButton::hasHeightForWidth() const
{
    return true;
}

int WelcomeButton::heightForWidth(int width) const
{
    int heightWithoutDescription = descriptionOffset() + m_margin;
    return qMax(heightWithoutDescription + descriptionHeight(width),
                m_margin + icon().actualSize(iconSize()).height() + m_margin);
}

void WelcomeButton::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange) {
        resetTitleFont();
        updateGeometry();
        update();
    }
    QWidget::changeEvent(e);
}

void WelcomeButton::resizeEvent(QResizeEvent *re)
{
    m_description.setTextWidth(titleRect().width());
    QWidget::resizeEvent(re);
}

void WelcomeButton::paintEvent(QPaintEvent *)
{
    QStylePainter p(this);
    p.save();

    QStyleOptionButton option;
    initStyleOption(&option);

    //Enable command link appearance on Vista
    option.features |= QStyleOptionButton::CommandLinkButton;
    option.features &= ~QStyleOptionButton::Flat;
    option.text = QString();
    option.icon = QIcon(); //we draw this ourselves
    QSize pixmapSize = icon().actualSize(iconSize());

    const int vOffset = isDown()
        ? style()->pixelMetric(QStyle::PM_ButtonShiftVertical, &option) : 0;
    const int hOffset = isDown()
        ? style()->pixelMetric(QStyle::PM_ButtonShiftHorizontal, &option) : 0;

    //Draw icon
    p.drawControl(QStyle::CE_PushButton, option);
    if (!icon().isNull()) {
        QFontMetrics fm(m_titleFont);
        p.drawPixmap(m_margin + hOffset, m_margin + qMax(0, fm.height() - pixmapSize.height()) / 2 + vOffset,
                     icon().pixmap(pixmapSize,
                                   isEnabled() ? QIcon::Normal : QIcon::Disabled,
                                   isChecked() ? QIcon::On : QIcon::Off));
    }

    //Draw title
    int textflags = Qt::TextShowMnemonic;
    if (!style()->styleHint(QStyle::SH_UnderlineShortcut, &option, this))
        textflags |= Qt::TextHideMnemonic;

    p.setFont(m_titleFont);
    QString str = p.fontMetrics().elidedText(text(), Qt::ElideRight, titleRect().width());
    p.drawItemText(titleRect().translated(hOffset, vOffset),
                    textflags, option.palette, isEnabled(), str, QPalette::ButtonText);

    //Draw description
    p.setFont(font());
    p.drawStaticText(descriptionRect().translated(hOffset, vOffset).topLeft(), m_description);
    p.restore();
}


WelcomeWidget::WelcomeWidget(QWidget *parent)
    : QWidget(parent)
    , m_docIcon(u":/assets/generated-app-icons/brickstore_doc"_qs)
{
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

        auto recent = Config::inst()->recentFiles();
        if (recent.isEmpty()) {
            if (!m_no_recent)
                m_no_recent = new QLabel();
            recent_layout->addWidget(m_no_recent);
        }

        for (const auto &f : recent) {
            auto b = new WelcomeButton(QFileInfo(f).fileName(), QFileInfo(f).absolutePath());
            b->setIcon(m_docIcon);
            recent_layout->addWidget(b);
            connect(b, &WelcomeButton::clicked,
                    this, [f]() { Document::load(f); });
        }
    };
    recreateRecentGroup();
    connect(Config::inst(), &Config::recentFilesChanged,
            this, recreateRecentGroup);

    // document

    m_document_frame = new QGroupBox();
    auto document_layout = new QVBoxLayout();
    for (const auto &name : { "document_new", "document_open" }) {
        auto b = new WelcomeButton(ActionManager::inst()->qAction(name));
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
        auto b = new WelcomeButton(ActionManager::inst()->qAction(name));
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

void WelcomeWidget::updateVersionsText()
{
    auto delta = HumanReadableTimeDelta::toString(QDateTime::currentDateTime(),
                                                  BrickLink::core()->database()->lastUpdated());

    QString dbd = u"<b>" % delta % u"</b>";
    QString ver = u"<b>" % QCoreApplication::applicationVersion() % u"</b>";

    QString s = QCoreApplication::applicationName() % u" " %
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


#include "welcomewidget.moc"
#include "moc_welcomewidget.cpp"
