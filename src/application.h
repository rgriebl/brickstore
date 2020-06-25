/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#pragma once

#include <QApplication>
#include <QStringList>

class FrameWork;
class QTranslator;


class Application : public QApplication
{
    Q_OBJECT
public:
    Application(bool rebuild_db_only, bool skip_download, int &argc, char **argv);
    ~Application() override;

    static Application *inst() { return s_inst; }

    void enableEmitOpenDocument(bool b = true);

    QString applicationUrl() const;

    bool pixmapAlphaSupported() const;

    bool isOnline() const;

    QStringList externalResourceSearchPath(const QString &subdir = QString()) const;

public slots:
    void about();
    void checkForUpdates();
    void updateTranslations();

signals:
    void openDocument(const QString &);
    void onlineStateChanged(bool isOnline);

protected:
    bool event(QEvent *e) override;

private slots:
    void doEmitOpenDocument();
    void clientMessage();
    void checkNetwork();

private:
    bool isClient(int timeout = 1000);

    bool initBrickLink();
    void exitBrickLink();

private:
    QStringList m_files_to_open;
    bool m_enable_emit;
    bool m_has_alpha;

    bool m_online;
    qreal m_default_fontsize;

    QTranslator *m_trans_qt;
    QTranslator *m_trans_brickstore;

    static Application *s_inst;
};
