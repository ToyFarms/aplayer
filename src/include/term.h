#ifndef __TERM_H
#define __TERM_H

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
#define TRESETTERM    "[!p"
#define THLINE        "[%dX"
#define TMAINBUF      "[?1049l"
#define TALTBUF       "[?1049h"
#define TCURSORSHOW   "[?25h"
#define TCURSORHIDE   "[?25l"
#define TGETCURSOR    "[6n"
#define TMOUSEENABLE  "[?1003h"
#define TMOUSEDISABLE "[?1003l"

typedef struct handle_t
{
    int fd;
} handle_t;

void term_write(handle_t handle, char *str, int size);
void term_altbuf(handle_t handle);
void term_mainbuf(handle_t handle);

#endif /* __TERM_H */
