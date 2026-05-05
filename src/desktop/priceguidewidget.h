// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QTreeView>

#include "common/currency.h"
#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QAction)
class PriceGuideWidgetPrivate;


// This isn't a QTreeView, let alone an item-view at all, but the Vista and macOS styles will
// only style widgets derived from at least QTreeView correctly when it comes to hovering and
// headers

class PriceGuideWidget : public QTreeView
{
    Q_OBJECT
public:
    PriceGuideWidget(QWidget *parent = nullptr);
    ~PriceGuideWidget() override;

    BrickLink::PriceGuide *priceGuide() const;

    enum Layout {
        Normal,
        Horizontal,
        Vertical
    };

    QSize sizeHint() const override;

    QString currencyCode() const;

public slots:
    void setLayout(PriceGuideWidget::Layout l);
    void setPriceGuide(BrickLink::PriceGuide *pg);
    void setCurrencyCode(const QString &code);

signals:
    void priceDoubleClicked(double p);

protected:
    void recalcLayout();
    void recalcLayoutNormal(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb);
    void recalcLayoutHorizontal(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb);
    void recalcLayoutVertical(const QSize &s, const QFontMetrics &fm, const QFontMetrics &fmb);

    void resizeEvent(QResizeEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *) override;
    void changeEvent(QEvent *e) override;

protected slots:
    void doUpdate();
    void gotUpdate(BrickLink::PriceGuide *pg);
    void showBLCatalogInfo();
    void showBLPriceGuideInfo();
    void showBLLotsForSale();
    void languageChange();

private:
    void paintHeader(QPainter *p, const QRect &r, Qt::Alignment align, const QString &str,
                     bool bold = false, bool left = false, bool right = false);
    void paintCell(QPainter *p, const QRect &r, Qt::Alignment align, const QString &str,
                   bool alternate = false, bool mouseOver = false);
    void updateNonStaticCells() const;
    void updateVatType(BrickLink::VatType vatType);

private:
    std::unique_ptr<PriceGuideWidgetPrivate> d;
};
