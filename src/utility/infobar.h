#ifndef INFOBAR_H
#define INFOBAR_H

#include <QWidget>
#include <QScopedPointer>

class QAbstractButton;
class QLabel;
class InfoBarPrivate;

class InfoBar : public QWidget
{
    Q_OBJECT

public:
    InfoBar(QWidget *parent = 0);
    ~InfoBar();

    enum ButtonType {
        Cancel,
        Previous,
        Next,
        More,

        Count
    };

    int insertSection(int pos, const QString &title = QString());
    void removeSection(int section);

    QLabel *sectionTitel(int section);
    QAbstractButton *sectionButton(int section, ButtonType button);
    QWidget *sectionContent(int section) const;
    void setSectionContent(int section, QWidget *w);

private:
    QScopedPointer<InfoBarPrivate> d;

    Q_DISABLE_COPY(InfoBar)
};

#endif // INFOBAR_H
