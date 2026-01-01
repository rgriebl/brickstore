// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QCoreApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QHelpEvent>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QQmlEngine>
#include <QtQuickWidgets/QQuickWidget>

#include "camerapreviewwidget.h"


namespace Scanner {

CameraPreviewWidget::CameraPreviewWidget(QQmlEngine *engine, QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_widget = engine ? new QQuickWidget(engine, this) : new QQuickWidget(this);
    m_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_widget->setClearColor(Qt::black);
    m_widget->setSource(u"qrc:/Scanner/CameraPreview.qml"_qs);

    if (auto *ro = m_widget->rootObject()) {
        connect(ro, SIGNAL(clicked()), this, SIGNAL(clicked())); // clazy:exclude=old-style-connect
    } else {
        qWarning() << "CameraPreviewWidget: failed to load QML file:";
        const auto errors = m_widget->errors();
        for (const auto &err : errors)
            qWarning() << "  " << err.toString();
    }

    int videoWidth = logicalDpiX() * 3; // ~7.5cm on-screen
    m_widget->setMinimumSize(videoWidth, videoWidth * 9 / 16);
    m_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(m_widget, 10);
}

CameraPreviewWidget::~CameraPreviewWidget()
{
}

bool CameraPreviewWidget::isActive() const
{
    return m_widget->rootObject() ? m_widget->rootObject()->property("active").toBool() : false;
}

void CameraPreviewWidget::setActive(bool newActive)
{
    if (m_widget->rootObject())
        m_widget->rootObject()->setProperty("active", newActive);
}

QObject *CameraPreviewWidget::videoOutput() const
{
    return m_widget->rootObject();
}

} // namespace Scanner

#include "moc_camerapreviewwidget.cpp"
