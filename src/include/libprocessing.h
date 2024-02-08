#ifndef _LIBPROCESSING_H
#define _LIBPROCESSING_H

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libhelper.h"

float loudness_lufs(float *samples,
                         int num_channel,
                         int num_sample,
                         int sample_rate,
                         float block_size_ms);

#endif /* _LIBPROCESSING_H */