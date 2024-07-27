#include "term.h"

#include <string.h>
#include <unistd.h>
#include <termios.h>

static void term_prepare(handle_t handle)
{
    term_write(handle, TESC TMOUSEENABLE, -1);

    struct termios term;
    tcgetattr(handle.fd, &term);

    // term.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    // term.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    // term.c_oflag &= ~(OPOST);
    // term.c_cflag |= CS8;
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;

    tcsetattr(handle.fd, TCSANOW, &term);
}

static void term_reset(handle_t handle)
{
    term_write(handle, TESC TMOUSEDISABLE, -1);

    struct termios term;
    tcgetattr(handle.fd, &term);

    term.c_lflag |= ICANON | ECHO;

    tcsetattr(handle.fd, TCSANOW, &term);
}

void term_write(handle_t handle, char *str, int size)
{
    write(handle.fd, str, size >= 0 ? size : strlen(str));
}

void term_altbuf(handle_t handle)
{
    term_write(handle, TESC TALTBUF, -1);
    term_prepare(handle);
}

void term_mainbuf(handle_t handle)
{
    term_write(handle, TESC TMAINBUF, -1);
    term_reset(handle);
}
