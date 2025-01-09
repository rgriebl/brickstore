// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QButtonGroup>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QToolButton>
#include <QFormLayout>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QQmlApplicationEngine>

#include "bricklink/core.h"
#include "bricklink/item.h"
#include <common/application.h>
#include "common/config.h"
#include "scanner/core.h"
#include "scanner/capture.h"
#include "scanner/camerapreviewwidget.h"
#include "itemscannerdialog.h"

using namespace std::chrono_literals;


ItemScannerDialog::ItemScannerDialog(QWidget *parent)
    : QDialog(parent)
    , m_mediaDevices(new QMediaDevices(this))
    , m_capture(new Scanner::Capture(this))
{
    setSizeGripEnabled(true);

    m_selectCamera = new QComboBox(this);
    m_selectCamera->setFocusPolicy(Qt::NoFocus);
    connect(m_mediaDevices, &QMediaDevices::videoInputsChanged,
            this, &ItemScannerDialog::updateCameraDevices);

    m_selectBackend = new QComboBox(this);
    m_selectBackend->setFocusPolicy(Qt::NoFocus);

    m_selectItemType = new QButtonGroup(this);
    auto *itemTypesWidget = new QWidget(this);
    itemTypesWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *itemTypesLayout = new QHBoxLayout(itemTypesWidget);
    itemTypesLayout->setContentsMargins(0, 0, 0, 0);
    itemTypesLayout->setSpacing(2);

    auto createTypeButton = [&, this](const BrickLink::ItemType *itt) {
        auto *b = new QToolButton(this);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        b->setProperty("iconScaling", true);
        b->setCheckable(true);
        b->setChecked(!itt);
        b->setText(itt ? itt->name() : tr("Any"));
        m_selectItemType->addButton(b, itt ? itt->id() : '*');
        itemTypesLayout->addWidget(b, 1);
    };

    createTypeButton(nullptr);
    for (const auto &itt : BrickLink::core()->itemTypes())
        createTypeButton(&itt);

    m_cameraPreviewWidget = new Scanner::CameraPreviewWidget(Application::inst()->qmlEngine(), this);
    setFocusProxy(m_cameraPreviewWidget);
    m_cameraPreviewWidget->setFocus();
    m_capture->setVideoOutput(m_cameraPreviewWidget->videoOutput());
    m_capture->trackWindowVisibility(this);

    connect(m_cameraPreviewWidget, &Scanner::CameraPreviewWidget::clicked,
            m_capture, &Scanner::Capture::captureAndScan);
    connect(m_capture, &Scanner::Capture::cameraActiveChanged,
            m_cameraPreviewWidget, &Scanner::CameraPreviewWidget::setActive);

    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setRange(0, 100);
    connect(m_capture, &Scanner::Capture::progressChanged,
            m_progress, &QProgressBar::setValue);

    m_status = new QLabel(this);
    QPalette pal = m_status->palette();
    pal.setColor(QPalette::Text, pal.color(QPalette::Active, QPalette::Text));
    m_status->setPalette(pal);
    m_status->setTextFormat(Qt::RichText);
    m_status->setAlignment(Qt::AlignCenter);

    m_pinWindow = new QToolButton(this);
    m_pinWindow->setFocusPolicy(Qt::NoFocus);
    m_pinWindow->setCheckable(true);
    m_pinWindow->setProperty("iconScaling", true);

    auto updatePinIcon = [this]() {
        bool b = m_pinWindow->isChecked();
        m_pinWindow->setIcon(QIcon::fromTheme(b ? u"window-unpin"_qs : u"window-pin"_qs));
    };
    connect(m_pinWindow, &QToolButton::toggled, this, updatePinIcon);
    updatePinIcon();

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->addRow(m_labelCamera = new QLabel(this), m_selectCamera);
    form->addRow(m_labelBackend = new QLabel(this), m_selectBackend);
    form->addRow(m_labelItemType = new QLabel(this), itemTypesWidget);
    layout->addLayout(form);
    layout->addWidget(m_cameraPreviewWidget, 100);
    m_bottomStack = new QStackedLayout();
    m_bottomStack->setContentsMargins(0, 0, 0, 0);
    m_bottomStack->addWidget(m_status);
    m_bottomStack->addWidget(m_progress);
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(m_pinWindow);
    bottomLayout->addLayout(m_bottomStack, 1);
    layout->addLayout(bottomLayout, 0);

    updateCameraDevices();

    const QByteArrayList backendIds = Scanner::core()->availableBackendIds();
    for (const QByteArray &backendId : backendIds) {
        const auto *backend = Scanner::core()->backendFromId(backendId);
        m_selectBackend->addItem(QIcon(backend->icon()), backend->name, backendId);
    }

    m_selectCamera->setCurrentIndex(-1);
    connect(m_selectCamera, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0)
            m_capture->setCurrentCameraId(m_selectCamera->itemData(index).toByteArray());
    });
    m_selectBackend->setCurrentIndex(-1);
    connect(m_selectBackend, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0)
            m_capture->setCurrentBackendId(m_selectBackend->itemData(index).toByteArray());
    });
    connect(m_selectItemType, &QButtonGroup::idClicked,
            this, [this](int id) {
        m_capture->setCurrentItemTypeFilter(BrickLink::core()->itemType(char(id)));
    });

    //TODO: move default ids to Capture?
    const QByteArray cameraId = Config::inst()->value(u"MainWindow/ItemScannerDialog/CameraId"_qs).toByteArray();
    int cameraIndex = m_selectCamera->findData(cameraId);
    m_selectCamera->setCurrentIndex(cameraIndex < 0 ? 0 : cameraIndex);

    const QByteArray backendId = Config::inst()->value(u"MainWindow/ItemScannerDialog/BackendId"_qs).toByteArray();
    int backendIndex = m_selectBackend->findData(backendId);
    m_selectBackend->setCurrentIndex(backendIndex < 0 ? 0 : backendIndex);

    languageChange();

    if (!restoreGeometry(Config::inst()->value(u"MainWindow/ItemScannerDialog/Geometry"_qs).toByteArray())) {
        // the camera preview is most likely just black without this
        QMetaObject::invokeMethod(this, [this]() {
                m_cameraPreviewWidget->resize(m_cameraPreviewWidget->size() + QSize(1, 1));
            }, Qt::QueuedConnection);
    }
    m_pinWindow->setChecked(Config::inst()->value(u"MainWindow/ItemScannerDialog/Pinned"_qs, false).toBool());

    updateItemTypeFilters();
    connect(m_capture, &Scanner::Capture::supportedItemTypeFiltersChanged,
            this, &ItemScannerDialog::updateItemTypeFilters);
    connect(m_capture, &Scanner::Capture::currentItemTypeFilterChanged,
            this, &ItemScannerDialog::updateItemTypeFilters);

    connect(m_capture, &Scanner::Capture::stateChanged,
            this, [this](Scanner::Capture::State newState) {
        updateStatusText();

        if (newState == Scanner::Capture::State::Scanning)
            m_bottomStack->setCurrentWidget(m_progress);
        else
            m_bottomStack->setCurrentWidget(m_status);
    });

    connect(m_capture, &Scanner::Capture::captureAndScanFinished,
            this, [this](const QVector<const BrickLink::Item *> &items) {
        emit itemsScanned(items);
        if (!m_pinWindow->isChecked())
            accept();
    });
}

