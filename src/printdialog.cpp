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
#include "printdialog.h"
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QPdfWriter>
#include <QStringBuilder>
#include <QStackedWidget>
#include <QPushButton>
#include <QDir>
#include <QFileDialog>
#include <QDebug>
#include <QAbstractScrollArea>

#include <cmath>

#include "config.h"
#include "utility/messagebox.h"
#include "utility/utility.h"
#include "view.h"


PrintDialog::PrintDialog(QPrinter *printer, View *window)
    : QDialog(window)
    , m_printer(printer)
    , m_pdfWriter(new QPdfWriter(""_l1))
{
    setWindowFlags(windowFlags() | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMaximizeButtonHint);

    qRegisterMetaType<QPageSize>();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    static bool once = false;
    if (!once) {
        QMetaType::registerEqualsComparator<QPageSize>();
        once = true;
    }
#endif
    m_documentName = window->document()->fileName();
    if (!m_documentName.isEmpty())
        m_documentName = QFileInfo(m_documentName).completeBaseName();
    else
        m_documentName = window->document()->title();

    setupUi(this);

    w_print_preview = new QPrintPreviewWidget(m_printer);
    if (auto *asa = w_print_preview->findChild<QAbstractScrollArea *>()) {
        // workaround for QTBUG-20677
        asa->installEventFilter(this);
    }
    connect(w_print_preview, &QPrintPreviewWidget::paintRequested,
            this, [this](QPrinter *prt) {
        emit paintRequested(prt, m_pages, m_scaleFactor, &m_maxPageCount, &m_maxWidth);
    });
    connect(w_print_preview, &QPrintPreviewWidget::previewChanged,
            this, [this]() {
        updateActions();
    });

    QVBoxLayout *containerLayout = new QVBoxLayout(w_print_preview_container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(w_print_preview);

    w_pageSelect->hide();
    w_pageSelectWarning->hide();
    w_scalePercent->hide();

    w_sysprint->setText(R"(<a href="sysprint">)"_l1 % tr("Print using the system dialog...") % "</a>"_l1);

    connect(w_page_first, &QToolButton::clicked,
            this, [this]() { gotoPage(1); });
    connect(w_page_prev, &QToolButton::clicked,
            this, [this]() { gotoPage(w_print_preview->currentPage() - 1); });
    connect(w_page_next, &QToolButton::clicked,
            this, [this]() { gotoPage(w_print_preview->currentPage() + 1); });
    connect(w_page_last, &QToolButton::clicked,
            this, [this]() { gotoPage(w_print_preview->pageCount()); });
    connect(w_zoom_in, &QToolButton::clicked,
            this, [this]() { w_print_preview->zoomIn(); updateActions(); });
    connect(w_zoom_out, &QToolButton::clicked,
            this, [this]() { w_print_preview->zoomOut(); updateActions(); });
    connect(w_page_single, &QToolButton::clicked,
            w_print_preview, &QPrintPreviewWidget::setSinglePageViewMode);
    connect(w_page_double, &QToolButton::clicked,
            w_print_preview, &QPrintPreviewWidget::setAllPagesViewMode);

    connect(w_fit_width, &QToolButton::clicked,
            w_print_preview, &QPrintPreviewWidget::fitToWidth);
    connect(w_fit_best, &QToolButton::clicked,
            w_print_preview, &QPrintPreviewWidget::fitInView);

    connect(w_buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    w_buttons->addButton(m_printButton = new QPushButton(), QDialogButtonBox::AcceptRole);
    connect(w_buttons, &QDialogButtonBox::accepted,
            this, &PrintDialog::print);

    connect(w_printers, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::updatePrinter);
    connect(w_pageMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        w_pageSelect->setVisible(idx == 1);
        updatePageRange();
    });
    connect(w_pageSelect, &QLineEdit::textChanged,
            this, &PrintDialog::updatePageRange);
    connect(w_layout, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int idx) {
        if (w_print_preview)
            w_print_preview->setOrientation(idx == 0 ?
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                                                QPrinter::Portrait: QPrinter::Landscape);
#else
                                                QPageLayout::Orientation::Portrait : QPageLayout::Orientation::Landscape);
#endif
    });
    connect(w_color, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::updateColorMode);
    connect(w_paperSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::updatePaperSize);
    connect(w_margins, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::updateMargins);
    connect(w_copies, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) { m_printer->setCopyCount(v); });

    connect(w_scaleMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        w_scalePercent->setVisible(idx == 2);
        updateScaling();
    });
    connect(w_scalePercent, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PrintDialog::updateScaling);

    connect(w_sysprint, &QLabel::linkActivated,
            this, [this, window](const QString &) {
        if (m_saveAsPdf)
            m_printer->setPrinterName(QPrinter().printerName());

        QPrintDialog pd(m_printer, this);

        pd.setOption(QAbstractPrintDialog::PrintToFile);
        pd.setOption(QAbstractPrintDialog::PrintCollateCopies);
        pd.setOption(QAbstractPrintDialog::PrintShowPageSize);
        pd.setOption(QAbstractPrintDialog::PrintPageRange);

        if (!window->selectedLots().isEmpty())
            pd.setOption(QAbstractPrintDialog::PrintSelection);

        pd.setPrintRange(window->selectedLots().isEmpty() ? QAbstractPrintDialog::AllPages
                                                          : QAbstractPrintDialog::Selection);
        if (pd.exec() == QDialog::Accepted)
            print();
    });

    const QList<QPrinterInfo> printers = QPrinterInfo::availablePrinters();

    int idx = 0;
    int defaultIdx = -1;
    QSignalBlocker blocker(w_printers); // delay initialization
    for (const QPrinterInfo &printer : printers) {
        w_printers->addItem(QIcon::fromTheme("document-print"_l1), printer.description(),
                            printer.printerName());
        if (printer.isDefault())
            defaultIdx = idx;
        ++idx;
    }
    if (w_printers->count())
        w_printers->insertSeparator(w_printers->count());
    w_printers->addItem(QIcon::fromTheme("document-save-as"_l1), tr("Save as PDF"),
                        QString::fromLatin1("__PDF__"));

    if ((defaultIdx == -1) || (printer->outputFormat() == QPrinter::PdfFormat))
        defaultIdx = w_printers->count() - 1;

    QMetaObject::invokeMethod(this, [this, defaultIdx]() {
        w_printers->setCurrentIndex(defaultIdx);
        updatePrinter(defaultIdx);
    }, Qt::QueuedConnection);
}

