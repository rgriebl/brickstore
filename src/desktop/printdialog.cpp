// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "printdialog.h"
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QPdfWriter>
#include <QStackedWidget>
#include <QPushButton>
#include <QDir>
#include <QFileDialog>
#include <QDebug>
#include <QAbstractScrollArea>
#include <QMessageBox>

#include <cmath>

#include "common/config.h"
#include "common/document.h"
#include "common/eventfilter.h"
#include "view.h"


PrintDialog::PrintDialog(bool asPdf, View *window)
    : QDialog(window)
    , m_printer(new QPrinter(QPrinter::HighResolution))
    , m_pdfWriter(new QPdfWriter(QString { }))
{
    if (asPdf)
        m_printer->setPrinterName(QString { });

    setWindowFlags(windowFlags() | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMaximizeButtonHint);

    qRegisterMetaType<QPageSize>();
    m_documentName = window->document()->filePathOrTitle();
    m_hasSelection = !window->selectedLots().isEmpty();

    setupUi(this);

    w_page_first->setProperty("toolBarLike", true);
    w_page_prev->setProperty("toolBarLike", true);
    w_page_next->setProperty("toolBarLike", true);
    w_page_last->setProperty("toolBarLike", true);
    w_page_double->setProperty("toolBarLike", true);
    w_page_single->setProperty("toolBarLike", true);
    w_fit_best->setProperty("toolBarLike", true);
    w_fit_width->setProperty("toolBarLike", true);
    w_zoom_in->setProperty("toolBarLike", true);
    w_zoom_out->setProperty("toolBarLike", true);

    if (!m_hasSelection)
        w_pageMode->removeItem(1);

    w_print_preview = new QPrintPreviewWidget(m_printer);
    if (auto *asa = w_print_preview->findChild<QAbstractScrollArea *>()) {
        // workaround for QTBUG-20677
        new EventFilter(asa, { }, [this](QObject *, QEvent *e) {
            if (e->type() == QEvent::MetaCall) {
                if (++m_freezeLoopWorkaround > 11)
                    return EventFilter::StopEventProcessing;
            } else {
                m_freezeLoopWorkaround = 0;
            }
            return EventFilter::ContinueEventProcessing;
        });
    }
    connect(w_print_preview, &QPrintPreviewWidget::paintRequested,
            this, [this](QPrinter *prt) {
        if (!m_setupComplete)
            return;
        emit paintRequested(prt, m_pages, m_scaleFactor, &m_maxPageCount, &m_maxWidth);
    });
    connect(w_print_preview, &QPrintPreviewWidget::previewChanged,
            this, [this]() {
        updateActions();
    });

    auto *containerLayout = new QVBoxLayout(w_print_preview_container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(w_print_preview);

    w_pageSelect->hide();
    w_pageSelectWarning->hide();
    w_scalePercent->hide();

    w_sysprint->setText(uR"(<a href="sysprint">)" + tr("Print using the system dialog...") + u"</a>");

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
        w_pageSelect->setVisible(idx == (w_pageMode->count() - 1));
        updatePageRange();
    });
    connect(w_pageSelect, &QLineEdit::textChanged,
            this, &PrintDialog::updatePageRange);
    connect(w_layout, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::updateOrientation);
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

        QPrintDialog dlg(m_printer, this);

        dlg.setOption(QAbstractPrintDialog::PrintToFile);
        dlg.setOption(QAbstractPrintDialog::PrintCollateCopies);
        dlg.setOption(QAbstractPrintDialog::PrintShowPageSize);
        dlg.setOption(QAbstractPrintDialog::PrintPageRange);

        if (!window->selectedLots().isEmpty())
            dlg.setOption(QAbstractPrintDialog::PrintSelection);

        dlg.setPrintRange(window->selectedLots().isEmpty() ? QAbstractPrintDialog::AllPages
                                                           : QAbstractPrintDialog::Selection);

        if (dlg.exec() == QDialog::Accepted)
            print();
    });

    const QList<QPrinterInfo> printers = QPrinterInfo::availablePrinters();

    int idx = 0;
    int defaultIdx = -1;
    QSignalBlocker blocker(w_printers); // delay initialization
    for (const QPrinterInfo &printer : printers) {
        w_printers->addItem(QIcon::fromTheme(u"document-print"_qs), printer.description(),
                            printer.printerName());
        if (printer.isDefault())
            defaultIdx = idx;
        ++idx;
    }
    if (w_printers->count())
        w_printers->insertSeparator(w_printers->count());
    w_printers->addItem(QIcon::fromTheme(u"document-save-as"_qs), tr("Save as PDF"), u"__PDF__"_qs);

    if ((defaultIdx == -1) || (m_printer->outputFormat() == QPrinter::PdfFormat))
        defaultIdx = w_printers->count() - 1;

    w_printers->setCurrentIndex(defaultIdx);

    QByteArray ba = Config::inst()->value(u"MainWindow/PrintDialog/Geometry"_qs).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    QMetaObject::invokeMethod(this, [this]() {
        updatePrinter(w_printers->currentIndex());
        m_setupComplete = true;
        w_print_preview->updatePreview();
   }, Qt::QueuedConnection);
}

