#ifndef __TERM_H
#define __TERM_H

#include "_math.h"
#include <stdbool.h>
#include <stdint.h>

#define TESC          "\x1b"
#define TPOS00        "[1;1H"
#define TPOSYX        "[%d;%dH"
#define TNDOWN        "[%dB"
#define TNUP          "[%dA"
#define TNRIGHT       "[%dC"
#define TNLEFT        "[%dD"
#define TDOWN         "[B"
#define TUP           "[A"
#define TRIGHT        "[C"
#define TLEFT         "[D"
#define TBG           "[48;2;%d;%d;%dm"
#define TFG           "[38;2;%d;%d;%dm"
#define TBGFG         "[48;2;%d;%d;%d;38;2;%d;%d;%dm"
#define TRESET        "[0m"
#define TRESETBG      "[49m"
#define TRESETFG      "[39m"
#define THLINE        "[%dX"
#define TMAINBUF      "[?1049l"
#define TALTBUF       "[?1049h"
#define TCURSORSHOW   "[?25h"
#define TCURSORHIDE   "[?25l"
#define TMOUSEENABLE  "[?1003h"
#define TMOUSEDISABLE "[?1003l"

#define TERM_KMOD_SHIFT   (1 << 0)
#define TERM_KMOD_CTRL    (1 << 1)
#define TERM_KMOD_ALT     (1 << 2)
#define KEY_ISMOD(c, tgt) (((c).mod & (tgt)) != 0)
#define TERM_MAX_MOUSEKEY 8

#ifdef _WIN32
#  error Key remapping not defined
#elif defined(__linux__)
#  include "curses.h"
#  define TERM_KEY_ESC         27
#  define TERM_KEY_TAB         9
#  define TERM_KEY_F0          KEY_F0
#  define TERM_KEY_F(n)        (TERM_KEY_F0 + (n))
#  define TERM_KEY_F1          TERM_KEY_F(1)
#  define TERM_KEY_F2          TERM_KEY_F(2)
#  define TERM_KEY_F3          TERM_KEY_F(3)
#  define TERM_KEY_F4          TERM_KEY_F(4)
#  define TERM_KEY_F5          TERM_KEY_F(5)
#  define TERM_KEY_F6          TERM_KEY_F(6)
#  define TERM_KEY_F7          TERM_KEY_F(7)
#  define TERM_KEY_F8          TERM_KEY_F(8)
#  define TERM_KEY_F9          TERM_KEY_F(9)
#  define TERM_KEY_F10         TERM_KEY_F(10)
#  define TERM_KEY_F11         TERM_KEY_F(11)
#  define TERM_KEY_F12         TERM_KEY_F(12)
#  define TERM_KEY_ENTER       KEY_ENTER
#  define TERM_KEY_UP          KEY_UP
#  define TERM_KEY_DOWN        KEY_DOWN
#  define TERM_KEY_LEFT        KEY_LEFT
#  define TERM_KEY_LEFT_SHIFT  KEY_SLEFT
#  define TERM_KEY_RIGHT       KEY_RIGHT
#  define TERM_KEY_RIGHT_SHIFT KEY_SRIGHT
#  define TERM_KEY_BACKSPACE   KEY_BACKSPACE
#  define TERM_KEY_PAD_7       KEY_A1
#  define TERM_KEY_PAD_9       KEY_A3
#  define TERM_KEY_PAD_5       KEY_B2
#  define TERM_KEY_PAD_1       KEY_C1
#  define TERM_KEY_PAD_3       KEY_C3
#  define TERM_KEY_HOME        KEY_HOME
#  define TERM_KEY_HOME_SHIFT  KEY_SHOME
#  define TERM_KEY_END         KEY_END
#  define TERM_KEY_END_SHIFT   KEY_SEND
#  define TERM_KEY_INS         KEY_IC
#  define TERM_KEY_INS_SHIFT   KEY_SIC
#  define TERM_KEY_DEL         KEY_DC
#  define TERM_KEY_DEL_SHIFT   KEY_SDC
#  define TERM_KEY_PAGEUP      KEY_PPAGE
#  define TERM_KEY_PAGEDOWN    KEY_NPAGE
#endif

enum term_event_type
{
    TERM_EVENT_UNKNOWN = 0,
    TERM_EVENT_KEY,
    TERM_EVENT_MOUSE,
    TERM_EVENT_RESIZE,
};

typedef struct term_event_key
{
    bool keydown;
    char ascii;
    uint32_t virtual;
    uint32_t mod;
} term_event_key;

typedef struct term_event_mouse
{
    int x, y;
    bool state[TERM_MAX_MOUSEKEY];
    bool double_clicked;
    bool moved;
    bool scrolled;
    uint32_t mod;
    int scroll_delta;
} term_event_mouse;

typedef struct term_event_buffer
{
    int width, height;
} term_event_buffer;

typedef struct term_event
{
    enum term_event_type type;
    union {
        term_event_key key;
        term_event_mouse mouse;
        term_event_buffer resize;
    } as;
} term_event;

typedef struct handle_t
{
    int fd;
} handle_t;

enum handle_type
{
    HANDLE_STDOUT,
    HANDLE_STDIN,
    HANDLE_STDERR,
};

handle_t term_handle(enum handle_type type);
void term_write(char *str, int size);
void term_altbuf();
void term_mainbuf();
vec2 term_size();
int term_get_events(term_event *out, int max);

#endif /* __TERM_H */
