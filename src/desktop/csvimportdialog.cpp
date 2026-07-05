// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTableView>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QFile>
#include <QFileInfo>
#include <QLocale>

#include "utility/csvtokenizer.h"
#include "csvimportdialog.h"

using namespace Qt::StringLiterals;


///////////////////////////////////////////////////////////////////////
// CsvPreviewModel
///////////////////////////////////////////////////////////////////////


void CsvPreviewModel::setRows(const QList<QStringList> &rows)
{
    beginResetModel();
    m_rows = rows;
    m_columns = 0;
    for (const QStringList &row : m_rows)
        m_columns = qMax(m_columns, int(row.size()));

    // keep the current mapping for the columns that still exist; new ones default to Ignore
    m_mapping.resize(m_columns, CsvImport::Field::Ignore);
    endResetModel();
}

void CsvPreviewModel::setMapping(const QList<CsvImport::Field> &mapping)
{
    m_mapping = mapping;
    m_mapping.resize(m_columns, CsvImport::Field::Ignore);   // pad/truncate to the column count
    emit headerDataChanged(Qt::Horizontal, 0, qMax(0, m_columns - 1));
}

void CsvPreviewModel::setFieldForColumn(int column, CsvImport::Field field)
{
    if (column < 0 || column >= m_mapping.size())
        return;
    m_mapping[column] = field;
    emit headerDataChanged(Qt::Horizontal, column, column);
}

int CsvPreviewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int CsvPreviewModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_columns;
}

QVariant CsvPreviewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
        return { };
    const QStringList &row = m_rows.at(index.row());
    return (index.column() < row.size()) ? row.at(index.column()) : QString();
}

QVariant CsvPreviewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return { };
    if (orientation == Qt::Horizontal)
        return (section < m_mapping.size()) ? CsvImport::displayName(m_mapping.at(section)) : QString();
    return section + 1;
}


///////////////////////////////////////////////////////////////////////
// CsvFieldHeader
///////////////////////////////////////////////////////////////////////


CsvFieldHeader::CsvFieldHeader(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
    setSectionsClickable(true);
    setSectionResizeMode(QHeaderView::Interactive);
    setHighlightSections(true);
}

void CsvFieldHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid())
        return;

    QHeaderView::paintSection(painter, rect, logicalIndex);   // background + field name

    QStyleOption opt;
    opt.initFrom(this);
    int arrow = style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &opt, this);
    if (arrow <= 0)
        arrow = 12;
    opt.rect = QRect(rect.right() - arrow - 4, rect.center().y() - arrow / 2, arrow, arrow);
    opt.state |= QStyle::State_Enabled;
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &opt, painter, this);
}


///////////////////////////////////////////////////////////////////////
// CsvImportDialog
///////////////////////////////////////////////////////////////////////


CsvImportDialog::CsvImportDialog(const QString &fileName, QWidget *parent)
    : QDialog(parent)
    , m_fileName(fileName)
{
    setWindowTitle(tr("Import CSV — %1").arg(QFileInfo(fileName).fileName()));

    QFile f(fileName);
    if (f.open(QIODevice::ReadOnly))
        m_bytes = f.readAll();

    m_encoding = new QComboBox(this);
    m_encoding->addItem(tr("Automatic"), QVariant());
    m_encoding->addItem(u"UTF-8"_s, int(QStringConverter::Utf8));
    m_encoding->addItem(u"UTF-16"_s, int(QStringConverter::Utf16));
    m_encoding->addItem(u"ISO-8859-1 (Latin-1)"_s, int(QStringConverter::Latin1));
    m_encoding->addItem(tr("System"), int(QStringConverter::System));

    auto *sepWidget = new QWidget(this);
    auto *sepLayout = new QHBoxLayout(sepWidget);
    sepLayout->setContentsMargins(0, 0, 0, 0);
    m_separator = new QButtonGroup(this);
    const auto addSeparator = [&](const QString &text, QChar ch, bool checked) {
        auto *rb = new QRadioButton(text, sepWidget);
        rb->setChecked(checked);
        m_separator->addButton(rb, int(ch.unicode()));
        sepLayout->addWidget(rb);
    };
    addSeparator(tr("Comma"), u',', true);
    addSeparator(tr("Semicolon"), u';', false);
    addSeparator(tr("Tab"), u'\t', false);
    sepLayout->addStretch(1);

    m_numberFormat = new QComboBox(this);
    m_numberFormat->addItem(tr("System"), QVariant::fromValue(QLocale::system()));
    m_numberFormat->addItem(tr("1,234.56 (point)"), QVariant::fromValue(QLocale::c()));
    m_numberFormat->addItem(tr("1.234,56 (comma)"), QVariant::fromValue(QLocale(QLocale::German)));

    m_firstRowHeader = new QCheckBox(tr("First row contains column names"), this);
    m_firstRowHeader->setChecked(true);

    auto *form = new QFormLayout;
    form->addRow(tr("Character set:"), m_encoding);
    form->addRow(tr("Separator:"), sepWidget);
    form->addRow(tr("Number format:"), m_numberFormat);
    form->addRow(QString(), m_firstRowHeader);

    auto *optionsGroup = new QGroupBox(tr("Import options"), this);
    optionsGroup->setLayout(form);

    m_model = new CsvPreviewModel(this);
    m_preview = new QTableView(this);
    m_preview->setModel(m_model);
    m_preview->setHorizontalHeader(new CsvFieldHeader(m_preview));
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->setSelectionMode(QAbstractItemView::NoSelection);
    m_preview->setFocusPolicy(Qt::NoFocus); // prevent the current cell highlight
    m_preview->setWordWrap(false);
    m_preview->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *top = new QVBoxLayout(this);
    top->addWidget(optionsGroup);
    top->addWidget(new QLabel(tr("Click a column header to choose which field it maps to:"), this));
    top->addWidget(m_preview, 1);
    top->addWidget(m_buttons);

    connect(m_encoding, &QComboBox::currentIndexChanged, this, [this] { reparse(false); });
    connect(m_separator, &QButtonGroup::idClicked, this, [this] { reparse(false); });
    connect(m_firstRowHeader, &QCheckBox::toggled, this, [this] { reparse(true); });
    connect(m_preview->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &CsvImportDialog::pickField);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(760, 520);
    reparse(true);
}

