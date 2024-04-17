// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QApplication>
#include <QMessageBox>
#include <QTextDocument>
#include <QByteArray>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QDoubleSpinBox>
#include <QScreen>
#include <QWindow>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDebug>

#include <QCoro/QCoroSignal>

#include "desktopuihelpers.h"
#include "desktopuihelpers_p.h"


QPointer<QWidget> DesktopUIHelpers::s_defaultParent;

void DesktopUIHelpers::create()
{
    s_inst = new DesktopUIHelpers();
}

void DesktopUIHelpers::setDefaultParent(QWidget *defaultParent)
{
    s_defaultParent = defaultParent;
}

void DesktopUIHelpers::setPopupPos(QWidget *w, const QRect &pos)
{
    QSize sh = w->sizeHint();
    QRect screenRect = w->window()->windowHandle()->screen()->availableGeometry();

    // center below pos
    int x = pos.x() + (pos.width() - sh.width()) / 2;
    int y = pos.y() + pos.height();

    if ((y + sh.height()) > (screenRect.bottom() + 1)) {
        int frameHeight = (w->frameSize().height() - w->size().height());
        y = pos.y() - sh.height() - frameHeight;
    }
    if (y < screenRect.top())
        y = screenRect.top();

    if ((x + sh.width()) > (screenRect.right() + 1))
        x = (screenRect.right() + 1) - sh.width();
    if (x < screenRect.left())
        x = screenRect.left();

    w->move(x, y);
    w->resize(sh);
}

EventFilter::Result DesktopUIHelpers::selectAllFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::FocusIn) {
        auto *fe = static_cast<QFocusEvent *>(e);
        static const QVector<Qt::FocusReason> validReasons = {
            Qt::MouseFocusReason,
            Qt::TabFocusReason,
            Qt::BacktabFocusReason,
            Qt::ActiveWindowFocusReason,
            Qt::ShortcutFocusReason
        };
        if (validReasons.contains(fe->reason())) {
            if (auto *le = qobject_cast<QLineEdit *>(o))
                QMetaObject::invokeMethod(le, &QLineEdit::selectAll, Qt::QueuedConnection);
            else if (auto *cb = qobject_cast<QComboBox *>(o))
                QMetaObject::invokeMethod(cb->lineEdit(), &QLineEdit::selectAll, Qt::QueuedConnection);
            else if (auto *sb = qobject_cast<QAbstractSpinBox *>(o))
                QMetaObject::invokeMethod(sb, &QAbstractSpinBox::selectAll, Qt::QueuedConnection);
        }
    }
    return EventFilter::ContinueEventProcessing;
}


QCoro::Task<UIHelpers::StandardButton> DesktopUIHelpers::showMessageBox(QString msg,
                                                                        UIHelpers::Icon icon,
                                                                        StandardButtons buttons,
                                                                        StandardButton defaultButton,
                                                                        QString title)
{
    if (qobject_cast<QApplication *>(qApp)) {
        QMessageBox mb(QMessageBox::Icon(icon), !title.isEmpty() ? title : defaultTitle(), msg,
                       QMessageBox::NoButton, s_defaultParent);
        mb.setObjectName(u"messagebox"_qs);
        mb.setStandardButtons(QMessageBox::StandardButton(int(buttons)));
        mb.setDefaultButton(QMessageBox::StandardButton(defaultButton));
        mb.setTextFormat(Qt::RichText);
        mb.setWindowModality(Qt::ApplicationModal);

        mb.show();

        int result = co_await qCoro(&mb, &QDialog::finished);
        co_return static_cast<StandardButton>(result);

    } else {
        QByteArray out;
        if (msg.contains(u'<')) {
            QTextDocument doc;
            doc.setHtml(msg);
            out = doc.toPlainText().toLocal8Bit();
        } else {
            out = msg.toLocal8Bit();
        }

        if (icon == Critical)
            qCritical("%s", out.constData());
        else
            qWarning("%s", out.constData());

        co_return defaultButton;
    }
}

QCoro::Task<std::optional<QString>> DesktopUIHelpers::getInputString(QString text,
                                                                     QString initialValue,
                                                                     bool isPassword,
                                                                     QString title)
{
    QInputDialog dlg(s_defaultParent, Qt::Sheet);
    dlg.setWindowTitle(!title.isEmpty() ? title : defaultTitle());
    dlg.setWindowModality(Qt::ApplicationModal);
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setLabelText(text);
    dlg.setTextValue(initialValue);
    if (isPassword)
        dlg.setTextEchoMode(QLineEdit::Password);

    dlg.show();

    if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted)
        co_return dlg.textValue();
    else
        co_return { };
}

