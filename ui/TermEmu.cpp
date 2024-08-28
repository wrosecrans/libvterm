#include "TermEmu.h"

#include <QKeyEvent>
#include <QPainter>
#include <iostream>
#include <format>
// int (*resize)(int rows, int cols, VTermStateFields *fields, void *user)
int screen_resize(int rows, int cols, void *user) {
    std::cout << "Resize, \n" << std::endl;

    auto emu = (TermEmu*) user;
    if(emu) {
        emu->term_w = cols;
        emu->term_h = rows;
    }
    return 1;
}
int screen_sb_pushline(int cols, const VTermScreenCell *cells, void *user) {

    std::cout << "Pushline, \n" << std::endl;
    return 1;
}


int damage(VTermRect rect, void *user);
int moverect(VTermRect dest, VTermRect src, void *user);
int movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user);
int settermprop(VTermProp prop, VTermValue *val, void *user);
int bell(void *user);
int resize(int rows, int cols, void *user);
int sb_pushline(int cols, const VTermScreenCell *cells, void *user);
int sb_popline(int cols, VTermScreenCell *cells, void *user);
int sb_clear(void* user);
/* ABI-compat this is only used if vterm_screen_callbacks_has_pushline4() is called */
// int (*sb_pushline4)(int cols, const VTermScreenCell *cells, bool continuation, void *user);

static VTermScreenCallbacks cb_screen = {
    .resize      = &screen_resize,
    .sb_pushline = &screen_sb_pushline,
};

static VTermStateCallbacks cb_state = {};


TermEmu::TermEmu()
{
    // int rows = 25;  int cols = 80;
    term = vterm_new(term_h, term_w);
    vterm_set_utf8(term, true);

    screen = vterm_obtain_screen(term);
    auto state = vterm_obtain_state(term);
    vterm_screen_set_callbacks(screen, &cb_screen, this);
    // vterm_state_set_callbacks(state, &cb_state, this);
    /* vterm_output_set_callback(term, [](const char *s, size_t len, void *user) {
            std::cout << std::format("Output {}\n{}\n", len, s);
            auto self = (TermEmu*)user;
            self->update();
        }, this);
*/
    vterm_screen_reset(screen, 1);
    std::string msg = "Screen Startup\n";
    vterm_input_write(term, msg.c_str(), msg.size());

}

void TermEmu::keyPressEvent(QKeyEvent *event)
{

    std::cout << "1 TermEmu::keyPressEvent(), 'No Format' " << std::endl;


    auto text = event->text();

    auto k = VTERM_KEY_NONE;

    std::cout << std::format("2 TermEmu::keyPressEvent(),  str'{}'aft \n", text.toStdString()) << std::endl;

    if (event->key() == Qt::Key_Up) {
        k = VTERM_KEY_UP;
    } else if (event->key() == Qt::Key_Down) {
        k = VTERM_KEY_DOWN;
    } else if (event->key() == Qt::Key_Left) {
        k = VTERM_KEY_LEFT;
    } else if (event->key() == Qt::Key_Right) {
        k = VTERM_KEY_RIGHT;
    }

    if (event->key() == Qt::Key_Backspace) {
        k = VTERM_KEY_BACKSPACE;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        k = VTERM_KEY_ENTER;
        std::cout << std::format("  doing key = ENTER '{}'  ", (int)k) << std::endl;
        std::string msg = "\r\n";
        vterm_input_write(term, msg.c_str(), msg.size());
    } else if (event->key() == Qt::Key_Home) {
        k = VTERM_KEY_HOME;
    } else if (event->key() == Qt::Key_Escape) {
        k = VTERM_KEY_ESCAPE;
    }

    if (k == VTERM_KEY_NONE) {
        std::cout << std::format("  VTERM_KEY_NONE '{}'  ", (int)k);

    } else {
        std::cout << std::format("  doing vterm_keyboard_key() '{}'  ", (int)k);
        vterm_keyboard_key(term, k, VTERM_MOD_NONE);
    }



    if(text.size()) {
        unsigned int ch = text.toUcs4()[0];
        vterm_keyboard_unichar(term, ch, VTERM_MOD_NONE);
        auto ss = text.toStdString();
        std::cout << std::format("  TermEmu::keyPressEvent, writing '{}'", ss) << std::endl;

        vterm_input_write(term, ss.c_str(), ss.size());
        update();
        // return;
    } else {
        std::cout << "Text() had no size\n";
    }
    update();

    std::cout << std::format("\n finished TermEmu::keyPressEvent() ") << std::endl;

}

void TermEmu::paintEvent(QPaintEvent *event)
{
    if(!screen) {
        return;
    }

    std::string str;
    VTermRect v_rect{0, term_h, 0, term_w};
    vterm_screen_get_text(screen, str.data(), str.size(), v_rect);

    QPainter p(this);
    p.fillRect(rect(), QBrush(Qt::black));
    p.setPen(Qt::white);
    p.setFont(QFont("Consolas", 16));


    for(int row = 0; row < term_h; row++) {
        VTermRect rect{0, row, 0, term_w};
        vterm_screen_get_text(screen, str.data(), str.size(), v_rect);
        p.drawText(0, (row+1) * 16, QString::fromStdString(str));
        p.drawLine(0, row * 16, width(), row*16);
        for(int column = 0; column < term_w; column++) {
            VTermScreenCell refCell{};
            VTermPos vtp{row, column};
            vterm_screen_get_cell(screen, vtp, &refCell);
            if(refCell.chars[0]) {
                // std::cout << std::format("{}", (char)refCell.chars[0]);
                p.drawText(16*column, 16*(row+1), QString::fromUcs4(refCell.chars, refCell.width));
            }

        }
    }

}