PrintDialog::~PrintDialog()
{
    Config::inst()->setValue(u"MainWindow/PrintDialog/Geometry"_qs, saveGeometry());

    delete m_pdfWriter;
    delete m_printer;
}

void PrintDialog::updatePrinter(int idx)
{
    QString printerKey = w_printers->itemData(idx).toString();

    QList<QPageSize> pageSizes;
    QPageSize defaultPageSize;
    QList<QPrinter::ColorMode> colorModes;
    QPrinter::ColorMode defaultColorMode;

    if (printerKey == u"__PDF__") {
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
    for (const auto &ps : std::as_const(pageSizes))
        w_paperSize->addItem(ps.name(), QVariant::fromValue(ps));
    w_paperSize->setCurrentIndex(w_paperSize->findData(QVariant::fromValue(defaultPageSize)));

    w_color->clear();
    if (colorModes.contains(QPrinter::Color))
        w_color->addItem(tr("Color"), int(QPrinter::Color));
    if (colorModes.contains(QPrinter::GrayScale))
        w_color->addItem(tr("Black and white"), int(QPrinter::GrayScale));
    w_color->setCurrentIndex(w_color->findData(int(defaultColorMode)));

    w_pageMode->setCurrentIndex(m_hasSelection ? 1 : 0);

    updateColorMode();
    updateMargins();
    updatePaperSize();
    updateScaling();
    updateActions();

    blocker.unblock();

    m_saveAsPdf = m_printer->printerName().isEmpty();
    m_printButton->setText(m_saveAsPdf ? tr("Save") : tr("Print"));

    w_print_preview->updatePreview();
}

void PrintDialog::updatePageRange()
{
    if (!m_printer || !w_print_preview)
        return;
    bool allPages = (w_pageMode->currentIndex() == 0);
    bool selectionOnly = m_hasSelection && (w_pageMode->currentIndex() == 1);
    bool customPages = (w_pageMode->currentIndex() == (w_pageMode->count() - 1));

    QString s = w_pageSelect->text().simplified().remove(u' ');
    if (customPages && s.isEmpty())
        allPages = true;

    m_printer->setPrintRange(selectionOnly ? QPrinter::Selection
                                           : (allPages ? QPrinter::AllPages : QPrinter::PageRange));

    QSet<uint> pages;
    bool ok = true;

    if (customPages && !allPages) {
        const auto ranges = s.split(u","_qs);
        for (const QString &range : ranges) {
            const QStringList fromTo = range.split(u"-"_qs);
            uint from = 0, to = 0;
            if (fromTo.size() == 1) {
                from = to = fromTo.at(0).toUInt(&ok);
            } else if (fromTo.size() == 2) {
                const QString &fromStr = fromTo.at(0);
                const QString &toStr = fromTo.at(1);

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

void PrintDialog::updateOrientation()
{
    if (!m_printer || !w_print_preview)
        return;
    w_print_preview->setOrientation(w_layout->currentIndex() == 0 ? QPageLayout::Orientation::Portrait
                                                                  : QPageLayout::Orientation::Landscape);
    updateScaling();
}

void PrintDialog::updatePaperSize()
{
    if (!m_printer || !w_print_preview)
        return;
    m_printer->setPageSize(w_paperSize->currentData().value<QPageSize>());
    updateScaling();
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
    case 0: m = QMarginsF(std::max(30., mm.left()),  std::max(30., mm.top()), // 30pt == 10.5mm
                          std::max(30., mm.right()), std::max(30., mm.bottom())); break; // default
    case 1: break; // none
    case 2: m = mm; break; // minimal
    }
    pl.setMargins(m);
    m_printer->setPageLayout(pl);
    updateScaling();
    w_print_preview->updatePreview();
}

void PrintDialog::updateScaling()
{
    int percent = 100;
    switch (w_scaleMode->currentIndex()) {
    case 1: // fit to width
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

    w_pages->setText(QString::number(p) + u" / " + QString::number(last));
    double z = w_print_preview->zoomFactor() * logicalDpiX() / physicalDpiX();
    w_zoom->setText(QString::number(int(z * 100)) + u" %");
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
        QString suffix = u".pdf"_qs;
        QString pdfname = m_documentName + suffix;

        QDir d(Config::inst()->documentDir());
        if (d.exists())
            pdfname = d.filePath(pdfname);

        pdfname = QFileDialog::getSaveFileName(this, tr("Save as PDF"), pdfname,
                                               tr("PDF Documents") + u" (*" + suffix + u")");
        if (pdfname.isEmpty())
            return;
        if (QFileInfo(pdfname).suffix().isEmpty())
            pdfname.append(suffix);

        // check if the file is actually writable - otherwise QPainter::begin() will fail
        if (QFile::exists(pdfname) && !QFile(pdfname).open(QFile::ReadWrite)) {
            QMessageBox::warning(this, tr("Save as PDF"),
                                 tr("The PDF document already exists and is not writable."));
            return;
        }
        m_printer->setOutputFileName(pdfname);
    }
    w_print_preview->print();
    accept();
}

#include "moc_printdialog.cpp"