QCoro::Task<std::optional<double>> DesktopUIHelpers::getInputDouble(QString text,
                                                                    QString unit,
                                                                    double initialValue,
                                                                    double minValue, double maxValue,
                                                                    int decimals, QString title)
{
    QInputDialog dlg(s_defaultParent, Qt::Sheet);
    dlg.setWindowTitle(!title.isEmpty() ? title : defaultTitle());
    dlg.setWindowModality(Qt::ApplicationModal);
    dlg.setInputMode(QInputDialog::DoubleInput);
    dlg.setLabelText(text);
    dlg.setDoubleValue(initialValue);
    dlg.setDoubleMinimum(minValue);
    dlg.setDoubleMaximum(maxValue);
    dlg.setDoubleDecimals(decimals);

    if (auto *sp = dlg.findChild<QDoubleSpinBox *>()) {
        if (!unit.isEmpty())
            sp->setSuffix(u' ' + unit);
        sp->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sp->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
        sp->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        sp->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    }

    dlg.show();

    if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted)
        co_return dlg.doubleValue();
    else
        co_return { };
}

QCoro::Task<std::optional<int>> DesktopUIHelpers::getInputInteger(QString text,
                                                                  QString unit,
                                                                  int initialValue, int minValue,
                                                                  int maxValue, QString title)
{
    QInputDialog dlg(s_defaultParent, Qt::Sheet);
    dlg.setWindowTitle(!title.isEmpty() ? title : defaultTitle());
    dlg.setWindowModality(Qt::ApplicationModal);
    dlg.setInputMode(QInputDialog::IntInput);
    dlg.setLabelText(text);
    dlg.setIntValue(initialValue);
    dlg.setIntMinimum(minValue);
    dlg.setIntMaximum(maxValue);

    if (auto *sp = dlg.findChild<QSpinBox *>()) {
        if (!unit.isEmpty())
            sp->setSuffix(u' ' + unit);
        sp->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sp->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
        sp->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        sp->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    }

    dlg.show();

    if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted)
        co_return dlg.intValue();
    else
        co_return { };
}

QCoro::Task<std::optional<QColor>> DesktopUIHelpers::getInputColor(QColor initialColor,
                                                                   QString title)
{
    QColorDialog dlg(s_defaultParent);
    dlg.setWindowFlag(Qt::Sheet);
    dlg.setWindowTitle(!title.isEmpty() ? title : defaultTitle());
    dlg.setWindowModality(Qt::ApplicationModal);
    if (initialColor.isValid())
        dlg.setCurrentColor(initialColor);

    dlg.show();

    if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted)
        co_return dlg.selectedColor();
    else
        co_return { };
}

QCoro::Task<std::optional<QString>> DesktopUIHelpers::getFileName(bool doSave, QString fileName,
                                                                  QStringList filters,
                                                                  QString title)
{
    QFileDialog fd(s_defaultParent, !title.isEmpty() ? title : defaultTitle(), fileName,
                   filters.join(u";;"));
    fd.setFileMode(doSave ? QFileDialog::AnyFile : QFileDialog::ExistingFile);
    fd.setAcceptMode(doSave ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
#if defined(Q_OS_MACOS)  // QTBUG-42516
    fd.setWindowModality(Qt::WindowModal);
#else
    fd.setWindowModality(Qt::ApplicationModal);
#endif

    fd.show();
    if (co_await qCoro(&fd, &QFileDialog::finished) == QDialog::Accepted) {
        auto files = fd.selectedFiles();
        if (!files.isEmpty())
            co_return files.constFirst();
    }
    co_return { };
}

UIHelpers_ProgressDialogInterface *DesktopUIHelpers::createProgressDialog(const QString &title, const QString &message)
{
    return new DesktopPDI(title, message, DesktopUIHelpers::s_defaultParent);
}

void DesktopUIHelpers::processToastMessages()
{
    if (m_toastMessageVisible || m_toastMessages.isEmpty())
        return;

    auto *activeWindow = qApp->activeWindow();
    if ((qApp->applicationState() != Qt::ApplicationActive) || !activeWindow) {
        m_toastMessages.clear();
        return;
    }

    auto [message, timeout] = m_toastMessages.takeFirst();
    auto toast = new ToastMessage(message, timeout);
    toast->open(activeWindow->frameGeometry());
    connect(toast, &ToastMessage::closed,
            this, [this, toast]() {
        toast->deleteLater();
        m_toastMessageVisible = false;
        processToastMessages();
    }, Qt::QueuedConnection);
}

#include "moc_desktopuihelpers.cpp"
#include "moc_desktopuihelpers_p.cpp"
