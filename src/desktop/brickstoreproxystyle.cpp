// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QComboBox>
#include <QToolButton>
#include <QToolBar>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QPainter>
#include <QApplication>
#include <QPalette>
#include <QPixmapCache>
#include <QStyleFactory>
#include <QPointer>
#include <QDynamicPropertyChangeEvent>
#include <QKeyEvent>
#include <QVariantAnimation>
#include <QMenuBar>
#include <private/qmenubar_p.h>
#include <private/qstylehelper_p.h>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#endif

#include "utility/utility.h"
#include "common/config.h"
#include "desktop/headerview.h"
#include "brickstoreproxystyle.h"

static QSize toolButtonSize(Config::UISize uiSize, QStyle *style)
{
    static const QMap<Config::UISize, QStyle::PixelMetric> map = {
        { Config::UISize::System, QStyle::PM_ToolBarIconSize },
        { Config::UISize::Small, QStyle::PM_SmallIconSize },
        { Config::UISize::Large, QStyle::PM_LargeIconSize },
    };
    auto pm = map.value(uiSize, QStyle::PM_ToolBarIconSize);
    int s = style->pixelMetric(pm, nullptr, nullptr);
    return { s, s };
}

static QSize iconSize(int p, QStyle::PixelMetric pm, QStyle *style)
{
    int s = style->pixelMetric(pm, nullptr, nullptr) * p / 100;
    return { s, s };
}

class UnderlineShortcutFilter : public QObject
{
public:
    UnderlineShortcutFilter(BrickStoreProxyStyle *style)
        : QObject(style)
    {
        qApp->installEventFilter(this);
    }
    ~UnderlineShortcutFilter()
    {
        qApp->removeEventFilter(this);
    }

    bool status(const QStyleOption *option, const QWidget *widget) const
    {
        bool us = false;
#if defined(Q_OS_WINDOWS)
        BOOL cues = false;
        SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &cues, 0);
        us = bool(cues);
#endif
        if (!us && widget) {
            const QMenuBar *menuBar = qobject_cast<const QMenuBar *>(widget);
            if (!menuBar && qobject_cast<const QMenu *>(widget)) {
                QWidget *w = QApplication::activeWindow();
                if (w && w != widget)
                    menuBar = w->findChild<QMenuBar *>();
            }
            // If we paint a menu bar draw underlines if it is in the keyboardState
            if (menuBar) {
                if (static_cast<const QMenuBarPrivate *>(QMenuBarPrivate::get(menuBar))->keyboardState || m_altDown)
                    us = true;
                // Otherwise draw underlines if the toplevel widget has seen an alt-press
            } else if (m_seenAlt.contains(widget->window())) {
                us = true;
            }
        }
#if QT_CONFIG(accessibility)
        if (!us && option && option->type == QStyleOption::SO_MenuItem
            && QStyleHelper::isInstanceOf(option->styleObject, QAccessible::MenuItem)
            && option->styleObject->property("_q_showUnderlined").toBool()) {
            us = true;
        }
#else
        Q_UNUSED(option);
#endif
        return us;
    }

    bool eventFilter(QObject *o, QEvent *e) override;

    bool m_altDown = false;
    QList<const QWidget *> m_seenAlt;
};

bool UnderlineShortcutFilter::eventFilter(QObject *o, QEvent *e)
{
    if (!o->isWidgetType())
        return QObject::eventFilter(o, e);

    // Alt key handling - copied from qwindowsstyle.cpp
    if (((e->type() == QEvent::KeyPress) || (e->type() == QEvent::KeyRelease))
            && (static_cast<QKeyEvent *>(e)->key() == Qt::Key_Alt)) {
        QWidget *widget = qobject_cast<QWidget *>(o)->window();

        if (e->type() == QEvent::KeyPress) {
            // Alt has been pressed - find all widgets that care
            const QList<QWidget *> children = widget->findChildren<QWidget *>();
            m_seenAlt.append(widget);
            m_altDown = true;

            // Repaint all relevant widgets
            for (QWidget *w : children) {
                if (!w->isWindow() && w->isVisible())
                    w->update();
            }
        } else {
            // Update state and repaint the menu bars.
            m_altDown = false;
            const QList<QMenuBar *> menuBars = widget->findChildren<QMenuBar *>();
            for (QWidget *w : menuBars)
                w->update();
        }
    } else if (e->type() == QEvent::Close) {
        // Reset widget when closing
        QWidget *widget = qobject_cast<QWidget *>(o);
        m_seenAlt.removeAll(widget);
        m_seenAlt.removeAll(widget->window());
    }
    return QObject::eventFilter(o, e);
}


