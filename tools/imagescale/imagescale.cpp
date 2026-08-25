// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

// Scales one source image to a set of square icons, either as separate PNGs:
//
//     imagescale [options] <source> <out-dir> <size>:<name> [<size>:<name> ...]
//
// writing <out-dir>/<name>.png for every argument, or packed into one Windows
// icon container:
//
//     imagescale --ico <file> [options] <source> <size> [<size> ...]
//
// The source is decoded once either way.
//
// This exists so that the generated app icons do not have to be committed, and
// so that generating them needs no image toolchain on the build machine: neither
// ImageMagick on Windows nor makeicns on macOS, while a Qt installation is a
// given everywhere. QImage and QPainter are all this needs.
//
// .icns is the one container still left to ImageMagick: Qt writes a single image
// into it and its ICNS handler is documented as experimental.

#include <QBuffer>
#include <QColor>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QPainter>

#include <cstdio>

using namespace Qt::StringLiterals;


static QImage renderIcon(const QImage &source, int size, int inset, const QColor &background)
{
    const int content = qRound(size * (100 - 2 * inset) / 100.);

    QImage scaled = source.scaled(content, content, Qt::IgnoreAspectRatio,
                                  Qt::SmoothTransformation);
    if ((content == size) && !background.isValid())
        return scaled;

    QImage result(size, size, QImage::Format_ARGB32_Premultiplied);
    result.fill(background.isValid() ? background : QColor(Qt::transparent));

    QPainter p(&result);
    p.drawImage((size - content) / 2, (size - content) / 2, scaled);
    p.end();

    return result;
}

