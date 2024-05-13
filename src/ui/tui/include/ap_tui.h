#ifndef __AP_TUI_H
#define __AP_TUI_H

#include <stdbool.h>
#include "libavutil/time.h"
#include "ap_array.h"
#include "ap_utils.h"
#include <pthread.h>
#include "ap_flags.h"
#include "ap_widgets.h"

typedef struct APTUIParams
{
    APTermContext *termctx;
    APWidgets *widgets;
} APTUIParams;

void ap_tui_init_widgets(APWidgets *ws);
void ap_tui_propagate_event(APWidgets *ws, APEvent e);
void *ap_tui_render_loop(void *arg);
pthread_t ap_tui_render_loop_async(APTermContext *termctx, APWidgets *widgets);

#endif /* __AP_TUI_H */
