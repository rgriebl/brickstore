// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QButtonGroup>
#include <QToolButton>
#include <QLabel>
#include <QGridLayout>

#include "selectmergemode.h"


SelectMergeMode::SelectMergeMode(QWidget *parent)
    : SelectMergeMode(DocumentModel::MergeMode::MergeAverage, parent)
{ }

SelectMergeMode::SelectMergeMode(DocumentModel::MergeMode defaultMode, QWidget *parent)
    : QWidget(parent)
{
    createFields();
    setFieldMergeModes(DocumentModel::createFieldMergeModes(defaultMode));
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
}

bool SelectMergeMode::isQuantityEnabled() const
{
    return m_quantityEnabled;
}

void SelectMergeMode::setQuantityEnabled(bool enabled)
{
    if (enabled != m_quantityEnabled) {
        m_quantityEnabled = enabled;

        if (!enabled)
            m_modes.remove(DocumentModel::Quantity);

        for (QButtonGroup *fieldGroup : std::as_const(m_fieldGroups)) {
            const auto fields = fieldGroup->property("documentFields").value<QVector<DocumentModel::Field>>();

            if ((fields.size() == 1) && (fields.constFirst() == DocumentModel::Quantity)) {
                if (auto *label = fieldGroup->property("fieldGroupLabel").value<QLabel *>())
                    label->setVisible(enabled);
                const auto buttons = fieldGroup->buttons();
                for (auto *button : buttons)
                    button->setVisible(enabled);
                break;
            }
        }
    }
}

void SelectMergeMode::setFieldMergeModes(const DocumentModel::FieldMergeModes &modes)
{
    if (m_modes != modes) {
        m_modes = modes;

        if (!isQuantityEnabled())
            m_modes.remove(DocumentModel::Quantity);

        for (QButtonGroup *fieldGroup : std::as_const(m_fieldGroups)) {
            const auto fields = fieldGroup->property("documentFields").value<QVector<DocumentModel::Field>>();

            Q_ASSERT(!fields.isEmpty());

            auto mode = modes.value(DocumentModel::Field(fields.constFirst()), DocumentModel::MergeMode::Ignore);

            for (int field : fields) {
                auto otherMode = modes.value(DocumentModel::Field(field), DocumentModel::MergeMode::Ignore);
                Q_ASSERT(otherMode == mode);
                if (otherMode != mode)
                    qWarning() << "setFieldMergeModes: differing modes for group" << fieldGroup->objectName();
            }
            auto *button = fieldGroup->button(int(mode));
            if (!button)
                button = fieldGroup->button(int(DocumentModel::MergeMode::Ignore));
            Q_ASSERT(button);
            button->setChecked(true);
        }
    }
}

DocumentModel::FieldMergeModes SelectMergeMode::fieldMergeModes() const
{
    return m_modes;
}

QByteArray SelectMergeMode::saveState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SMM") << qint32(2);
    ds << m_modes;
    return ba;
}

bool SelectMergeMode::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "SMM") || (version != 2))
        return false;

    DocumentModel::FieldMergeModes modes;
    ds >> modes;
    if (ds.status() != QDataStream::Ok)
        return false;

    setFieldMergeModes(modes);
    return true;
}

