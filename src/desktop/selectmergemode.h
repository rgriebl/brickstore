/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