BrickStoreProxyStyle::BrickStoreProxyStyle(QStyle *baseStyle)
    : QProxyStyle(baseStyle)
{
#if defined(Q_OS_MACOS)
    // macOS style guide doesn't want shortcut keys in dialogs (Alt + underlined character)
    void qt_set_sequence_auto_mnemonic(bool on);
    qt_set_sequence_auto_mnemonic(true);

    extern void removeUnneededMacMenuItems();
    removeUnneededMacMenuItems();
#endif
    m_isWindowsVistaStyle = ((baseStyle ? baseStyle : qApp->style())->name() == u"windowsvista");

    connect(Config::inst(), &Config::toolBarSizeChanged,
            this, [this](Config::UISize uiSize) {
        auto s = toolButtonSize(uiSize, this);

        const auto allWidgets = qApp->allWidgets();
        for (auto w : allWidgets) {
            if (auto *tbar = qobject_cast<QToolBar *>(w))
                tbar->setIconSize(s);
        }
    });
    connect(Config::inst(), &Config::iconSizePercentChanged,
            this, [this](int p) {
        const auto allWidgets = qApp->allWidgets();
        for (auto w : allWidgets) {
            if (!w->property("iconScaling").toBool())
                continue;

            if (auto *iv = qobject_cast<QAbstractItemView *>(w)) {
                iv->setIconSize(iconSize(p, PM_ListViewIconSize, this));
            } else if (auto *cb = qobject_cast<QComboBox *>(w)) {
                cb->setIconSize(iconSize(p, PM_ListViewIconSize, this));
            } else if (auto *tb = qobject_cast<QToolButton *>(w)) {
                if (!qobject_cast<QToolBar *>(tb->parentWidget()))
                    tb->setIconSize(iconSize(p, PM_ButtonIconSize, this));
            }
        }
    });
}

