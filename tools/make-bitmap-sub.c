/*
 * Generates a Matroska file with two DVD bitmap subtitles.
 *
 * This tool exists because the FFmpeg CLI cannot convert text subtitles to
 * bitmap subtitles ("Subtitle encoding currently only possible from text to
 * text or bitmap to bitmap"), and the avcpp bitmap fixture has to be produced
 * deterministically.
 *
 * The output depends only on this file and on the linked FFmpeg version: the
 * muxer runs in bitexact mode and no metadata or timestamps are taken from the
 * environment.
 *
 * Usage: make-bitmap-sub <output.mkv>
 *
 * The generated file is released under CC0 1.0 Universal, like the rest of the
 * avcpp-test-data fixtures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>

#define BITMAP_WIDTH  64
#define BITMAP_HEIGHT 32
#define BITMAP_COLORS 4

/* Palette index 0 is transparent, which is what DVD subtitles expect. */
static const uint32_t palette[BITMAP_COLORS] = {
    0x00000000,
    0xFFFFFFFF,
    0xFF000000,
    0xFFFFFF00,
};

static void fill_bitmap(uint8_t *data)
{
    for (int y = 0; y < BITMAP_HEIGHT; ++y) {
        for (int x = 0; x < BITMAP_WIDTH; ++x) {
            uint8_t value;

            if (x < 2 || x >= BITMAP_WIDTH - 2 || y < 2 || y >= BITMAP_HEIGHT - 2)
                value = 1; /* white border */
            else
                value = ((x / 8 + y / 8) & 1) ? 3 : 2; /* yellow/black checks */

            data[y * BITMAP_WIDTH + x] = value;
        }
    }
}

static void report_error(const char *what, int error)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = { 0 };

    av_strerror(error, text, sizeof(text));
    fprintf(stderr, "%s: %s\n", what, text);
}

static int write_subtitle(AVCodecContext *encoder, AVFormatContext *format,
                          AVStream *stream, const uint8_t *bitmap, int64_t pts_ms)
{
    AVSubtitleRect rect = { 0 };
    AVSubtitleRect *rects[1] = { &rect };
    AVSubtitle subtitle = { 0 };
    uint8_t buffer[4096];
    AVPacket *packet = NULL;
    int ret;

    rect.x = 0;
    rect.y = 0;
    rect.w = BITMAP_WIDTH;
    rect.h = BITMAP_HEIGHT;
    rect.nb_colors = BITMAP_COLORS;
    rect.type = SUBTITLE_BITMAP;
    rect.data[0] = (uint8_t *)bitmap;
    rect.linesize[0] = BITMAP_WIDTH;
    rect.data[1] = (uint8_t *)palette;

    subtitle.format = 0;
    subtitle.start_display_time = 0;
    subtitle.end_display_time = 1000;
    subtitle.num_rects = 1;
    subtitle.rects = rects;

    ret = avcodec_encode_subtitle(encoder, buffer, sizeof(buffer), &subtitle);
    if (ret < 0) {
        report_error("encode subtitle", ret);
        return ret;
    }

    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "packet allocation failed\n");
        return AVERROR(ENOMEM);
    }

    ret = av_new_packet(packet, ret);
    if (ret < 0) {
        report_error("packet allocation", ret);
        av_packet_free(&packet);
        return ret;
    }

    memcpy(packet->data, buffer, packet->size);
    packet->pts = pts_ms;
    packet->dts = pts_ms;
    packet->duration = 1000;
    packet->stream_index = stream->index;

    ret = av_interleaved_write_frame(format, packet);
    av_packet_free(&packet);

    if (ret < 0)
        report_error("write packet", ret);

    return ret;
}

int main(int argc, char **argv)
{
    const AVCodec *codec = NULL;
    AVCodecContext *encoder = NULL;
    AVFormatContext *format = NULL;
    AVStream *stream = NULL;
    AVDictionary *options = NULL;
    uint8_t bitmap[BITMAP_WIDTH * BITMAP_HEIGHT];
    int ret = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <output.mkv>\n", argv[0]);
        return 2;
    }

    av_log_set_level(AV_LOG_ERROR);

    codec = avcodec_find_encoder(AV_CODEC_ID_DVD_SUBTITLE);
    if (!codec) {
        fprintf(stderr, "the dvdsub encoder is not available\n");
        return 1;
    }

    encoder = avcodec_alloc_context3(codec);
    if (!encoder) {
        fprintf(stderr, "encoder allocation failed\n");
        return 1;
    }

    encoder->time_base = (AVRational){ 1, 1000 };

    ret = avcodec_open2(encoder, codec, NULL);
    if (ret < 0) {
        report_error("open encoder", ret);
        goto fail;
    }

    ret = avformat_alloc_output_context2(&format, NULL, "matroska", argv[1]);
    if (ret < 0 || !format) {
        report_error("allocate output context", ret);
        goto fail;
    }

    format->flags |= AVFMT_FLAG_BITEXACT;

    stream = avformat_new_stream(format, NULL);
    if (!stream) {
        fprintf(stderr, "stream allocation failed\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    stream->time_base = (AVRational){ 1, 1000 };

    ret = avcodec_parameters_from_context(stream->codecpar, encoder);
    if (ret < 0) {
        report_error("copy codec parameters", ret);
        goto fail;
    }

    stream->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;

    ret = avio_open(&format->pb, argv[1], AVIO_FLAG_WRITE);
    if (ret < 0) {
        report_error("open output file", ret);
        goto fail;
    }

    av_dict_set(&options, "bitexact", "1", 0);

    ret = avformat_write_header(format, &options);
    if (ret < 0) {
        report_error("write header", ret);
        goto fail;
    }

    fill_bitmap(bitmap);

    ret = write_subtitle(encoder, format, stream, bitmap, 500);
    if (ret < 0)
        goto fail;

    ret = write_subtitle(encoder, format, stream, bitmap, 1500);
    if (ret < 0)
        goto fail;

    ret = av_write_trailer(format);
    if (ret < 0) {
        report_error("write trailer", ret);
        goto fail;
    }

fail:
    av_dict_free(&options);

    if (format) {
        if (format->pb)
            avio_closep(&format->pb);
        avformat_free_context(format);
    }

    avcodec_free_context(&encoder);

    return ret < 0 ? 1 : 0;
}
