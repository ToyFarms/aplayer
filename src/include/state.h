#ifndef _STATE_H
#define _STATE_H

#include <stdbool.h>
#include <stdint.h>
#include <libavformat/avformat.h>

#include "struct.h"
#include "libstream.h"
#include "libhelper.h"

PlayerState *state_player_init();
void state_player_free(PlayerState **pst);

StreamState *state_stream_init(char *filename);
void state_stream_free(StreamState **sst);

#endif // _STATE_H