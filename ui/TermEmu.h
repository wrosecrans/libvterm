#pragma once


#include <QWidget>
#include <QProcess>
#include <QTimer>

#include "vterm.h"


class TermEmuWinPty;

class TermEmu : public QWidget {
public:
    TermEmu();
    ~TermEmu();

    void run(std::string_view program);
    void write(std::string_view text);

    VTerm *term{};
    VTermScreen *screen{};
    VTermState *state{};

    int term_w = 80;
    int term_h = 24;

    bool draw_cursor = true;

    QProcess *P = nullptr;
    TermEmuWinPty *Wpty = nullptr;

    QTimer timer;

protected:
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void paintEvent(QPaintEvent *event);


};

