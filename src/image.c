#include "image.h"
#include "imgconv.h"
#include "logger.h"
#include <stdlib.h>
#include <threads.h>

void image_free(image_t *img)
{
    if (img == NULL)
        return;

    if (img->repr)
    {
        image_free(img->repr);
        free(img->repr);
    }
    else
    {
        free(img->data);
        return;
    }

    free(img->title);
    free(img->comment);
    free(img->data);
}

int image_resize(image_t *img, int flags, int width, int height)
{
    if (img->repr)
        free(img->repr->data);
    else
        img->repr = calloc(1, sizeof(*img->repr));

    imgconv_frame resized_frame =
        imgconv_resize(img->data, img->width, img->height, AV_PIX_FMT_RGB24,
                       AV_PIX_FMT_RGB24, width, height);
    if (resized_frame.buffer == NULL)
    {
        log_error("Failed to convert image\n");
        return -1;
    }

    img->repr->data = resized_frame.buffer;
    img->repr->size = resized_frame.size;
    img->repr->width = resized_frame.width;
    img->repr->height = resized_frame.height;
    img->repr->title = img->title;
    img->repr->comment = img->comment;
    img->repr->repr = NULL;

    return 0;
}
