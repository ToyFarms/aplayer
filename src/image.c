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
        free(img->repr->data);
        free(img->repr);
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
        imgconv_resize(img->data, flags, img->width, img->height,
                       AV_PIX_FMT_RGB24, AV_PIX_FMT_RGB24, width, height, true);
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

image_t image_from_frame(imgconv_frame *frame)
{
    image_t img = {0};
    if (frame == NULL)
        return img;

    img.data = frame->buffer;
    img.width = frame->width;
    img.height = frame->height;
    img.size = frame->size;

    return img;
}
