#include "TermEmu.h"


#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <iostream>
#include <format>

#include <thread>

#ifdef WIN32

#include <Windows.h>
#include <process.h>
constexpr bool using_windows = true;


class TermEmuWinPty {
public:
    HANDLE inputReadSide, outputWriteSide;
    HANDLE outputReadSide, inputWriteSide;

    HPCON hPC;

    STARTUPINFOEXW startup_info;

    TermEmuWinPty() {

        if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0)) {
            // return HRESULT_FROM_WIN32(GetLastError());
            std::cout << "Failed to make inputWriteSide" << std::endl;

        }

        if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
            // return HRESULT_FROM_WIN32(GetLastError());
            std::cout << "Failed to make outputWriteSide" << std::endl;

        }

        auto result = CreatePseudoConsole({80, 24}, inputReadSide, outputWriteSide, 0, &hPC);
        if (FAILED(result)) {
            std::cout << "Failed to make Pseudoconsole" << std::endl;
        }
    }

    ~TermEmuWinPty() {
        ClosePseudoConsole(hPC);

        CloseHandle(inputReadSide);
        CloseHandle(outputWriteSide);
        CloseHandle(outputReadSide);
        CloseHandle(inputWriteSide);
    }



    HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEXW* psi)
    {
        // Prepare Startup Information structure
        STARTUPINFOEXW si{0};
        // ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(STARTUPINFOEX);

        // Discover the size required for the list
        size_t bytesRequired;
        InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

        // Allocate memory to represent the list
        si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
        if (!si.lpAttributeList)
        {
            return E_OUTOFMEMORY;
        }

        // Initialize the list memory location
        if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired))
        {
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // Set the pseudoconsole information into the list
        if (!UpdateProcThreadAttribute(si.lpAttributeList,
                                       0,
                                       PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                       hpc,
                                       sizeof(hpc),
                                       NULL,
                                       NULL))
        {
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            return HRESULT_FROM_WIN32(GetLastError());
        }

        *psi = si;

        return S_OK;
    }

    void setupArguments(QProcess::CreateProcessArguments &args) {
        // args.startupInfo.
        args.flags |= EXTENDED_STARTUPINFO_PRESENT;
        // args.startupInfo->hStdInput;

        startup_info.StartupInfo = *(args.startupInfo);
        args.startupInfo = (_STARTUPINFOW*)&startup_info;
        PrepareStartupInformation(hPC, &startup_info);
        std::cout << "Set up args\n";
    }


    void Listener() {
        std::cout << "Starting listener\n";
        constexpr int bufsize = 128;
        char buffer[bufsize];
        unsigned long bytes_read = 0;
        while (1)
        {
            unsigned long avail = 0;
            int idx = 0;
            for (auto &p : {inputReadSide, outputReadSide}) { // outputWriteSide, , inputWriteSide}) {

                auto peeked = PeekNamedPipe(p, 0, 0, 0, &avail, 0);
                if(peeked == false) {
                    auto E = GetLastError();
                    std::cout << std::format("Peeked E {}, {}, {}. {}", E, idx%4, peeked, avail) << std::endl;

                } else {
                    if(avail > 0) {
                        std::cout << std::format("Peeked {}, {}. {}", idx%4, peeked, avail) << std::endl;
                    }
                }

                idx++;
            }
            // Read client requests from the pipe. This simplistic code only allows messages
            // up to BUFSIZE characters in length.
            /*      auto fSuccess = ReadFile(
                outputReadSide,        // handle to pipe
                buffer,    // buffer to receive data
                bufsize, // size of buffer
                &bytes_read, // number of bytes read
                NULL);        // not overlapped I/O

            if (!fSuccess || bytes_read == 0) {
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                   // _tprintf(TEXT("InstanceThread: client disconnected.\n"));

                } else {
                   // _tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError());
                }
                break;
            }

            std::string_view view(buffer, bytes_read);
            std::cout << std::format("Reading pipe, got '{}'\n", view) << std::endl;
*/
        }
    }

};


#else

constexpr bool using_windows = false;

