#ifndef __AP_TUI_H
#define __AP_TUI_H

#include <stdbool.h>
#include "ap_terminal.h"
#include "ap_utils.h"
#include "libavutil/time.h"
#include "ap_ui.h"
#include "ap_array.h"
#include "ap_widgets.h"

void *ap_tui_render_loop(void *arg);

#endif /* __AP_TUI_H */