void SelectMergeMode::createFields()
{
    static const struct {
        DocumentModel::MergeMode mergeMode;
        const char *name;
        const char *toolTip;
        const char *icon;
    } modes[] = {
    { DocumentModel::MergeMode::Ignore,
                QT_TR_NOOP("Ignore"),
                QT_TR_NOOP("Leave the destination value as is"),
                "process-stop", },
    { DocumentModel::MergeMode::Copy,
                QT_TR_NOOP("Copy"),
                QT_TR_NOOP("Set to source value"),
                "edit-copy", },
    { DocumentModel::MergeMode::Merge,
                QT_TR_NOOP("Merge"),
                QT_TR_NOOP("Set to source value, but only if destination is at default value"),
                "join", },
    { DocumentModel::MergeMode::MergeText,
                QT_TR_NOOP("Merge text"),
                QT_TR_NOOP("Merge the text from the source and the destination"),
                "edit-select-text", },
    { DocumentModel::MergeMode::MergeAverage,
                QT_TR_NOOP("Merge average"),
                QT_TR_NOOP("Merge field by calculating a quantity average"),
                "taxes-finances", },
    };

    static const struct {
        const char *name;
        QVector<DocumentModel::Field> fields;
    } fields[] = {
    { QT_TR_NOOP("Price"),           { DocumentModel::Price } },
    { QT_TR_NOOP("Cost"),            { DocumentModel::Cost } },
    { QT_TR_NOOP("Tier prices"),     { DocumentModel::TierP1, DocumentModel::TierP2, DocumentModel::TierP3 } },
    { QT_TR_NOOP("Quantity"),        { DocumentModel::Quantity } },
    { QT_TR_NOOP("Bulk quantity"),   { DocumentModel::Bulk } },
    { QT_TR_NOOP("Sale percentage"), { DocumentModel::Sale } },
    { QT_TR_NOOP("Comment"),         { DocumentModel::Comments } },
    { QT_TR_NOOP("Remark"),          { DocumentModel::Remarks } },
    { QT_TR_NOOP("Reserved"),        { DocumentModel::Reserved } },
    { QT_TR_NOOP("Retain flag"),     { DocumentModel::Retain } },
    { QT_TR_NOOP("Stockroom"),       { DocumentModel::Stockroom } },
    };

    auto grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(6, 100);
    grid->setSpacing(1);
    int row = 0;
    int col = 0;

    auto label = new QLabel(tr("All fields"));
    grid->addWidget(label, row, 0);
    m_allGroup = new QButtonGroup();
    for (const auto &mode : modes) {
        auto tb = new QToolButton();
        tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tb->setIcon(QIcon::fromTheme(QString::fromLatin1(mode.icon)));
        tb->setText(tr(mode.name));
        tb->setToolTip(tr(mode.toolTip));
        tb->setProperty("iconScaling", true);
        grid->addWidget(tb, row, col + 1);
        m_allGroup->addButton(tb, int(mode.mergeMode));
        ++col;
    }

    connect(m_allGroup, &QButtonGroup::idClicked,
            this, [this](int id) {
        auto mergeMode = DocumentModel::MergeMode(id);
        setFieldMergeModes(DocumentModel::createFieldMergeModes(mergeMode));
    });

    ++row;
    auto hbar = new QFrame();
    hbar->setFrameStyle(QFrame::HLine);
    grid->addWidget(hbar, row, 0, 1, 6);

    ++row;
    col = 0;

    for (const auto &field : fields) {
        label = new QLabel(tr(field.name));
        grid->addWidget(label, row, 0);
        auto fieldGroup = new QButtonGroup();
        fieldGroup->setObjectName(QString::fromLatin1(field.name));
        fieldGroup->setProperty("documentFields", QVariant::fromValue(field.fields));
        fieldGroup->setProperty("fieldGroupLabel", QVariant::fromValue(label));

        auto possibleModes = DocumentModel::possibleMergeModesForField(field.fields.at(0));
        for (const auto &mode : modes) {
            if ((mode.mergeMode == DocumentModel::MergeMode::Ignore)
                    || (possibleModes & mode.mergeMode)) {
                auto tb = new QToolButton();
                tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                tb->setIcon(QIcon::fromTheme(QString::fromLatin1(mode.icon)));
                tb->setText(tr(mode.name));
                tb->setToolTip(tr(mode.toolTip));
                tb->setCheckable(true);
                tb->setProperty("iconScaling", true);
                grid->addWidget(tb, row, col + 1);
                fieldGroup->addButton(tb, int(mode.mergeMode));
            }
            ++col;
        }
        m_fieldGroups.append(fieldGroup);

        connect(fieldGroup, &QButtonGroup::idClicked,
                this, [this, fieldGroup](int id) {
            DocumentModel::FieldMergeModes newModes = m_modes;

            const auto mode = static_cast<DocumentModel::MergeMode>(id);
            const auto docFields = fieldGroup->property("documentFields").value<QVector<DocumentModel::Field>>();
            for (const auto &docField : docFields) {
                if (mode == DocumentModel::MergeMode::Ignore)
                    newModes.remove(docField);
                else
                    newModes.insert(docField, mode);
            }
            if (newModes != m_modes)
                m_modes = newModes;
        });

        ++row;
        col = 0;
    }
}