// A modern .ico is just the PNGs concatenated behind a directory of fixed size
// records. Storing every entry as PNG needs Vista or newer to display and VS2008
// or newer to compile into a resource - both far below what BrickStore requires.
static bool writeIco(const QString &fileName, const QList<QByteArray> &icons)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        std::fprintf(stderr, "Cannot write %s: %s\n", qPrintable(fileName),
                     qPrintable(file.errorString()));
        return false;
    }

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << quint16(0)                    // reserved
       << quint16(1)                    // 1 is an icon, 2 would be a cursor
       << quint16(icons.size());

    // the images follow the directory, which is 6 bytes plus 16 per entry
    quint32 offset = 6 + 16 * quint32(icons.size());

    for (const QByteArray &icon : icons) {
        QImage image = QImage::fromData(icon, "png");
        const quint8 extent = (image.width() >= 256) ? 0 : quint8(image.width()); // 0 means 256

        ds << extent << extent
           << quint8(0)                 // palette size, 0 when there is no palette
           << quint8(0)                 // reserved
           << quint16(1)                // color planes
           << quint16(32)               // bits per pixel
           << quint32(icon.size())
           << offset;

        offset += quint32(icon.size());
    }
    for (const QByteArray &icon : icons)
        ds.writeRawData(icon.constData(), int(icon.size()));

    file.close();

    if (file.error() != QFileDevice::NoError) {
        std::fprintf(stderr, "Cannot write %s: %s\n", qPrintable(fileName),
                     qPrintable(file.errorString()));
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"imagescale"_s);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Scales an image to a set of square PNGs."_s);
    parser.addHelpOption();
    parser.addPositionalArgument(u"source"_s, u"The image to scale down."_s);
    parser.addPositionalArgument(u"out-dir"_s, u"Where to write the PNGs."_s);
    parser.addPositionalArgument(u"size:name"_s, u"Pixel size and base name, repeatable."_s,
                                 u"<size>:<name>..."_s);

    QCommandLineOption backgroundOpt(u"background"_s,
                                     u"Fill an opaque background, instead of keeping the alpha "
                                     "channel."_s, u"color"_s);
    QCommandLineOption insetOpt(u"inset"_s,
                                u"Keep this much of each edge free, in percent of the target "
                                "size."_s, u"percent"_s, u"0"_s);
    QCommandLineOption upscaleOpt(u"allow-upscale"_s,
                                  u"Scale beyond the source resolution instead of failing."_s);
    QCommandLineOption icoOpt(u"ico"_s,
                              u"Pack the sizes into this Windows icon file instead of writing "
                              "PNGs. The positional arguments are <source> <size>... then."_s,
                              u"file"_s);
    parser.addOptions({ backgroundOpt, insetOpt, upscaleOpt, icoOpt });
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    const bool ico = parser.isSet(icoOpt);

    if (args.size() < (ico ? 2 : 3)) {
        parser.showHelp(1);
        return 1;
    }

    QColor background;
    if (parser.isSet(backgroundOpt)) {
        background = QColor::fromString(parser.value(backgroundOpt));
        if (!background.isValid()) {
            std::fprintf(stderr, "Not a valid color: %s\n", qPrintable(parser.value(backgroundOpt)));
            return 1;
        }
    }
    bool ok = false;
    const int inset = parser.value(insetOpt).toInt(&ok);
    if (!ok || (inset < 0) || (inset >= 50)) {
        std::fprintf(stderr, "Not a valid inset: %s\n", qPrintable(parser.value(insetOpt)));
        return 1;
    }

    QImage source(args.at(0));
    if (source.isNull()) {
        std::fprintf(stderr, "Cannot read %s\n", qPrintable(args.at(0)));
        return 1;
    }

    // QImage::scaled only takes the area-averaging path (smoothScaled) for the
    // premultiplied formats. Everything else is painted with a bilinear filter,
    // unless the image happens to shrink by more than 2x - which would alias the
    // bigger sizes and not the small ones. Converting up front makes all sizes
    // take the same, correct path.
    source.convertTo(QImage::Format_ARGB32_Premultiplied);

    // in .ico mode the out-dir argument is absent and the names are, too
    const QDir outDir(ico ? QString { } : args.at(1));
    const int maxSize = qMin(source.width(), source.height());
    QList<QByteArray> icons;

    for (qsizetype i = (ico ? 1 : 2); i < args.size(); ++i) {
        const QString arg = args.at(i);
        const qsizetype colon = ico ? arg.size() : arg.indexOf(u':');

        if (colon < 0) {
            std::fprintf(stderr, "Not a <size>:<name> argument: %s\n", qPrintable(arg));
            return 1;
        }
        const int size = arg.left(colon).toInt(&ok);

        if (!ok || (size <= 0)) {
            std::fprintf(stderr, "Not a valid size: %s\n", qPrintable(arg));
            return 1;
        }
        if (ico && (size > 256)) {
            std::fprintf(stderr, "An icon entry cannot be larger than 256x256: %s\n",
                         qPrintable(arg));
            return 1;
        }
        if ((size > maxSize) && !parser.isSet(upscaleOpt)) {
            std::fprintf(stderr, "Refusing to upscale %s (%dx%d) to %dx%d\n",
                         qPrintable(args.at(0)), source.width(), source.height(), size, size);
            return 1;
        }

        QImage icon = renderIcon(source, size, inset, background);
        // RGB32 saves as a PNG without an alpha channel, ARGB32 as an 8bit one:
        // either way this drops the 16bit depth of the document icon master
        icon.convertTo(background.isValid() ? QImage::Format_RGB32 : QImage::Format_ARGB32);

        if (ico) {
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);

            QImageWriter writer(&buffer, "png");
            if (!writer.write(icon)) {
                std::fprintf(stderr, "Cannot encode the %dx%d entry: %s\n", size, size,
                             qPrintable(writer.errorString()));
                return 1;
            }
            icons.append(png);
            continue;
        }

        const QString fileName = outDir.filePath(arg.mid(colon + 1) + u".png"_s);
        QImageWriter writer(fileName, "png");
        if (!writer.write(icon)) {
            std::fprintf(stderr, "Cannot write %s: %s\n", qPrintable(fileName),
                         qPrintable(writer.errorString()));
            return 1;
        }
    }

    if (ico && !writeIco(parser.value(icoOpt), icons))
        return 1;

    return 0;
}
