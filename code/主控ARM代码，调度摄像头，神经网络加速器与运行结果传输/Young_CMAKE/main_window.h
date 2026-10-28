#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QtWidgets>
#include "catch_image.h"
#include "v4l2_helper.h"

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

public Q_SLOTS:
    void updatePic0(QImage *pImg);
    void updatePic1(QImage *pImg);
    void play_change(bool);

private:
    catchImage *pCatchVideo0;
    catchImage *pCatchVideo1;
    QLabel *pVideo0;
    QLabel *pVideo1;
    run_elf *run_elf_p;
    v4l2_dev *dev;
};

#endif // MAIN_WINDOW_H
