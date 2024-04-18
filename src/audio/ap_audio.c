#include "ap_audio.h"

static void audio_fill_prop(APAudioContext *audioctx);
static int audio_decode(APAudioContext *audioctx, AVPacket *pkt);

APPacketQueue *ap_packet_queue_alloc()
{
    APPacketQueue *q = (APPacketQueue *)calloc(1, sizeof(APPacketQueue));
    if (!q)
        return NULL;

    pthread_mutex_init(&q->mutex, NULL);

    return q;
}

void ap_packet_queue_free(APPacketQueue **q)
{
    PTR_PTR_CHECK(q, );

    while ((*q)->first)
    {
        av_frame_free(&(*q)->first->pkt.frame);
        PacketList *next = (*q)->first->next;
        free((*q)->first);
        (*q)->first = next;
    }
    pthread_mutex_destroy(&(*q)->mutex);

    free(*q);
    *q = NULL;
}

bool ap_packet_queue_empty(APPacketQueue *q)
{
    return q->first == NULL;
}

int ap_packet_queue_enqueue(APPacketQueue *q, Packet pkt)
{
    PacketList *pkt_list = (PacketList *)calloc(1, sizeof(PacketList));
    if (!pkt_list)
        return -1;

    pkt_list->pkt = pkt;
    pthread_mutex_lock(&q->mutex);
    if (ap_packet_queue_empty(q))
        q->first = pkt_list;
    else
        q->last->next = pkt_list;

    q->last = pkt_list;
    q->nb_packets++;

    pthread_mutex_unlock(&q->mutex);

    return 0;
}

Packet ap_packet_queue_dequeue(APPacketQueue *q)
{
    pthread_mutex_lock(&q->mutex);
    if (ap_packet_queue_empty(q))
        return (Packet){0};

    Packet pkt = q->first->pkt;
    PacketList *next = q->first->next;

    free(q->first);

    q->first = next;
    if (!q->first)
        q->last = NULL;
    q->nb_packets--;

    pthread_mutex_unlock(&q->mutex);

    return pkt;
}

APAudioContext *ap_audio_alloc(const char *filename)
{
    av_log(NULL, AV_LOG_DEBUG, "Allocating audio context for '%s'\n", filename);
    APAudioContext *audioctx =
        (APAudioContext *)calloc(1, sizeof(APAudioContext));
    if (!audioctx)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate audio context\n");
        return NULL;
    }

    audioctx->filename = strdup(filename);
    audioctx->it = (AudioInternalCtx *)calloc(1, sizeof(AudioInternalCtx));

    return audioctx;
}

void ap_audio_free(APAudioContext **audioctx)
{
    PTR_PTR_CHECK(audioctx, );

    av_log(NULL, AV_LOG_DEBUG, "Free audio context for '%s'\n",
           (*audioctx)->filename);

    AudioInternalCtx *it = (*audioctx)->it;

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free AVCodecContext\n");
    avcodec_free_context(&it->avctx);

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Closing AVFormatContext\n");
    avformat_close_input(&it->ic);

    free(it);
    (*audioctx)->it = NULL;

    free((*audioctx)->filename);
    (*audioctx)->filename = NULL;

    free(*audioctx);
    *audioctx = NULL;
}

int ap_audio_init(APAudioContext *audioctx)
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing audio context\n");

    if (!audioctx->filename)
    {
        av_log(NULL, AV_LOG_ERROR, "Filename cannot be NULL\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Opening input\n");
    int ret =
        avformat_open_input(&audioctx->it->ic, audioctx->filename, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open input: %s\n",
               av_err2str(ret));
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Getting stream information\n");
    ret = avformat_find_stream_info(audioctx->it->ic, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to find the stream info: %s\n",
               av_err2str(ret));
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Getting audio stream index\n");
    audioctx->it->audio_stream = -1;
    for (int i = 0; i < audioctx->it->ic->nb_streams; i++)
    {
        if (audioctx->it->ic->streams[i]->codecpar->codec_type ==
            AVMEDIA_TYPE_AUDIO)
        {
            audioctx->it->audio_stream = i;
            break;
        }
    }

    if (audioctx->it->audio_stream < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not find audio stream\n");
        return -1;
    }

    av_dump_format(audioctx->it->ic, audioctx->it->audio_stream,
                   audioctx->filename, 0);

    av_log(NULL, AV_LOG_DEBUG, "Finding decoder\n");
    audioctx->it->codec = avcodec_find_decoder(
        audioctx->it->ic->streams[audioctx->it->audio_stream]
            ->codecpar->codec_id);
    if (!audioctx->it->codec)
    {
        av_log(NULL, AV_LOG_ERROR, "Unsupported codec\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVCodecContext\n");
    audioctx->it->avctx = avcodec_alloc_context3(audioctx->it->codec);
    if (!audioctx->it->avctx)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate AVCodecContext\n");
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Converting parameter to context\n");
    ret = avcodec_parameters_to_context(
        audioctx->it->avctx,
        audioctx->it->ic->streams[audioctx->it->audio_stream]->codecpar);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR,
               "Failed to convert parameter to context: %s\n", av_err2str(ret));
        return -1;
    }

    av_log(NULL, AV_LOG_DEBUG, "Opening codec\n");
    ret = avcodec_open2(audioctx->it->avctx, audioctx->it->codec, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open codec: %s\n",
               av_err2str(ret));
        return -1;
    }

    audio_fill_prop(audioctx);

    return 0;
}

static void audio_fill_prop(APAudioContext *audioctx)
{
    AVCodecContext *avctx = audioctx->it->avctx;
    audioctx->channels = avctx->ch_layout.nb_channels;
    audioctx->sample_rate = avctx->sample_rate;
}

void ap_audio_connect_buffer(APAudioContext *audioctx, APPacketQueue *pkt_queue)
{
    if (!pkt_queue)
    {
        av_log(NULL, AV_LOG_ERROR, "pkt_queue is NULL\n");
        return;
    }
    audioctx->pkt_queue = pkt_queue;
}

void ap_audio_read(APAudioContext *audioctx, APPacketQueue *pkt_queue,
                   int max_pkt_len)
{
    AVPacket *pkt = av_packet_alloc();

    while (av_read_frame(audioctx->it->ic, pkt) >= 0)
    {
        while (max_pkt_len > 0 && pkt_queue->nb_packets >= max_pkt_len)
            av_usleep(10 * 1000);

        if (pkt->stream_index == audioctx->it->audio_stream)
        {
            audio_decode(audioctx, pkt);
        }

        av_packet_unref(pkt);
    }
}

static int audio_decode(APAudioContext *audioctx, AVPacket *pkt)
{
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate AVFrame\n");
        return -1;
    }

    avcodec_send_packet(audioctx->it->avctx, pkt);

    int ret = avcodec_receive_frame(audioctx->it->avctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return ret;
    else if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error during decoding: %s\n",
               av_err2str(ret));
        return ret;
    }

    ap_packet_queue_enqueue(audioctx->pkt_queue,
                            (Packet){frame, audioctx->it->avctx->sample_fmt});

    return 0;
}