QString CsvImportDialog::fileName() const
{
    return m_fileName;
}

QList<CsvImport::Field> CsvImportDialog::mapping() const
{
    return m_model->mapping();
}

QChar CsvImportDialog::delimiter() const
{
    const int id = m_separator->checkedId();
    return (id >= 0) ? QChar(char16_t(id)) : u',';
}

QChar CsvImportDialog::quote() const
{
    return u'"';
}

std::optional<QStringConverter::Encoding> CsvImportDialog::encoding() const
{
    const QVariant v = m_encoding->currentData();
    if (!v.isValid())
        return { };
    return QStringConverter::Encoding(v.toInt());
}

CsvImport::Options CsvImportDialog::options() const
{
    CsvImport::Options opt;
    opt.locale = m_numberFormat->currentData().value<QLocale>();
    opt.firstRowIsHeader = m_firstRowHeader->isChecked();
    return opt;
}

void CsvImportDialog::reparse(bool guess)
{
    const auto enc = encoding().value_or(QStringConverter::encodingForData(m_bytes)
                                             .value_or(QStringConverter::Utf8));
    const QString text = QStringDecoder(enc).decode(m_bytes);
    const CsvTokenizer::ParseResult tokens = CsvTokenizer::tokenize(text, delimiter(), quote(), 20);

    m_model->setRows(tokens.rows);
    if (guess && m_firstRowHeader->isChecked() && !tokens.rows.isEmpty())
        m_model->setMapping(guessMapping(tokens.rows.first()));

    m_preview->resizeColumnsToContents();
    updateOkState();
}

void CsvImportDialog::pickField(int column)
{
    QMenu menu(this);
    for (CsvImport::Field field : CsvImport::allFields()) {
        QAction *a = menu.addAction(CsvImport::displayName(field));
        a->setData(int(field));
    }
    auto *hdr = m_preview->horizontalHeader();
    const QPoint pos = hdr->mapToGlobal(QPoint(hdr->sectionViewportPosition(column), hdr->height()));
    if (QAction *chosen = menu.exec(pos)) {
        m_model->setFieldForColumn(column, CsvImport::Field(chosen->data().toInt()));
        updateOkState();
    }
}

void CsvImportDialog::updateOkState()
{
    const auto mapping = m_model->mapping();
    const bool hasIdentity = mapping.contains(CsvImport::Field::ItemId)
                             || mapping.contains(CsvImport::Field::Pcc);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(hasIdentity);
}

QList<CsvImport::Field> CsvImportDialog::guessMapping(const QStringList &headerRow) const
{
    using F = CsvImport::Field;
    QList<F> mapping;
    mapping.reserve(headerRow.size());
    for (const QString &cell : headerRow) {
        const QString s = cell.trimmed().toLower();
        F f = F::Ignore;
        if (s.contains(u"element") || s.contains(u"pcc"))           f = F::Pcc;
        else if (s.contains(u"type"))                               f = F::ItemType;
        else if (s.contains(u"colour") || s.contains(u"color"))
            f = (s.contains(u"id") || s.contains(u"code")) ? F::ColorId : F::ColorName;
        else if (s.contains(u"part") || s.contains(u"item") || s.contains(u"design"))
            f = F::ItemId;
        else if (s.contains(u"cond"))                               f = F::Condition;
        else if (s.contains(u"qty") || s.contains(u"quant")
                 || s.contains(u"amount") || s.contains(u"count"))  f = F::Quantity;
        else if (s.contains(u"price"))                              f = F::Price;
        else if (s.contains(u"cost"))                               f = F::Cost;
        else if (s.contains(u"bulk"))                               f = F::Bulk;
        else if (s.contains(u"sale"))                               f = F::Sale;
        else if (s.contains(u"comment"))                            f = F::Comments;
        else if (s.contains(u"remark") || s.contains(u"note"))      f = F::Remarks;
        mapping.append(f);
    }
    return mapping;
}