void BrickStoreProxyStyle::polish(QWidget *w)
{
    int iconScaling = w->property("iconScaling").toBool() ? Config::inst()->iconSizePercent() : 0;

    if (qobject_cast<QLineEdit *>(w)
            || qobject_cast<QDoubleSpinBox *>(w)
            || qobject_cast<QSpinBox *>(w)) {
        w->installEventFilter(this);

    } else if (auto *iv = qobject_cast<QAbstractItemView *>(w)) {
        w->setMouseTracking(true);
        w->installEventFilter(this);
        if (iconScaling)
            iv->setIconSize(iconSize(iconScaling, PM_ListViewIconSize, this));

    } else if (qobject_cast<QMenu *>(w)) {
        // SH_Menu_Scrollable is checked early in the QMenu constructor,
        // so we need to force an re-evaluation
        if (w->property("scrollableMenu").toBool())
            qApp->postEvent(w, new QEvent(QEvent::StyleChange));

    } else if (auto *tb = qobject_cast<QToolButton *>(w)) {
        if (!qobject_cast<QToolBar *>(tb->parentWidget())) {
            if (tb->property("toolBarScaling").toBool()) {
                tb->setAutoRaise(true);
                tb->setIconSize(toolButtonSize(Config::inst()->toolBarSize(), this));
            }
            if (iconScaling) {
                tb->setAutoRaise(true);
                tb->setIconSize(iconSize(iconScaling, PM_ButtonIconSize, this));
            }

            QPointer<QToolButton> tbptr(tb);
            QMetaObject::invokeMethod(this, [tbptr]() {
                if (tbptr && tbptr->autoRaise()) {
                    QStyle *style = tbptr->style();
                    if (auto *proxyStyle = qobject_cast<QProxyStyle *>(style))
                        style = proxyStyle->baseStyle();
                    QString styleName = style ? style->name() : QString {};

#if defined(Q_OS_MACOS)
                    if (styleName == u"macos") {
                        // QToolButtons look really ugly on macOS, so we re-style them
                        static QStyle *fusion = QStyleFactory::create(u"fusion"_qs);
                        static QStyle *proxyFusion = new BrickStoreProxyStyle(fusion);
                        tbptr->setStyle(proxyFusion);
                        styleName = fusion->name();
                    }
#endif
                    // checked Fusion buttons are barely distinguishable from non-checked ones
                    if (styleName == u"fusion") {
                        QPalette pal = tbptr->palette();
                        pal.setColor(QPalette::Button, Utility::premultiplyAlpha(
                                         qApp->palette("QAbstractItemView")
                                         .color(QPalette::Highlight)));
                        tbptr->setPalette(pal);
                    }
                }
            });
        }
    } else if (auto *tbar = qobject_cast<QToolBar *>(w)) {
        tbar->setIconSize(toolButtonSize(Config::inst()->toolBarSize(), this));

    } else if (auto *cb = qobject_cast<QComboBox *>(w)) {
        if (iconScaling)
            cb->setIconSize(iconSize(iconScaling, PM_ListViewIconSize, this));
    }
    QProxyStyle::polish(w);
}

void BrickStoreProxyStyle::unpolish(QWidget *w)
{
    if (qobject_cast<QLineEdit *>(w)
            || qobject_cast<QDoubleSpinBox *>(w)
            || qobject_cast<QSpinBox *>(w)
            || qobject_cast<QAbstractItemView *>(w)) {
        w->removeEventFilter(this);
    }
    QProxyStyle::unpolish(w);
}

void BrickStoreProxyStyle::polish(QApplication *app)
{
    if (!styleHint(SH_UnderlineShortcut, nullptr) && app && !m_underlineShortcutFilter)
        m_underlineShortcutFilter = new UnderlineShortcutFilter(this);
    QProxyStyle::polish(app);
}

void BrickStoreProxyStyle::unpolish(QApplication *)
{
    delete m_underlineShortcutFilter;
    m_underlineShortcutFilter = nullptr;
}

int BrickStoreProxyStyle::styleHint(QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == SH_UnderlineShortcut) {
        if (m_underlineShortcutFilter)
            return m_underlineShortcutFilter->status(option, widget);
        else
            return false;
    } else if (hint == SH_ItemView_MovementWithoutUpdatingSelection) {
        // move the current item within a selection in a QTableView
        return true;
    } else if (hint == SH_ItemView_ScrollMode) {
        // smooth scrolling on all platforms - not just macOS
        return QAbstractItemView::ScrollPerPixel;
    } else if (hint == SH_Menu_Scrollable) {
        if (widget && widget->property("scrollableMenu").toBool())
            return true;
    } else if (hint == SH_ItemView_ActivateItemOnSingleClick) {
        if (widget && widget->property("singleClickActivation").toBool())
            return true;
        else
            return false;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void BrickStoreProxyStyle::drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &pal, bool enabled, const QString &text, QPalette::ColorRole textRole) const
{
#if defined(Q_OS_MACOS)
    // show _ shortcuts even on macOS
    QCommonStyle::drawItemText(painter, rect, flags, pal, enabled, text, textRole);
#else
    QProxyStyle::drawItemText(painter, rect, flags, pal, enabled, text, textRole);
#endif
}