#endif


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
    connect(&timer, &QTimer::timeout, this, [&]() {
        update();
        // std::cout << "Timer" << std::endl;
        timer.start(500);
        draw_cursor = ! draw_cursor;
    });
    timer.start(100);


    // int rows = 25;  int cols = 80;
    term = vterm_new(term_h, term_w);
    vterm_set_utf8(term, true);

    screen = vterm_obtain_screen(term);
    state = vterm_obtain_state(term);
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

    if(using_windows) {
    //    Wpty = new TermEmuWinPty;
    }



    setFont(QFont("Consolas", 16));
    P = new QProcess(this);
    /* P->setCreateProcessArgumentsModifier([this](QProcess::CreateProcessArguments *args) {
        Wpty->setupArguments(*args);
        return;
    }); */
    connect(P, &QProcess::readyReadStandardOutput, [=]() {
        auto ba = P->readAllStandardOutput();
        qDebug() << ba.constData();
        vterm_input_write(term, ba.constData(), ba.size());
        update();
    });

    connect(P, &QProcess::errorOccurred, [=]() {
        qDebug() << "ERROR " << P->errorString();

    });


    connect(P, &QProcess::readyReadStandardError, [=,this]() {
        auto ba = P->readAllStandardError();
        vterm_input_write(term, ba.constData(), ba.size());
        update();
        qDebug() << ba.constData();
    });

    connect(P, &QProcess::started, [=]() {
        qDebug() << "Started!";
    });

    connect(P, &QProcess::stateChanged, [=]() {
        auto ba = P->state();
        qDebug() << "State changed " << ba;
    });


    // std::thread *t = new std::thread([this]() {
    //     Wpty->Listener();
    // });

    //  P->start("python", {"-i", "-c", "print ('Hello cmdline')"}, QProcess::ReadWrite);

    P->start("C:\\Users\\wrose\\Documents\\dev\\vcpkg\\downloads\\tools\\msys2\\d7266db249278763\\usr\\bin\\env.exe");

    if( P->waitForStarted()) {
        std::cout << "Yes\n";
        P->write("print('Hello World')\n");
    } else {
        std::cout << "Nope\n";
    }





}

TermEmu::~TermEmu()
{
    if(P) {
        P->terminate();
        auto b = P->waitForFinished(300);
        if(!b) {
            P->kill();
            b = P->waitForFinished();
        }
        std::cout << std::format("Process killed {} \n", b);

        delete P;
    }
}

void TermEmu::keyPressEvent(QKeyEvent *event)
{

    std::cout << "1 TermEmu::keyPressEvent(), 'No Format' " << std::endl;


    auto text = event->text();

    auto k = VTERM_KEY_NONE;

    std::cout << std::format("2 TermEmu::keyPressEvent(),  str'{}'aft \n", text.toStdString()) << std::endl;


    if(P) {
        P->write(text.toStdString().c_str());
        update();
        // return;
    }

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

    auto &f = font();
    if (f == QFont()) {
        qDebug() << "Default font " << f.family();
        setFont(QFont("Consolas", 16));
    }

    auto metric = QFontMetrics(f);
    auto char_w = metric.averageCharWidth();
    auto char_h = metric.lineSpacing();

    std::string str;
    VTermRect v_rect{0, term_h, 0, term_w};
    vterm_screen_get_text(screen, str.data(), str.size(), v_rect);

    QPainter p(this);
    p.fillRect(rect(), QBrush(Qt::black));
    p.setPen(Qt::white);
    // p.setFont(QFont("Consolas", 16));
    p.setFont(f);


    for(int row = 0; row < term_h; row++) {
        VTermRect rect{0, row, 0, term_w};
        vterm_screen_get_text(screen, str.data(), str.size(), v_rect);
        p.drawText(0, (row+1) * char_h, QString::fromStdString(str));
        p.drawLine(0, row * char_h, width(), row*char_h);
        for(int column = 0; column < term_w; column++) {
            VTermScreenCell refCell{};
            VTermPos vtp{row, column};
            vterm_screen_get_cell(screen, vtp, &refCell);
            if(refCell.chars[0]) {
                // std::cout << std::format("{}", (char)refCell.chars[0]);
                p.drawText(char_w*column, char_h*(row+1), QString::fromUcs4(refCell.chars, refCell.width));
            }

        }
    }

    VTermPos cursorpos{};
    if(!state) {
        return;
    }
    vterm_state_get_cursorpos(state, &cursorpos);
    if(draw_cursor) {
        p.setBrush(QColor::fromRgbF(.9, .8, .8, .8));
    } else {
        p.setBrush(QColor::fromRgbF(.2, .8, .8, .2));
    }
    p.drawRect(char_w * cursorpos.col, char_h * cursorpos.row, char_w, char_h);

}
