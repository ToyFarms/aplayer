#include "session.h"
#include "cJSON.h"
#include "audio_source.h"

int session_deserialize(app_instance *app, const char *data, size_t size)
{
    cJSON *root = cJSON_ParseWithLength(data, size);
    playlist_deserialize(&app->playlist, root);
    cJSON_Delete(root);

    return 0;
}

char *session_serialize(app_instance *app)
{
    cJSON *root = playlist_serialize(&app->playlist);
    char *s = cJSON_Print(root);

    // audio_source *src;
    // ARR_FOREACH_BYREF(app->audio->mixer.sources, src, i)
    // {
    // }

    return s;
}