bool PrintDialog::eventFilter(QObject *o, QEvent *e)
{
    if (qobject_cast<QAbstractScrollArea *>(o)) {
        if (e->type() == QEvent::MetaCall) {
            if (++m_freezeLoopWorkaround > 11)
                return true;
        } else {
            m_freezeLoopWorkaround = 0;
        }
    }
    return false;
}

void PrintDialog::updatePrinter(int idx)
{
    QString printerKey = w_printers->itemData(idx).toString();

    QList<QPageSize> pageSizes;
    QPageSize defaultPageSize;
    QList<QPrinter::ColorMode> colorModes;
    QPrinter::ColorMode defaultColorMode;

    if (printerKey == "__PDF__"_l1) {
        static const QList<QPageSize> pdfPageSizes = {
            QPageSize(QPageSize::Letter),
            QPageSize(QPageSize::Legal),
            QPageSize(QPageSize::ExecutiveStandard),
            QPageSize(QPageSize::A3),
            QPageSize(QPageSize::A4),
            QPageSize(QPageSize::A5),
            QPageSize(QPageSize::JisB4),
            QPageSize(QPageSize::JisB5),
            QPageSize(QPageSize::Prc16K),
        };
        pageSizes = pdfPageSizes;
        defaultPageSize = m_pdfWriter->pageLayout().pageSize();

        colorModes = { QPrinter::Color, QPrinter::GrayScale };
        defaultColorMode = QPrinter::Color;

        m_printer->setPrinterName(QString { });
    } else {
        auto printer = QPrinterInfo::printerInfo(printerKey);

        pageSizes = printer.supportedPageSizes();
        defaultPageSize = printer.defaultPageSize();

        colorModes = printer.supportedColorModes();
        defaultColorMode = printer.defaultColorMode();

        m_printer->setPrinterName(printerKey);
    }

    QSignalBlocker blocker(this);

    w_paperSize->clear();
    for (const auto &ps : qAsConst(pageSizes))
        w_paperSize->addItem(ps.name(), QVariant::fromValue(ps));
    w_paperSize->setCurrentIndex(w_paperSize->findData(QVariant::fromValue(defaultPageSize)));

    w_color->clear();
    if (colorModes.contains(QPrinter::Color))
        w_color->addItem(tr("Color"), int(QPrinter::Color));
    if (colorModes.contains(QPrinter::GrayScale))
        w_color->addItem(tr("Black and white"), int(QPrinter::GrayScale));
    w_color->setCurrentIndex(w_color->findData(int(defaultColorMode)));

    w_pageMode->setCurrentIndex(0);

    updateColorMode();
    updateMargins();
    updatePaperSize();
    updateActions();

    blocker.unblock();
    w_print_preview->updatePreview();

    m_saveAsPdf = m_printer->printerName().isEmpty();

    m_printButton->setText(m_saveAsPdf ? tr("Save") : tr("Print"));
}

