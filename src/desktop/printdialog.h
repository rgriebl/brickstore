// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include <QPrintPreviewWidget>
#include "ui_printdialog.h"

QT_FORWARD_DECLARE_CLASS(QPdfWriter)
class View;

class PrintDialog : public QDialog, private Ui::PrintDialog
{
    Q_OBJECT

public:
    PrintDialog(bool asPdf, View *window);
    ~PrintDialog() override;

signals:
    void paintRequested(QPrinter *printer, const QList<uint> &pages, double scaleFactor,
                        uint *maxPageCount, double *maxWidth);

private:
    void updatePrinter(int idx);
    void updatePageRange();
    void updateColorMode();
    void updateOrientation();
    void updatePaperSize();
    void updateMargins();
    void updateScaling();
    void updateActions();
    void gotoPage(int page);
    void print();

    QPrinter *m_printer;
    QString m_documentName;
    QPushButton *m_printButton;
    QPrintPreviewWidget *w_print_preview;
    QPdfWriter *m_pdfWriter;
    QList<uint> m_pages;
    double m_scaleFactor = 1.;
    uint m_maxPageCount = 0;
    double m_maxWidth = 0.;
    bool m_saveAsPdf = false;
    bool m_hasSelection = false;
    uint m_freezeLoopWorkaround = 0;
    bool m_setupComplete = false;
};
