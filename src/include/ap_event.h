#ifndef __AP_EVENT_H
#define __AP_EVENT_H

#include <stdbool.h>
#include <wchar.h>
#include <stdint.h>

#define APEV_MLEFT_CLICK   (1 << 0)
#define APEV_MMIDDLE_CLICK (1 << 1)
#define APEV_MRIGHT_CLICK  (1 << 2)

#define APEV_KMOD_SHIFT (1 << 0)
#define APEV_KMOD_CTRL  (1 << 1)
#define APEV_KMOD_ALT   (1 << 2)

typedef enum APEventType
{
    AP_EVENT_KEY,
    AP_EVENT_MOUSE,
    AP_EVENT_BUFFER,
    AP_EVENT_UNKNOWN,
} APEventType;

typedef struct APEventKey
{
    bool keydown;
    char ascii;
    wchar_t unicode;
    uint32_t virtual;
    uint32_t mod;
} APEventKey;

typedef struct APEventMouse
{
    int x, y;
    uint32_t state;
    bool double_clicked;
    bool moved;
    bool scrolled;
    int scroll_delta;
} APEventMouse;

typedef struct APEventBuffer
{
    int width, height;
} APEventBuffer;

typedef struct APEvent
{
    APEventType type;
    union
    {
        APEventKey key;
        APEventMouse mouse;
        APEventBuffer buf;
    } event;
} APEvent;

#endif /* __AP_EVENT_H */
