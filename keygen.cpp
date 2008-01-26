#include <qstring.h>
#include <qfile.h>
#include <qdatastream.h>
#include <qtextstream.h>
#include <qcstring.h>

#include "sha1.cpp"

QString generateKey(const QString &name, const QString &privkey)
{
    if (name.isEmpty())
        return QString();

    QByteArray src;
    QDataStream ss(src, IO_WriteOnly);
    ss.setByteOrder(QDataStream::LittleEndian);
    ss << (privkey + name);

    QByteArray sha1 = sha1::calc(src.data() + 4, src.size() - 4);

    if (sha1.count() < 8)
        return QString();

    QString result;
    Q_UINT64 serial = 0;
    QDataStream ds(sha1, IO_ReadOnly);
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

int main(int argc, char **argv)
{
    if (argc >= 2) {
        QString name = argv [1];

        for (int i = 2; i < argc; i++) {
            name += " ";
            name += argv [i];
        }

        QFile f(".private-key");
        if (f.open(IO_ReadOnly)) {
            QString privkey;
            QTextStream ts(&f);
            ts >> privkey;

            QString regkey = generateKey(name, privkey);

            if (regkey.length() == 14) {
                printf("%s\n", regkey.latin1());

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
