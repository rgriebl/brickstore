/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
