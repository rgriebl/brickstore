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
#include <QStringBuilder>
#include <QMenu>
#include <QTextLayout>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QStyleFactory>

#include "bettercommandbutton.h"


BetterCommandButton::BetterCommandButton(QAction *action, QWidget *parent)
    : BetterCommandButton(parent)
{
    setAction(action);
}

BetterCommandButton::BetterCommandButton(QWidget *parent)
    : BetterCommandButton(QString(), QString(), parent)
{ }

BetterCommandButton::BetterCommandButton(const QString &text, QWidget *parent)
    : BetterCommandButton(text, QString(), parent)
{ }

BetterCommandButton::BetterCommandButton(const QString &text, const QString &description, QWidget *parent)
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

QRect BetterCommandButton::titleRect() const
{
    QRect r = rect().adjusted(textOffset(), m_margin, -m_margin, 0);
    if (m_description.text().isEmpty()) {
        QFontMetrics fm(m_titleFont);
        r.setTop(r.top() + qMax(0, (icon().actualSize(iconSize()).height() - fm.height()) / 2));
    }
    return r;
}

QRect BetterCommandButton::descriptionRect() const
{
    return rect().adjusted(textOffset(), descriptionOffset(), -m_margin, -m_margin);
}

int BetterCommandButton::textOffset() const
{
    return m_margin + icon().actualSize(iconSize()).width() + m_margin;
}

int BetterCommandButton::descriptionOffset() const
{
    QFontMetrics fm(m_titleFont);
    return m_margin + fm.height();
}

int BetterCommandButton::descriptionHeight(int widgetWidth) const
{
    int lineWidth = widgetWidth - textOffset() - m_margin;
    QStaticText copy(m_description);
    copy.setTextWidth(lineWidth);
    return int(copy.size().height());
}

QSize BetterCommandButton::sizeHint() const
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

QSize BetterCommandButton::minimumSizeHint() const
{
    QSize s = sizeHint();
    int l = QFontMetrics(m_titleFont).averageCharWidth() * 40;
    if (s.width() > l) {
        s.setWidth(l);
        s.setHeight(heightForWidth(l));
    }
    return s;
}

bool BetterCommandButton::hasHeightForWidth() const
{
    return true;
}

int BetterCommandButton::heightForWidth(int width) const
{
    int heightWithoutDescription = descriptionOffset() + m_margin;
    return qMax(heightWithoutDescription + descriptionHeight(width),
                m_margin + icon().actualSize(iconSize()).height() + m_margin);
}

void BetterCommandButton::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange) {
        resetTitleFont();
        updateGeometry();
        update();
    } else if (e->type() == QEvent::LanguageChange) {
        updateAction();
    }
    QWidget::changeEvent(e);
}

void BetterCommandButton::resizeEvent(QResizeEvent *re)
{
    m_description.setTextWidth(titleRect().width());
    QWidget::resizeEvent(re);
}

void BetterCommandButton::paintEvent(QPaintEvent *)
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

    if (option.state & QStyle::State_HasFocus) {
        QStyleOptionFocusRect focusOpt;
        focusOpt.initFrom(this);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, &p, this);
    }

    p.restore();
}

void BetterCommandButton::resetTitleFont()
{
    m_titleFont = font();
    m_titleFont.setPointSizeF(m_titleFont.pointSizeF() * 1.25);
}

void BetterCommandButton::updateAction()
{
    if (!m_action)
        return;

    setText(m_action->text());
    updateDescriptionText();
    setToolTip(m_action->toolTip());

    if (!m_action->icon().isNull()) {
        setIcon(m_action->icon());
    } else {
        const auto objects = m_action->associatedObjects();
        for (auto *object : objects) {
            if (QMenu *menu = qobject_cast<QMenu *>(object)) {
                if (!menu->icon().isNull())
                    setIcon(menu->icon());
            }
        }
    }
    updateGeometry();
    update();
}

void BetterCommandButton::updateDescriptionText()
{
    auto sc = m_action ? m_action->shortcut() : shortcut();
    QString desc = m_descriptionText;

    if (!sc.isEmpty())
        desc = desc % u" <i>(%1)</i>"_qs.arg(sc.toString(QKeySequence::NativeText));

    if (m_description.text() != desc) {
        m_description.setText(desc);
        updateGeometry();
        update();
    }
}

QString BetterCommandButton::description() const
{
    return m_description.text();
}

void BetterCommandButton::setDescription(const QString &desc)
{
    if (desc != m_descriptionText) {
        m_descriptionText = desc;
        updateDescriptionText();
    }
}

QAction *BetterCommandButton::action() const
{
    return m_action;
}

void BetterCommandButton::setAction(QAction *action)
{
    if (m_action) {
        disconnect(m_action, nullptr, this, nullptr);
        disconnect(this, nullptr, m_action, nullptr);
    }
    m_action = action;
    if (m_action) {
        connect(m_action, &QAction::changed, this, &BetterCommandButton::updateAction);
        connect(this, &BetterCommandButton::clicked, m_action, &QAction::trigger);
    }
    updateAction();
}

#include "moc_bettercommandbutton.cpp"
