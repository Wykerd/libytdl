#include <ytdl/mux.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static int ytdl__mux_write_frame (AVFormatContext *fmt_ctx, AVFormatContext *o_fmt_ctx, 
                                    AVPacket *packet, int64_t *next_pts, int video_st, int loglevel, int idx) 
{
    int ret;
    AVStream *in_stream, *out_stream;
    ret = av_read_frame(fmt_ctx, packet);
    if (ret < 0)
        return 0;
        
    in_stream = fmt_ctx->streams[packet->stream_index];
    if (packet->stream_index != video_st) {
        av_packet_unref(packet);
        return 1;
    }
    packet->stream_index = idx;
    out_stream = o_fmt_ctx->streams[packet->stream_index];
    av_packet_rescale_ts(packet, in_stream->time_base, out_stream->time_base);
    *next_pts = packet->pts;
    
    ret = av_interleaved_write_frame(o_fmt_ctx, packet);
    if (ret < 0) {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Error muxing packet\n");
        return 0;
    }
    av_packet_unref(packet);
    return 1;
}

int ytdl_mux_files (const char *audio_path, const char *video_path, const char *output_path, ytdl_mux_loglevel loglevel)
{
    int video_st = -1, audio_st = -1;
    int64_t next_v_pts = 0, next_a_pts = 0;
    AVFormatContext *a_fmt_ctx = NULL, *v_fmt_ctx = NULL, *o_fmt_ctx;
    AVPacket packet;
    int ret, encode_video = 1, encode_audio = 1;

    avformat_alloc_output_context2(&o_fmt_ctx, NULL, "matroska", output_path);
    if (!o_fmt_ctx)
    {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto exit_1;
    }

    if ((ret = avformat_open_input(&a_fmt_ctx, audio_path, NULL, NULL)) < 0)
    {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Could not open input file '%s'\n", audio_path);
        goto exit_1;
    }

    if ((ret = avformat_find_stream_info(a_fmt_ctx, NULL)) < 0) {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Failed to retrieve input stream '%s' information\n", audio_path);
        goto exit_2;
    }

    for (int i = 0; i < a_fmt_ctx->nb_streams; i++)
    {
        if (a_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {    
            AVStream *out_stream;
            AVStream *in_stream = a_fmt_ctx->streams[i];
            AVCodecParameters *in_codecpar = in_stream->codecpar;
            
            audio_st = i;

            out_stream = avformat_new_stream(o_fmt_ctx, NULL);
            if (!out_stream)
            {
                if (loglevel > YTDL_MUX_LOG_QUIET)
                    fprintf(stderr, "Failed allocating audio output stream\n");
                ret = AVERROR_UNKNOWN;
                goto exit_2;
            }
            in_codecpar->codec_tag = 0;
            ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
            if (ret < 0) {
                if (loglevel > YTDL_MUX_LOG_QUIET)
                    fprintf(stderr, "Failed to copy audio codec parameters\n");
                goto exit_2;
            }

            break;
        }
    }

    if (audio_st == -1)
    {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Audio stream not found\n");
        ret = AVERROR_UNKNOWN;
        goto exit_2;
    }

    if ((ret = avformat_open_input(&v_fmt_ctx, video_path, NULL, NULL)) < 0)
    {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Could not open input file '%s'\n", video_path);
        goto exit_2;
    }

    if ((ret = avformat_find_stream_info(v_fmt_ctx, NULL)) < 0) {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Failed to retrieve input stream '%s' information\n", video_path);
        goto exit_3;
    }

    for (int i = 0; i < v_fmt_ctx->nb_streams; i++)
    {
        if (v_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {    
            AVStream *out_stream;
            AVStream *in_stream = v_fmt_ctx->streams[i];
            AVCodecParameters *in_codecpar = in_stream->codecpar;
            
            video_st = i;

            out_stream = avformat_new_stream(o_fmt_ctx, NULL);
            if (!out_stream)
            {
                if (loglevel > YTDL_MUX_LOG_QUIET)
                    fprintf(stderr, "Failed allocating video output stream\n");
                ret = AVERROR_UNKNOWN;
                goto exit_3;
            }
            ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
            if (ret < 0) {
                if (loglevel > YTDL_MUX_LOG_QUIET)
                    fprintf(stderr, "Failed to copy video codec parameters\n");
                goto exit_3;
            }

            break;
        }
    }

    if (video_st == -1)
    {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Video stream not found\n");
        ret = AVERROR_UNKNOWN;
        goto exit_3;
    }

    if (loglevel == YTDL_MUX_LOG_VERBOSE)
        av_dump_format(o_fmt_ctx, 0, output_path, 1);

    if (!(o_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_open(&o_fmt_ctx->pb, output_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", output_path);
            goto exit_3;
        }
    }

    ret = avformat_write_header(o_fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto exit_3;
    }

    while (encode_audio || encode_video) {
        if (encode_audio && (!encode_video || (av_compare_ts(next_a_pts, o_fmt_ctx->streams[0]->time_base,
                                                             next_v_pts, o_fmt_ctx->streams[1]->time_base) <= 0)))
        {
            encode_audio = ytdl__mux_write_frame(a_fmt_ctx, o_fmt_ctx, &packet, &next_a_pts, audio_st, loglevel, 0);
        } 
        else 
        {
            encode_video = ytdl__mux_write_frame(v_fmt_ctx, o_fmt_ctx, &packet, &next_v_pts, video_st, loglevel, 1);
        }
    }

    ret = av_write_trailer(o_fmt_ctx);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when terminating output file\n");
        goto exit_3;
    }

exit_3:
    if (!(o_fmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&o_fmt_ctx->pb);

    avformat_close_input(&v_fmt_ctx);
exit_2:
    avformat_close_input(&a_fmt_ctx);

    avformat_free_context(o_fmt_ctx);
exit_1:
    if (ret < 0 && ret != AVERROR_EOF) {
        if (loglevel > YTDL_MUX_LOG_QUIET)
            fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return ret;
    }
    return 0;
}
