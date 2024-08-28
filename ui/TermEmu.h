#pragma once


#include <QWidget>

#include "vterm.h"


class TermEmu : public QWidget {
public:
    TermEmu();

    VTerm *term{};
    VTermScreen *screen{};
    VTermState *state{};

    int term_w = 80;
    int term_h = 24;

protected:
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void paintEvent(QPaintEvent *event);


};