ItemScannerDialog::~ItemScannerDialog()
{
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/Geometry"_qs, saveGeometry());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/CameraId"_qs, m_selectCamera->currentData().toByteArray());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/BackendId"_qs, m_selectBackend->currentData().toByteArray());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/Pinned"_qs, m_pinWindow->isChecked());
}

void ItemScannerDialog::checkSystemPermissions(QObject *context, const std::function<void(bool)> &callback)
{
    Scanner::Capture::checkSystemPermissions(context, callback);
}

void ItemScannerDialog::setItemType(const BrickLink::ItemType *itemType)
{
    m_capture->setCurrentItemTypeFilter(itemType);
}

void ItemScannerDialog::updateCameraDevices()
{
    const auto allCameraDevices = QMediaDevices::videoInputs();

    QByteArray oldCameraId = m_selectCamera->currentData().toByteArray();
    int newCurrentIndex = -1;
    m_selectCamera->setCurrentIndex(-1);
    m_selectCamera->clear();

    const auto camIcon = QIcon::fromTheme(u"camera-photo"_qs);
    for (const auto &cameraDevice : allCameraDevices) {
        m_selectCamera->addItem(camIcon, cameraDevice.description(), cameraDevice.id());
        if (cameraDevice.id() == oldCameraId)
            newCurrentIndex = m_selectCamera->count() - 1;
    }
    if (newCurrentIndex >= 0)
        m_selectCamera->setCurrentIndex(newCurrentIndex);

    setEnabled(!allCameraDevices.isEmpty());
    updateStatusText();
}

void ItemScannerDialog::updateItemTypeFilters()
{
    const auto supportedItemTypes = m_capture->supportedItemTypeFilters();
    const auto *currentItemType = m_capture->currentItemTypeFilter();

    QAbstractButton *first = nullptr;
    bool checkedAny = false;

    const auto buttons = m_selectItemType->buttons();
    for (auto *b : buttons) {
        int id = m_selectItemType->id(b);
        const auto itt = BrickLink::core()->itemType(char(id));
        bool supported = !itt || supportedItemTypes.contains(itt);
        bool checked = supported && (itt == currentItemType);
        b->setVisible(supported);
        b->setChecked(checked);
        checkedAny = checkedAny || checked;
        if (!first && supported)
            first = b;
    }
    if (!checkedAny && first)
        first->setChecked(true);
}

void ItemScannerDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ItemScannerDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space)
        m_capture->captureAndScan();
    QDialog::keyPressEvent(e);
}

void ItemScannerDialog::languageChange()
{
    setWindowTitle(tr("Item Scanner"));
    updateStatusText();

    m_labelCamera->setText(tr("Camera"));
    m_labelBackend->setText(tr("Service"));
    m_labelItemType->setText(tr("Item type"));

    m_pinWindow->setToolTip(tr("Keep this window open"));
}

void ItemScannerDialog::updateStatusText()
{
    switch (m_capture->state()) {
    case Scanner::Capture::State::Idle:
        if (m_selectCamera->count())
            m_status->setText(tr("Click into the camera preview or press Space to capture an image."));
        else
            m_status->setText(u"<b>" + tr("There is no camera connected to this computer.") + u"</b>");
        break;
    case Scanner::Capture::State::NoMatch:
        m_status->setText(u"<b>" + tr("No matching item found - try again.") + u"</b>");
        break;
    case Scanner::Capture::State::Error:
        m_status->setText(u"<b>"_qs + tr("An error occurred:") + u"</b><br>" + m_capture->lastError());
        break;
    default:
        break;
    }
}


#include "moc_itemscannerdialog.cpp"