void BrickStoreProxyStyle::drawComplexControl(ComplexControl control,
                                                    const QStyleOptionComplex *option,
                                                    QPainter *painter, const QWidget *widget) const
{
    if (control == CC_ComboBox) {
        if (qobject_cast<const QComboBox *>(widget)
                && widget->property("transparentCombo").toBool()) {
            if (const auto *comboOpt = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
                if (!comboOpt->editable) {
                    QStyleOptionComboBox opt(*comboOpt);
#if defined(Q_OS_MACOS)
                    opt.rect.setLeft(opt.rect.right() - 32);
#else
                    if (!opt.frame)
                        opt.subControls &= ~SC_ComboBoxFrame;
#endif
                    QProxyStyle::drawComplexControl(control, &opt, painter, widget);
                    return;
                }
            }
        }
    } else if (control == CC_ToolButton) {
        if (qobject_cast<const QToolButton *>(widget)
                && widget->property("noMenuArrow").toBool()) {
            if (const auto *tbOpt = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
                if (tbOpt->features & QStyleOptionToolButton::HasMenu) {
                    QStyleOptionToolButton opt(*tbOpt);
                    opt.features &= ~QStyleOptionToolButton::HasMenu;
                    QProxyStyle::drawComplexControl(control, &opt, painter, widget);
                    return;
                }
            }
        }
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}


void BrickStoreProxyStyle::drawPrimitive(PrimitiveElement elem, const QStyleOption *option,
                                         QPainter *painter, const QWidget *widget) const
{
#if !defined(Q_OS_MACOS)
    if (elem == PE_PanelButtonCommand) {
        if (qobject_cast<const QComboBox *>(widget)
                && widget->property("transparentCombo").toBool()) {
            if (option->state & State_HasFocus && option->state & State_KeyboardFocusChange) {
                painter->save();

                QRect r = option->rect.adjusted(0, 1, -1, 0);
                painter->setRenderHint(QPainter::Antialiasing, true);
                painter->translate(0.5, -0.5);

                painter->setPen(option->palette.highlight().color());
                painter->setBrush(Qt::NoBrush);
                painter->drawRoundedRect(r, 2.0, 2.0);

                painter->restore();
            }
            return;
        }
    }
#endif
    QProxyStyle::drawPrimitive(elem, option, painter, widget);
}

void BrickStoreProxyStyle::drawControl(ControlElement element, const QStyleOption *opt, QPainter *p, const QWidget *widget) const
{
    if (element == CE_HeaderLabel) {
        if (qobject_cast<const HeaderView *>(widget)
                && widget->property("multipleSortColumns").toBool()) {
            if (const auto *headerOpt = qstyleoption_cast<const QStyleOptionHeader *>(opt)) {
                QStyleOptionHeader myheaderOpt(*headerOpt);
                // .iconAlignment only works in the vertical direction.
                // switching to RTL is a very bad hack, but it gets the job done, even in the
                // native platforms styles.
                myheaderOpt.direction = Qt::RightToLeft;
                int iconSize = pixelMetric(PM_SmallIconSize, opt);

                const auto *headerView = static_cast<const HeaderView *>(widget);
                const auto sortColumns = headerView->sortColumns();

                for (int i = 0; i < sortColumns.size(); ++i) {
                    if (sortColumns.at(i).first == headerOpt->section) {
                        QString key = u"hv_" + QString::number(iconSize) + u"-"
                                      + QString::number(i) + u"-" + QString::number(
                                          headerView->isSorted() ? int(sortColumns.at(i).second) : 2);

                        QPixmap pix;
                        if (!QPixmapCache::find(key, &pix)) {
                            QString name = u"view-sort"_qs;
                            if (headerView->isSorted()) {
                                name = name + ((sortColumns.at(i).second == Qt::AscendingOrder)
                                               ? u"-ascending" : u"-descending");
                            }
                            pix = QIcon::fromTheme(name).pixmap(QSize(iconSize, iconSize),
                                                                widget->devicePixelRatio(),
                                                                (opt->state & State_Enabled) ?
                                                                    QIcon::Normal : QIcon::Disabled);

                            if (i > 0) {
                                QPixmap pix2(pix.size());
                                pix2.setDevicePixelRatio(pix.devicePixelRatio());
                                pix2.fill(Qt::transparent);
                                QPainter pPix2(&pix2);
                                pPix2.setOpacity(std::max(0.15, 0.75 - (i / 4.)));
                                pPix2.drawPixmap(QRect({ }, pix2.deviceIndependentSize().toSize()), pix);
                                pPix2.end();
                                pix = pix2;
                            }
                            QPixmapCache::insert(key, pix);
                        }
                        myheaderOpt.icon = QIcon(pix);
                    }
                }
                QProxyStyle::drawControl(element, &myheaderOpt, p, widget);
                return;
            }
        }
    }
    QProxyStyle::drawControl(element, opt, p, widget);

}

QSize BrickStoreProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                             const QSize &size, const QWidget *widget) const
{
    QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);

    if ((type == CT_ComboBox) && m_isWindowsVistaStyle) {
        // the Vista Style has problems with the width in high-dpi modes
        s.setWidth(int(s.width() + 16));
    } else if ((type == CT_ItemViewItem) && qobject_cast<const QAbstractItemView *>(widget)
               && widget->property("pinnableItems").toBool()) {
        if (const auto *viewItemOpt = qstyleoption_cast<const QStyleOptionViewItem *>(option))
            s.setWidth(s.width() + viewItemOpt->decorationSize.width() + 4);
    }
    return s;
}

