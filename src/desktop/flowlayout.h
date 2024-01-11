// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QLayout>
#include <QRect>
#include <QVector>
#include <QStyle>

// based on Qt's flowlayout example

class FlowLayout : public QLayout
{
public:
    enum FlowMode {
        HorizontalFirst,
        VerticalOnly,
    };

    explicit FlowLayout(QWidget *parent, FlowMode mode, int margin = -1, int hSpacing = -1,
                        int vSpacing = -1);
    explicit FlowLayout(FlowMode mode, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    int horizontalSpacing() const;
    int verticalSpacing() const;

    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    void addItem(QLayoutItem *item) override;

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;

    void setGeometry(const QRect &rect) override;

private:
    int smartSpacing(QStyle::PixelMetric pm) const;
    int doLayout(const QRect &rect, bool testOnly) const;

    QVector<QLayoutItem *> m_items;
    FlowMode m_mode;
    int m_hspace;
    int m_vspace;
};
