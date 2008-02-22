#include <QString>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QByteArray>
#include <QCryptographicHash>
#include <QLineEdit>
#include <QLabel>
#include <QLayout>
#include <QApplication>
#include <QToolButton>
#include <QDialog>

QString privkey;


QString generateKey(const QString &name, const QString &privkey)
{
    if (name.isEmpty())
        return QString();

    QByteArray src;
    QDataStream ss(&src, QIODevice::WriteOnly);
    ss.setByteOrder(QDataStream::LittleEndian);
    ss << (privkey + name);
    

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(src.data() + 4, src.size() - 4);
    QByteArray sha1 = hash.result();

    if (sha1.count() < 8)
        return QString();

    QString result;
    quint64 serial = 0;
    QDataStream ds(&sha1, QIODevice::ReadOnly);
    ds >> serial;

    // 32bit without 0/O and 5/S
    const char *mapping = "12346789ABCDEFGHIJKLMNPQRTUVWXYZ";

    // get 12*5 = 60 bits
    for (int i = 12; i; i--) {
        result.append(mapping [serial & 0x1f]);
        serial >>= 5;
    }
    result.insert(8, '-');
    result.insert(4, '-');

    return result;
}

class KeyWidget : public QDialog {
    Q_OBJECT
public:
    KeyWidget() : QDialog()
    { 
        setWindowTitle(tr("BrickStore KeyGen"));
        QGridLayout *grid = new QGridLayout(this);
        grid->addWidget(new QLabel(tr("Name"), this), 0, 0);
        grid->addWidget(new QLabel(tr("Key"), this), 1, 0);
        m_key = new QLineEdit(this);
        m_key->setReadOnly(true); 
        grid->addWidget(m_key, 1, 1);
        m_name = new QLineEdit(this);
        QObject::connect(m_name, SIGNAL(textChanged(const QString &)), this, SLOT(updateKey(const QString &)));
        grid->addWidget(m_name, 0, 1);
        QToolButton *paste = new QToolButton(this);
        paste->setIcon(QIcon(":/images/paste"));
        paste->setToolTip(tr("Paste Name from Clipboard"));
        paste->setAutoRaise(true);
        QObject::connect(paste, SIGNAL(clicked()), this, SLOT(clearAndPaste()));
        grid->addWidget(paste, 0, 2);
        QToolButton *copy = new QToolButton(this);
        copy->setIcon(QIcon(":/images/copy"));
        copy->setToolTip(tr("Copy Key to Clipboard"));
        copy->setAutoRaise(true);
        QObject::connect(copy, SIGNAL(clicked()), this, SLOT(selectAndCopy()));
        grid->addWidget(copy, 1, 2);
    }
public slots:
    void updateKey(const QString &name)
    {
        m_key->setText(generateKey(name, privkey));
    }
    
    void selectAndCopy()
    {
        m_key->selectAll(); m_key->copy(); m_key->deselect();
    }

    void clearAndPaste()
    {
        m_name->clear(); m_name->paste();
    }
private:
    QLineEdit *m_name, *m_key;    
};

int gui_main(int argc, char **argv)
{
    QApplication a(argc, argv);
    (new KeyWidget())->show();
    return a.exec();
}

int main(int argc, char **argv)
{
    if (argc >= 2) {
        QString name = argv [1];

        for (int i = 2; i < argc; i++) {
            name += " ";
            name += argv [i];
        }

        QFile f(".private-key");
        if (f.open(QIODevice::ReadOnly)) {
            QTextStream ts(&f);
            ts >> privkey;

            if (name == "--gui")
                return gui_main(argc, argv);

            QString regkey = generateKey(name, privkey);

            if (regkey.length() == 14) {
                printf("%s\n", qPrintable(regkey));

                return 0;
            }
            else
                printf("Could not generate key.\n");
        }
        else
            printf("Could not read .private-key\n");
    }
    else
        printf("Usage: %s <name>\n", argv[0]);
    return 1;
}

#include "keygen.moc"

