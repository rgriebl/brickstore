// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QWidget>
#include <QHash>
#include <QVector>

#include "common/documentmodel.h"

QT_FORWARD_DECLARE_CLASS(QButtonGroup)


class SelectMergeMode : public QWidget
{
    Q_OBJECT
public:
    SelectMergeMode(QWidget *parent = nullptr);
    explicit SelectMergeMode(DocumentModel::MergeMode defaultMode, QWidget *parent = nullptr);

    bool isQuantityEnabled() const;
    void setQuantityEnabled(bool enabled);

    void setFieldMergeModes(const DocumentModel::FieldMergeModes &modes);
    DocumentModel::FieldMergeModes fieldMergeModes() const;

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

private:
    void createFields();

    QButtonGroup *m_allGroup;
    QVector<QButtonGroup *> m_fieldGroups;
    DocumentModel::FieldMergeModes m_modes;
    bool m_quantityEnabled = true;
};
