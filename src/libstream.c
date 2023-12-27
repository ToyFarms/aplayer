#include "libstream.h"

/* Initialize StreamState, returns NULL on fail */
StreamState *stream_state_init(char *filename)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing StreamState.\n");

    StreamState *sst = (StreamState *)malloc(sizeof(StreamState));
    if (!sst)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate StreamState.\n");
        goto fail;
    }

    memset(sst, 0, sizeof(sst));

    sst->filename = filename;

    sst->ic = NULL;
    sst->iformat = NULL;
    sst->swr_ctx = NULL;
    sst->frame = NULL;
    sst->swr_frame = NULL;

    sst->audio_stream = NULL;
    sst->audiodec = NULL;
    sst->audio_stream_index = -1;

    if (stream_open(sst) < 0)
        goto fail;

    if (stream_init_stream(sst, AVMEDIA_TYPE_AUDIO) < 0)
        goto fail;

    if (stream_init_swr(sst) < 0)
        goto fail;

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVFrame frame.\n");
    sst->frame = av_frame_alloc();
    if (!sst->frame)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate AVFrame frame.\n");
        goto fail;
    }

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVFrame swr_frame\n");
    sst->swr_frame = av_frame_alloc();
    if (!sst->swr_frame)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate swr_frame.\n");
        goto fail;
    }

    return sst;

fail:
    stream_state_free(&sst);

    return NULL;
}

void stream_state_free(StreamState **sst)
{
    av_log(NULL, AV_LOG_DEBUG, "Free StreamState.\n");

    if (!sst)
        return;

    if (!(*sst))
        return;

    if ((*sst)->ic)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Close AVFormatContext.\n");
        avformat_close_input(&(*sst)->ic);
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free AVFormatContext.\n");
        avformat_free_context((*sst)->ic);
    }

    if ((*sst)->audiodec)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free audio decoder.\n");
        decoder_free(&(*sst)->audiodec);
    }

    if ((*sst)->swr_ctx)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free SwrContext.\n");
        swr_free(&(*sst)->swr_ctx);
    }

    if ((*sst)->frame)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free AVFrame frame.\n");
        av_frame_free(&(*sst)->frame);
    }

    if ((*sst)->swr_frame)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free AVFrame swr_frame.\n");
        av_frame_free(&(*sst)->swr_frame);
    }

    free(*sst);
    *sst = NULL;
}

int stream_open(StreamState *sst)
{
    if (stream_alloc_context(sst) < 0)
        return -1;

    if (stream_get_input_format(sst, sst->filename) < 0)
        return -1;

    if (stream_open_input(sst) < 0)
        return -1;

    if (stream_get_info(sst) < 0)
        return -1;

    return 1;
}

/* Open a stream, returns -1 on fail */
int stream_open_input(StreamState *sst)
{
    av_log(NULL, AV_LOG_DEBUG, "Opening input.\n");
    int err = avformat_open_input(&sst->ic, sst->filename, sst->iformat, NULL);
    if (err < 0)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not open input %s. %s.\n",
               sst->filename,
               av_err2str(err));

        return -1;
    }

    return 1;
}

/* Allocate AVFormatContext, returns -1 on fail */
int stream_alloc_context(StreamState *sst)
{
    av_log(NULL, AV_LOG_DEBUG, "Allocating AVFormatContext.\n");
    AVFormatContext *ic = avformat_alloc_context();
    if (!ic)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate AVFormatContext.\n");
        return -1;
    }

    sst->ic = ic;
    return 1;
}

/* Get stream info, returns -1 on fail */
int stream_get_info(StreamState *sst)
{
    av_log(NULL, AV_LOG_DEBUG, "Getting stream info.\n");
    av_log_turn_off();

    int err = avformat_find_stream_info(sst->ic, NULL);

    av_log_turn_on();

    if (err < 0)
    {
        av_log(NULL, AV_LOG_FATAL,
               "%s: Could not find codec parameters. %s.\n",
               sst->filename,
               av_err2str(err));
        return -1;
    }

    return 1;
}

