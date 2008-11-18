
#include <QApplication>
#include <QSvgGenerator>
#include <QPicture>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>


int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QString fontname = QLatin1String("Mad Scientist");
    int fontsize = 24;
    QString logotext = QLatin1String("Brickstore");
    QString basename = QLatin1String("logo-text");

    {
        QSvgGenerator svg;
        svg.setFileName(basename + QLatin1String(".svg"));
        QPainter paint(&svg);
        QPainterPath path;
        QFont font(fontname, fontsize);
        QFontMetrics fm(font);
        path.addText(1, fm.ascent(), font, logotext);
        paint.setPen(Qt::black);
        paint.fillPath(path, Qt::black);
        paint.end();
    }
    {
        QPicture pic;
        QPainter paint(&pic);
        QPainterPath path;
        QFont font(fontname, fontsize);
        QFontMetrics fm(font);
        QRect r = fm.tightBoundingRect(logotext);
        path.addText(1, -r.top() - fm.ascent(), font, logotext);
        paint.setPen(Qt::black);
        paint.fillPath(path, Qt::black);
        paint.end();
        pic.save(basename + QLatin1String(".pic"));
    }
    return 0;
}