QRect BrickStoreProxyStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
    QRect r = QProxyStyle::subElementRect(element, option, widget);

    if (element == SE_ItemViewItemText) {
        if (qobject_cast<const QAbstractItemView *>(widget) && widget->property("pinnableItems").toBool()) {
            if (const auto *viewItemOpt = qstyleoption_cast<const QStyleOptionViewItem *>(option))
                r.setWidth(std::max(0, r.width() - viewItemOpt->decorationSize.width() - 4));
        }
    }
    return r;
}

bool BrickStoreProxyStyle::eventFilter(QObject *o, QEvent *e)
{
    if (qobject_cast<QLineEdit *>(o)
            || qobject_cast<QDoubleSpinBox *>(o)
            || qobject_cast<QSpinBox *>(o)) {
        if (e->type() == QEvent::DynamicPropertyChange) {
            auto *dpce = static_cast<QDynamicPropertyChangeEvent *>(e);
            if (dpce->propertyName() == "showInputError") {
                auto *w = qobject_cast<QWidget *>(o);
                QPalette pal = QApplication::palette(w);

                if (w->property("showInputError").toBool()) {
                    auto anim = new QVariantAnimation(w);
                    QColor base = pal.color(QPalette::Base);
                    anim->setDuration(2000);
                    anim->setStartValue(base);
                    anim->setEndValue(Utility::gradientColor(base, Qt::red, 0.33f));
                    anim->setEasingCurve(QEasingCurve::InOutQuart);
                    connect(anim, &QAbstractAnimation::finished,
                            this, [anim]() {
                        anim->setDirection(anim->direction() == QAbstractAnimation::Forward ? QAbstractAnimation::Backward : QAbstractAnimation::Forward);
                        anim->start();
                    });
                    connect(anim, &QVariantAnimation::valueChanged,
                            w, [w](const QVariant &value) {
                        QPalette pal = QApplication::palette(w);
                        pal.setColor(QPalette::Base, value.value<QColor>());
                        w->setPalette(pal);
                    });
                    anim->start();
                    w->setProperty("bsShowInputErrorAnimation", QVariant::fromValue(anim));
                } else {
                    if (auto anim = w->property("bsShowInputErrorAnimation").value<QVariantAnimation *>()) {
                        anim->stop();
                        delete anim;
                    }
                    w->setPalette(pal);
                }
            }
        }
    } else if (qobject_cast<QAbstractItemView *>(o)) {
        // Prevent implicit Ctrl+C in QAbstractItemView, as our model strings have a finite life span
        if (e->type() == QEvent::KeyPress) {
            if (static_cast<QKeyEvent *>(e) == QKeySequence::Copy)
                return true;
        }
    }

    return QProxyStyle::eventFilter(o, e);
}