/* Fill codec context based on codec parameter */
static int _stream_parameter2context(AVCodecContext *avctx, AVStream *stream)
{
    av_log(NULL, AV_LOG_DEBUG, "Filling context with parameter.\n");
    int err = avcodec_parameters_to_context(avctx, stream->codecpar);
    if (err < 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not copy codec parameters to codec context. %s\n", av_err2str(err));
        return -1;
    }

    return 1;
}

/* Get stream index given the stream type, returns -1 on fail */
static int _stream_get_index(AVFormatContext *ic, enum AVMediaType stream_type)
{
    av_log(NULL,
           AV_LOG_DEBUG,
           "Getting %s stream index.\n",
           av_get_media_type_string(stream_type));
    int stream_index = -1;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        if (ic->streams[i]->codecpar->codec_type == stream_type)
        {
            stream_index = i;
            break;
        }
    }

    if (stream_index == -1)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not get %s stream.\n",
               av_get_media_type_string(stream_type));
    }

    return stream_index;
}

/* Initialize stream, returns -1 on fail */
int stream_init_stream(StreamState *sst, enum AVMediaType media_type)
{
    av_log(NULL,
           AV_LOG_DEBUG,
           "Initializing %s stream.\n",
           av_get_media_type_string(media_type));

    int stream_index;
    AVStream **stream;
    Decoder **dec;

    switch (media_type)
    {
    case AVMEDIA_TYPE_AUDIO:
        stream_index = _stream_get_index(sst->ic, media_type);
        stream = &sst->audio_stream;
        dec = &sst->audiodec;
        break;
    default:
        av_log(NULL,
               AV_LOG_FATAL,
               "Unhandled stream type: %s\n",
               av_get_media_type_string(media_type));
        return -1;
    }

    if (stream_index < 0)
        return -1;

    *stream = sst->ic->streams[stream_index];

    if (decoder_init(dec, *stream) < 0)
        return -1;

    if (_stream_parameter2context((*dec)->avctx, *stream) < 0)
        return -1;

    return 1;
}

/* Guess input format from the filename, returns -1 on fail */
int stream_get_input_format(StreamState *sst, char *filename)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing AVProbeData, %s.\n", filename);
    int size = 2048;
    AVProbeData pd = (AVProbeData){.buf = av_mallocz(size + AVPROBE_PADDING_SIZE),
                                   .buf_size = size,
                                   .filename = filename};

    if (!pd.buf)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate AVProbeData buffer: %s\n", filename);
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Getting AVInputFormat from %s.\n", filename);
    const AVInputFormat *iformat = av_probe_input_format(&pd, 1);
    if (!iformat)
    {
        av_log(NULL, AV_LOG_FATAL, "Unknown input format: %s.\n", filename);
        av_log(NULL, AV_LOG_DEBUG, "Free AVProbeData buffer.\n");
        av_freep(pd.buf);
        return -1;
    }

    sst->iformat = iformat;

    return 1;
}

int stream_init_swr(StreamState *sst)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing SwrContext parameter.\n");
    av_channel_layout_default(&sst->audiodec->avctx->ch_layout, sst->audiodec->avctx->ch_layout.nb_channels);

    int err = swr_alloc_set_opts2(&sst->swr_ctx,
                                  &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO,
                                  AV_SAMPLE_FMT_FLT,
                                  sst->audiodec->avctx->sample_rate,
                                  &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO,
                                  sst->audiodec->avctx->sample_fmt,
                                  sst->audiodec->avctx->sample_rate,
                                  AV_LOG_DEBUG, NULL);

    if (err < 0)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not initialize SwrContext parameter. %s.\n",
               av_err2str(err));
        return -1;
    }

    if (!sst->swr_ctx)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Null pointer: SwrContext.\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initializing SwrContext.\n");
    if ((err = swr_init(sst->swr_ctx)) < 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SwrContext. %s.\n", av_err2str(err));
        return -1;
    }

    return 1;
}