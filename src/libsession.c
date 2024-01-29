#include "libsession.h"

int session_write(const char *path, SessionState st)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        goto cleanup;

    cJSON *entries = cJSON_AddArrayToObject(root, "entries");
    if (!entries)
        goto cleanup;

    for (int i = 0; i < st.entry_size; i++)
    {
        cJSON *entry = cJSON_CreateObject();
        if (!entry)
            goto cleanup;
        cJSON_AddStringToObject(entry, "filename", st.entries[i].filename);
        cJSON_AddStringToObject(entry, "path", st.entries[i].path);
        cJSON_AddItemToArray(entries, entry);
    }

    if (!cJSON_AddBoolToObject(root, "paused", st.paused))
        goto cleanup;
    if (!cJSON_AddNumberToObject(root, "playing_idx", st.playing))
        goto cleanup;
    if (!cJSON_AddNumberToObject(root, "timestamp", st.timestamp))
        goto cleanup;
    if (!cJSON_AddNumberToObject(root, "volume", st.volume))
        goto cleanup;

    char *str = cJSON_PrintUnformatted(root);
    if (!str)
        goto cleanup;

    FILE *f = fopen(path, "wb");
    if (!f)
        goto cleanup;

    fwrite(str, sizeof(char), strlen(str), f);

cleanup:
    if (f)
        fclose(f);
    if (str)
        free(str);

    cJSON_Delete(root);
}

SessionState session_read(const char *path, int *success)
{
    SessionState st = {0};
    int ret_code = 0;

    char *content = file_read(path);
    if (!content)
        goto error;

    cJSON *root = cJSON_Parse(content);
    if (!root)
        goto error;

    cJSON *paused = cJSON_GetObjectItemCaseSensitive(root, "paused");
    if (!cJSON_IsBool(paused))
        goto error;
    st.paused = cJSON_IsTrue(paused);

    cJSON *playing_idx = cJSON_GetObjectItemCaseSensitive(root, "playing_idx");
    if (!cJSON_IsNumber(playing_idx))
        goto error;
    st.playing = playing_idx->valueint;

    cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(root, "timestamp");
    if (!cJSON_IsNumber(timestamp))
        goto error;
    st.timestamp = (int64_t)timestamp->valuedouble;

    cJSON *volume = cJSON_GetObjectItemCaseSensitive(root, "volume");
    if (!cJSON_IsNumber(volume))
        goto error;
    st.volume = volume->valuedouble;

    cJSON *entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
    if (!cJSON_IsArray(entries))
        goto error;
    st.entry_size = cJSON_GetArraySize(entries);

    st.entries = (File *)malloc(st.entry_size * sizeof(File));

    cJSON *entry = NULL;
    int i = 0;
    cJSON_ArrayForEach(entry, entries)
    {
        cJSON *filename = cJSON_GetObjectItemCaseSensitive(entry, "filename");
        if (!cJSON_IsString(filename))
            goto error;
        st.entries[i].filename = strdup(filename->valuestring);

        cJSON *_path = cJSON_GetObjectItemCaseSensitive(entry, "path");
        if (!cJSON_IsString(_path))
            goto error;
        st.entries[i].path = strdup(_path->valuestring);

        st.entries[i].stat = file_get_stat(st.entries[i].path, NULL);

        i++;
    }

    goto cleanup;

error:
    ret_code = -1;
    if (st.entries)
        free(st.entries);
cleanup:
    if (content)
        free(content);

    cJSON_Delete(root);

    if (success)
        *success = ret_code;

    return st;
}
