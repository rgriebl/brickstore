#include <QApplication>

#include <stdio.h>
#include "ldraw.h"
#include "ldrawrenderer.h"

int main(int argc, char **argv)
{
    QString error;
    if (!LDraw::create(QString(), &error)) {
        printf("Startup error: %s\n", qPrintable(error));
        return 2;
    }

/*    if (argc == 2) {
        QString f = QLatin1String(argv[1]);

        LDraw::Model *m = LDraw::Model::fromFile(f);
        if (m && m->root()) {
            m->root()->dump();
            delete m;
        }
        else
            printf("Unable to parse file %s\n", argv[1]);
    }
    else if (argc == 1 || (argc == 3 && !strcmp(argv[1], "-gui"))) {
  */      QApplication a(argc, argv);
        
        const char *fn = (argc >= 2 ? argv[1] : "/Users/sandman/work/brickstore4/stuff/logo.ldr");

        LDraw::Model *m = LDraw::Model::fromFile(QLatin1String(fn));
        if (m && m->root()) {
//            m->root()->dump();

            LDraw::RenderWidget *w = new LDraw::RenderWidget();
            w->setPartAndColor(m->root(), 4 /*red*/);
            
            w->show();
        }
        else
            printf("Unable to parse file %s\n", fn);
        
        return a.exec();
 /*   }
    else
        printf("Usage: %s <file>\n", argv[0]);
  */      
    return 0;
}

