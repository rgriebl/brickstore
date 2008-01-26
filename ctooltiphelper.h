
#include <QWidget>
#include <QApplication>
#include <QEvent>

class CToolTipHelper : public QObject {
    Q_OBJECT

public:
    CToolTipHelper()
        : QObject(0), m_active_tip(0)
    {
        qApp->installEventFilter(this);
    }

    ~CToolTipHelper()
    {
        qApp->removeEventFilter(this);
    }

signals:
    void tipStateChanged(bool);

protected:
    virtual bool eventFilter(QObject *o, QEvent *e)
    {
        if ((e->type() == QEvent::Show || e->type() == QEvent::Hide) &&
            (o->isWidgetType() && (static_cast<QWidget *>(o)->windowType() == Qt::ToolTip) && o->inherits("QTipLabel"))) {
            QObject *oldtip = m_active_tip;

            if (e->type() == QEvent::Show)
                m_active_tip = o;
            else if (m_active_tip == o)
                m_active_tip = 0;

            if (m_active_tip != oldtip)
                emit tipStateChanged(m_active_tip != 0);
        }

        return false;
    }

private:
    QObject *m_active_tip;
};