void PrintDialog::updatePageRange()
{
    if (!m_printer || !w_print_preview)
        return;
    bool allPages = (w_pageMode->currentIndex() != 1);
    QString s = w_pageSelect->text().simplified().remove(' '_l1);
    if (!allPages && s.isEmpty())
        allPages = true;

    m_printer->setPrintRange(allPages ? QPrinter::AllPages : QPrinter::PageRange);

    QSet<uint> pages;
    bool ok = true;

    if (!allPages) {
        const auto ranges = s.split(','_l1);
        for (const QString &range : ranges) {
            const QStringList fromTo = range.split('-'_l1);
            uint from = 0, to = 0;
            if (fromTo.size() == 1) {
                from = to = fromTo.at(0).toUInt(&ok);
            } else if (fromTo.size() == 2) {
                QString fromStr = fromTo.at(0);
                QString toStr = fromTo.at(1);

                if (fromStr.isEmpty() && toStr.isEmpty()) {
                    ok = false;
                } else if (fromStr.isEmpty()) {
                    from = 1;
                    to = toStr.toUInt(&ok);
                } else if (toStr.isEmpty()) {
                    from = fromStr.toUInt(&ok);
                    to = m_maxPageCount;
                } else {
                    from = fromStr.toUInt(&ok);
                    to = toStr.toUInt(&ok);
                }
            } else {
                ok = false;
            }
            if (ok && ((from < 1) || (to > m_maxPageCount) || (from > to)))
                ok = false;
            if (ok) {
                for (uint i = from; i <= to; ++i)
                    pages.insert(i);
            } else {
                break;
            }
        }
    }
    if (ok) {
        m_pages = QList<uint>(pages.cbegin(), pages.cend());
        w_print_preview->updatePreview();
    }
    w_pageSelectWarning->setVisible(!ok);
}

void PrintDialog::updateColorMode()
{
    if (!m_printer || !w_print_preview)
        return;
    m_printer->setColorMode(QPrinter::ColorMode(w_color->currentData().toInt()));
    w_print_preview->updatePreview();
}

void PrintDialog::updatePaperSize()
{
    if (!m_printer || !w_print_preview)
        return;
    m_printer->setPageSize(w_paperSize->currentData().value<QPageSize>());
    w_print_preview->updatePreview();
}

void PrintDialog::updateMargins()
{
    if (!m_printer || !w_print_preview)
        return;

    auto pl = m_printer->pageLayout();
    QMarginsF mm = pl.minimumMargins();
    QMarginsF m;
    switch (w_margins->currentIndex()) {
    case 0: m = QMarginsF(qMax(30., mm.left()), qMax(30., mm.top()), // 30pt == 10.5mm
                          qMax(30., mm.right()), qMax(30., mm.bottom())); break; // default
    case 1: break; // none
    case 2: m = mm; break; // minimal
    }
    pl.setMargins(m);
    m_printer->setPageLayout(pl);
    w_print_preview->updatePreview();
}

void PrintDialog::updateScaling()
{
    int percent = 100;
    switch (w_scaleMode->currentIndex()) {
    case 1:
        if (!qFuzzyIsNull(m_maxWidth)) {
            QRectF pr = m_printer->pageLayout().paintRect(QPageLayout::Inch);
            double dpi = double(m_printer->logicalDpiX() + m_printer->logicalDpiY()) / 2;
            percent = int(pr.width() * dpi / m_maxWidth * 100);
        }
        break;
    case 2:
        percent = w_scalePercent->value();
        break;
    }
    double scale = double(percent) / 100;

    if (!qFuzzyCompare(scale, m_scaleFactor)) {
        m_scaleFactor = scale;
        w_print_preview->updatePreview();
    }
    w_scalePercent->setValue(percent);
}

void PrintDialog::updateActions()
{
    int p = w_print_preview->currentPage();
    int last = w_print_preview->pageCount();

    w_page_first->setEnabled(p > 1);
    w_page_prev->setEnabled(p > 1);
    w_page_next->setEnabled(p < last);
    w_page_last->setEnabled(p < last);

    w_pages->setText(QString::number(p) % u" / " % QString::number(last));
    double z = w_print_preview->zoomFactor() * logicalDpiX() / physicalDpiX();
    w_zoom->setText(QString::number(int(z * 100)) % u" %");
}

void PrintDialog::gotoPage(int page)
{
    if ((page >= 1) && (page <= w_print_preview->pageCount())) {
        w_print_preview->setCurrentPage(page);
        updateActions();
    }
}

void PrintDialog::print()
{
    if (m_saveAsPdf) {
        QString suffix = ".pdf"_l1;
        QString pdfname = m_documentName + suffix;

        QDir d(Config::inst()->documentDir());
        if (d.exists())
            pdfname = d.filePath(pdfname);

        pdfname = QFileDialog::getSaveFileName(this, tr("Save as PDF"), pdfname,
                                               tr("PDF Documents") % u" (*" % suffix % u")");
        if (pdfname.isEmpty())
            return;
        if (QFileInfo(pdfname).suffix().isEmpty())
            pdfname.append(suffix);

        // check if the file is actually writable - otherwise QPainter::begin() will fail
        if (QFile::exists(pdfname) && !QFile(pdfname).open(QFile::ReadWrite)) {
            MessageBox::warning(this, { }, tr("The PDF document already exists and is not writable."));
            return;
        }
        m_printer->setOutputFileName(pdfname);
    }
    w_print_preview->print();
    accept();
}
