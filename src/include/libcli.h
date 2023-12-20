#ifndef _LIBCLI_H
#define _LIBCLI_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <libavutil/log.h>
#include <stdio.h>
#include <libavutil/common.h>

#include "sb.h"
#include "libhelper.h"

typedef enum BUFFER_TYPE
{
    BUF_MAIN,
    BUF_ALTERNATE,
} BUFFER_TYPE;

typedef struct CLIState
{
    char **entries;
    int entry_size;
    int entry_offset;

    int hovered_idx;
    int playing_idx;
    int selected_idx;

    bool force_redraw;

    HANDLE out;
    int width;
    int height;
} CLIState;

CLIState *cli_state_init();
void cli_state_free(CLIState **cst);

void cli_buffer_switch(BUFFER_TYPE type);
INPUT_RECORD *cli_read_in(unsigned long *out_size);
void cli_draw(CLIState *cst);
HANDLE cli_get_handle(BUFFER_TYPE type);
void cli_get_console_size(CLIState *cst);

#endif // _LIBCLI_H