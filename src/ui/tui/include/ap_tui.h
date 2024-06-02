#ifndef __AP_TUI_H
#define __AP_TUI_H

#include "ap_array.h"
#include "ap_terminal.h"

#include <pthread.h>
#include <stdbool.h>

typedef struct APTUIParams
{
    APTermContext *termctx;
    APArrayT(APWidget) * widgets;
} APTUIParams;

void ap_tui_widgets_init(APArrayT(APWidget) * ws);
void ap_tui_widgets_free(APArrayT(APWidget) * ws);
void ap_tui_propagate_event(APArrayT(APWidget) * ws, APEvent e);
void *ap_tui_render_loop(void *arg);
pthread_t ap_tui_render_loop_async(APTermContext *termctx,
                                   APArrayT(APWidget) * widgets);

#endif /* __AP_TUI_H */
