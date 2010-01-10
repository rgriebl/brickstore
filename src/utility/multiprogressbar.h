/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __MULTIPROGRESSBAR_H__
#define __MULTIPROGRESSBAR_H__

#include <QWidget>
#include <QMap>
#include <QIcon>

class QProgressBar;
class QToolButton;


class MultiProgressBar : public QWidget {
    Q_OBJECT
public:
    MultiProgressBar(QWidget *parent = 0);
    virtual ~MultiProgressBar();

    void setStopIcon(const QIcon &);

    int addItem(const QString &label, int id = -1);
    void removeItem(int id);

    QString itemLabel(int id) const;

    int itemProgress(int id) const;
    int itemTotalSteps(int id) const;

public slots:
    void setItemProgress(int id, int progress);
    void setItemProgress(int id, int progress, int total);
    void setItemTotalSteps(int id, int total);

    void setItemLabel(int id, const QString &label);

signals:
    void stop();
    void statusChange(bool);

protected:
    void resizeEvent(QResizeEvent *e);

protected slots:
    void languageChange();

private:
    void recalc();

private:
    int m_autoid;

    QProgressBar *m_progress;
    QToolButton *m_stop;

    QIcon m_stop_ico;

    struct ItemData {
        ItemData(const QString &label);

        QString m_label;
        int     m_progress;
        int     m_total;
    };

    QMap<int, ItemData> m_items;
};

#endif
