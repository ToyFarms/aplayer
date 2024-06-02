#ifndef __AP_WIDGETS_H
#define __AP_WIDGETS_H

#include "ap_dict.h"
#include "ap_event.h"
#include "ap_math.h"
#include "ap_playlist.h"
#include "sds.h"

typedef enum APWidgetContext
{
    AP_WIDGET_TUI_CTX,
    AP_WIDGET_GUI_CTX,
} APWidgetContext;

typedef struct APTUIWidgetState
{
    sds draw_cmd;
    void *internal;
} APTUIWidgetState;

typedef struct APWidget
{
    Vec2 pos;
    Vec2 size;
    APDict *theme;
    APDict *listeners;

    APWidgetContext ctx;
    union {
        APTUIWidgetState *tui;
        // APGUIWidgetState gui_state;
    } state;
    void (*init)(struct APWidget *);
    void (*draw)(struct APWidget *);
    void (*on_event)(struct APWidget *, APEvent);
    void (*free)(struct APWidget *);
    bool resized;
} APWidget;

void ap_widget_init(APWidget *w, Vec2 pos, Vec2 size, APDict *theme,
                    void (*init)(APWidget *), void (*draw)(APWidget *),
                    void (*on_event)(APWidget *, APEvent),
                    void (*free)(APWidget *));

void ap_widget_filelist_init(APWidget *w, APPlaylist *pl);
void ap_widget_filelist_draw(APWidget *w);
void ap_widget_filelist_on_event(APWidget *w, APEvent e);
void ap_widget_filelist_free(APWidget *w);

#endif /* __AP_WIDGETS_H */
