#ifndef __SESSION_H
#define __SESSION_H

#include "app.h"
#include <stddef.h>

int session_deserialize(app_instance *app, const char *data, size_t size);
char *session_serialize(app_instance *app);

#endif /* __SESSION_H */
