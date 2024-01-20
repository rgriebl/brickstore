// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QCoreApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QHelpEvent>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QQmlEngine>
#include <QtQuickWidgets/QQuickWidget>

#include "camerapreviewwidget.h"


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

    connect(m_widget->rootObject(), SIGNAL(clicked()), // clazy:exclude=old-style-connect
            this, SIGNAL(clicked()));

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
    return m_widget->rootObject()->property("active").toBool();
}

void CameraPreviewWidget::setActive(bool newActive)
{
    m_widget->rootObject()->setProperty("active", newActive);
}

QObject *CameraPreviewWidget::videoOutput() const
{
    return m_widget->rootObject();
}

#include "moc_camerapreviewwidget.cpp"
