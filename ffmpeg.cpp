



extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libavutil/imgutils.h"
    #include "libavutil/avutil.h"
    #include "libavutil/avstring.h"
    #include "libavutil/opt.h"
    #include "libswresample/swresample.h"
}
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")



// todo: fsomething like this?
struct VideoBuffer
{
    AVFrame frame;  // this is setup to use raw_mem
    void *raw_mem;
};

struct SoundBuffer
{
    u8* data;
    int size_in_bytes;
};


struct StreamAV
{
    int index;
    AVCodecContext *codecContext;
    // AVCodec *codec;
};

struct MovieAV
{
    AVFormatContext *vfc;
    AVFormatContext *afc;  // now seperate sources are allowed so this seems sort of ok
    StreamAV video;
    StreamAV audio;

    bool IsAudioAvailable()
    {
        if (afc && audio.codecContext) return true;
        else return false;
    }
};

MovieAV DeepCopyMovieAV(MovieAV source)
{
    MovieAV dest;
    dest.vfc = source.vfc;
    dest.afc = source.afc;
    dest.video.index = source.video.index;
    dest.video.codecContext = source.video.codecContext;
    dest.audio.index = source.audio.index;
    dest.audio.codecContext = source.audio.codecContext;
    return dest;
}



// todo: decode with newest api?
// avcodec_send_packet / avcodec_receive_frame
// (for video too)
// avcodec_decode_audio4 is deprecated

// return bytes (not samples) written to outBuffer
int GetNextAudioFrame(
    AVFormatContext *fc,
    AVCodecContext *cc,
    int streamIndex,
    SoundBuffer outBuf,
    int requestedBytes,
    double startAtThisMsTimestamp, // throw out data until this TS, used for seeking, not used if < 0
    i64 *outPTS)
{

    if (!fc || !cc)
    {
        *outPTS = 0;
        return 0;
    }


    u8 *outBuffer = outBuf.data;

    // if (!swr)
    // {
    //     // todo: free this when loading new video? any other time?
    //     swr = swr_alloc_set_opts(NULL,         // we're allocating a new context
    //                 FFMPEG_LAYOUT,             // out_ch_layout    AV_CH_LAYOUT_STEREO  AV_CH_LAYOUT_MONO
    //                 FFMPEG_FORMAT,             // out_sample_fmt   AV_SAMPLE_FMT_S16   AV_SAMPLE_FMT_FLT
    //                 FFMPEG_SAMPLES_PER_SECOND, // out_sample_rate
    //                 cc->channel_layout,        // in_ch_layout
    //                 cc->sample_fmt,            // in_sample_fmt
    //                 cc->sample_rate,           // in_sample_rate
    //                 0,                         // log_offset
    //                 NULL);                     // log_ctx
    //     swr_init(swr);
    //     if (!swr_is_initialized(swr)) {
    //         OutputDebugString("ffmpeg: Audio resampler has not been properly initialized\n");
    //         return -1;
    //     }
    // }

    // char tempy[123];
    // sprintf(tempy, "%lli\n", cc->sample_fmt);
    // OutputDebugString(tempy);

    AVPacket readingPacket;
    av_init_packet(&readingPacket);



    AVFrame* frame = av_frame_alloc();
    if (!frame)
    {
        OutputDebugString("Error allocating the frame\n");
        return 1;
    }

    int bytes_written = 0;

    // https://stackoverflow.com/questions/20545767/decode-audio-from-memory-c

    // Read the packets in a loop
    while (av_read_frame(fc, &readingPacket) == 0)
    {
        if (readingPacket.stream_index == streamIndex)
        {
            // if this is just a copy, if we free and unref readingP do we free decodingP?
            AVPacket decodingPacket = readingPacket;

            // Audio packets can have multiple audio frames in a single packet
            while (decodingPacket.size > 0)
            {
                // Try to decode the packet into a frame
                // Some frames rely on multiple packets,
                // so we have to make sure the frame is finished before
                // we can use it
                int gotFrame = 0;
                int packet_bytes_decoded = avcodec_decode_audio4(cc, frame, &gotFrame, &decodingPacket);

                if (packet_bytes_decoded >= 0 && gotFrame)
                {

                    // shift packet over to get next frame (if there is one)
                    decodingPacket.size -= packet_bytes_decoded;
                    decodingPacket.data += packet_bytes_decoded;


                    int additional_bytes = frame->nb_samples *
                                           cc->channels *
                                           av_get_bytes_per_sample(cc->sample_fmt);



                    double msToPlayThisFrame = 1000.0 *
                        av_frame_get_best_effort_timestamp(frame) *
                        fc->streams[streamIndex]->time_base.num /
                        fc->streams[streamIndex]->time_base.den;


                    if (msToPlayThisFrame < startAtThisMsTimestamp && startAtThisMsTimestamp>=0)
                    {
                        // OutputDebugString("skipped a frame\n");
                        // frames_skipped++;

                        // if (!displayedSkipMsg) { displayedSkipMsg = true; OutputDebugString("skip: "); }

                        // double msTimestamp = msToPlayFrame + msAudioLatencyEstimate;
                        // i64 frame_count = nearestI64(msTimestamp/1000.0 * 30.0);
                        //     char frambuf[123];
                        //     sprintf(frambuf, "%lli ", frame_count+1);
                        //     OutputDebugString(frambuf);

                        // seems like we'd want this here right?
                        // av_packet_unref(&packet);
                        av_frame_unref(frame);

                        // continue;
                        goto next_frame;
                    }
                    // if (frames_skipped > 0) {
                    //     char skipbuf[256];
                    //     sprintf(skipbuf, "frames skipped: %i\n", frames_skipped);
                    //     OutputDebugString(skipbuf);
                    // }


                    // little fail-safe check so we don't overflow outBuffer
                    // (ie, in case we guessed when to quit wrong below)
                    if (bytes_written+additional_bytes > outBuf.size_in_bytes)
                    {
                        assert(false); // for now we want to know if this ever happens
                        return bytes_written;
                    }


                    // double msToPlayFrame = 1000 * frame->pts *
                    //     fc->streams[streamIndex]->time_base.num /
                    //     fc->streams[streamIndex]->time_base.den;

                    // char zxcv[123];
                    // sprintf(zxcv, "msToPlayAudioFrame: %.1f msSinceStart: %.1f\n",
                    //         msToPlayFrame,
                    //         msSinceStart
                    //         );
                    // OutputDebugString(zxcv);

                    // // todo: stretch or shrink this buffer
                    // // to external clock? but tough w/ audio latency right?
                    // double msDelayAllowed = 20;  // feels janky
                    // // console yourself with the thought that this should only happen if
                    // // our sound library or driver is playing audio out of sync w/ the system clock???
                    // if (msToPlayFrame + msDelayAllowed < msSinceStart)
                    // {
                    //     OutputDebugString("skipping some audio bytes.. tempy\n");
                    //     //continue;

                    //     // todo: replace this with better
                    //     int samples_to_skip = 10;
                    //     additional_bytes -= samples_to_skip *
                    //         av_get_bytes_per_sample(cc->sample_fmt);
                    // }


                    // see notes from http://lists.ffmpeg.org/pipermail/ffmpeg-user/2013-February/013592.html
                    // "It depends on sample format you set in swr_alloc_set_opts().
                    // For planar sample formats out[x] have x channel
                    // For interleaved sample formats there is out[0] only and it have
                    // all channels interleaved: ABABABABAB -> stereo: A is 1st and B 2nd channel.

                    bool planar = false;
                    switch (cc->sample_fmt) {
                        case AV_SAMPLE_FMT_U8P:
                        case AV_SAMPLE_FMT_S16P:
                        case AV_SAMPLE_FMT_S32P:
                        case AV_SAMPLE_FMT_FLTP:
                        case AV_SAMPLE_FMT_DBLP: planar = true;
                    }

                    if (!planar || cc->channels == 1)
                    {
                        memcpy(outBuffer, frame->data[0], additional_bytes);
                        outBuffer += additional_bytes;
                        bytes_written+=additional_bytes;
                    }
                    else
                    {
                        for (int sample = 0; sample < frame->nb_samples; sample++)
                        {
                            for (int channel = 0; channel < cc->channels; channel++)
                            {
                                u8 *thisChannel = frame->data[channel];
                                u8 *thisSample = thisChannel + sample*av_get_bytes_per_sample(cc->sample_fmt);
                                for (int byte = 0; byte < av_get_bytes_per_sample(cc->sample_fmt); byte++)
                                {
                                    *outBuffer = *thisSample;
                                    outBuffer++;
                                    thisSample++;
                                    bytes_written++;
                                }
                            }
                        }
                    }
                    // note bytes_written is total this call, not just this frame
                    // assert(bytes_written == additional_bytes); // only true on first loop



                    // now try to guess when we're done based on the size of the last frame
                    if (bytes_written+additional_bytes > requestedBytes)
                    {
                        *outPTS = av_frame_get_best_effort_timestamp(frame);

                        // av_free_packet(&readingPacket);
                        // av_free_packet(&decodingPacket);
                        av_free_packet(&readingPacket);
                        av_frame_free(&frame);
                        return bytes_written;
                    }

                    // // just one frame;
                    // return bytes_written;
                }
                else
                {
                    decodingPacket.size = 0;
                    decodingPacket.data = nullptr;
                }

                // grab pts from frame before unref, in case we exit via end of function
                *outPTS = av_frame_get_best_effort_timestamp(frame);

                av_frame_unref(frame);  // clear allocs made by avcodec_decode_audio4 ?

                // don't this right, because we may be pulling a second frame from it?
                // av_packet_unref(&decodingPacket);
            }

            // don't need because it's just a shallow copy of readingP??
            // av_free_packet(&decodingPacket);
            // av_packet_unref(&decodingPacket); // crash?? need to reuse over multi frames
        }

        next_frame:

        av_packet_unref(&readingPacket);  // clear allocs made by av_read_frame ?
    }

    // Some codecs will cause frames to be buffered up in the decoding process. If the CODEC_CAP_DELAY flag
    // is set, there can be buffered up frames that need to be flushed, so we'll do that
    if (cc->codec->capabilities & CODEC_CAP_DELAY)
    {
        assert(false);  // todo: for now we just need an example file with this
        av_init_packet(&readingPacket);
        // Decode all the remaining frames in the buffer, until the end is reached
        int gotFrame = 0;
        while (avcodec_decode_audio4(cc, frame, &gotFrame, &readingPacket) >= 0 && gotFrame)
        {
            // We now have a fully decoded audio frame
            // printAudioFrameInfo(cc, frame);
        }
    }

    av_free_packet(&readingPacket);
    av_frame_free(&frame);
    return bytes_written; // ever get here? yes, at the end of a file
}

bool GetNextVideoFrame(
    AVFormatContext *fc,
    AVCodecContext *cc,
    SwsContext *sws_context,
    int streamIndex,
    AVFrame *outFrame,
    double msOfDesiredFrame,
    double msAllowableAudioLead,
    bool skip_if_behind_audio,
    i64 *outPTS,
    int *frames_skipped)
{

    if (!fc || !cc)
    {
        *outPTS = 0;
        *frames_skipped = 0;
        return false;
    }


    AVPacket packet;

    AVFrame *frame = av_frame_alloc();  // just metadata

    if (!frame) { MsgBox("ffmpeg: Couldn't alloc frame."); return false; }

                // char temp2[123];
                // sprintf(temp2, "time_base %i / %i\n",
                //         fc->streams[streamIndex]->time_base.num,
                //         fc->streams[streamIndex]->time_base.den
                //         );
                // OutputDebugString(temp2);


    // i64 frame_want = nearestI64(msSinceStart /1000.0 * 30.0);
    //     char frambuf2[123];
    //     sprintf(frambuf2, "want: %lli \n", frame_want+1);
    //     OutputDebugString(frambuf2);
    // bool displayedSkipMsg = false;

    // int frames_skipped = 0;

    while(av_read_frame(fc, &packet) >= 0)
    {
        if (packet.stream_index == streamIndex)
        {
            int frame_finished;
            avcodec_decode_video2(cc, frame, &frame_finished, &packet);

            if (frame_finished)
            {

                // todo: is variable frame rate possible? (eg some frames shown twice?)
                // todo: is it possible to get these not in pts order? (doesn't seem like it)

                // char temp[123];
                // sprintf(temp, "frame->pts %lli\n", inFrame->pts);
                // OutputDebugString(temp);

                double msToPlayThisFrame = 1000.0 *
                    av_frame_get_best_effort_timestamp(frame) * //frame->pts *
                    fc->streams[streamIndex]->time_base.num /
                    fc->streams[streamIndex]->time_base.den;


                // char zxcv[123];
                // sprintf(zxcv, "msToPlayVideoFrame: %.1f msSinceStart: %.1f msAllowed: %.1f\n",
                //         msToPlayFrame,
                //         msSinceStart,
                //         msAudioLatencyEstimate + msDelayAllowed
                //         );
                // OutputDebugString(zxcv);


                if (msToPlayThisFrame < msOfDesiredFrame - msAllowableAudioLead
                    && skip_if_behind_audio)
                {
                    // OutputDebugString("skipped a frame\n");
                    *frames_skipped++;

                    // if (!displayedSkipMsg) { displayedSkipMsg = true; OutputDebugString("skip: "); }

                    // double msTimestamp = msToPlayFrame + msAudioLatencyEstimate;
                    // i64 frame_count = nearestI64(msTimestamp/1000.0 * 30.0);
                    //     char frambuf[123];
                    //     sprintf(frambuf, "%lli ", frame_count+1);
                    //     OutputDebugString(frambuf);

                    // seems like we'd want this here right?
                    av_packet_unref(&packet);
                    av_frame_unref(frame);

                    continue;
                }
                // if (frames_skipped > 0) {
                //     char skipbuf[256];
                //     sprintf(skipbuf, "frames skipped: %i\n", frames_skipped);
                //     OutputDebugString(skipbuf);
                // }


                *outPTS = av_frame_get_best_effort_timestamp(frame);


                // char linebuf[123];
                // sprintf(linebuf, "frame->linesize[0]: %i outFrame->linesize[0]: %i \n",
                //         frame->linesize[0],
                //         outFrame->linesize[0]
                //         );
                // OutputDebugString(linebuf);


                sws_scale(
                    sws_context,
                    (u8**)frame->data,
                    frame->linesize,
                    0,
                    frame->height,
                    outFrame->data,
                    outFrame->linesize);


                // as far as i can tell, these need to be freed before leaving
                // AND they need to be unref'd before every use below
                av_free_packet(&packet);
                av_frame_free(&frame);

                // for now try only returning when frame_finished,
                // (before it was every avcodec_decode_video2
                // even if we didn't get a compelte frame)
                return true;

            }


        }
        // call these before reuse in avcodec_decode_video2 or av_read_frame
        av_packet_unref(&packet);
        av_frame_unref(frame);
    }

    return false;
}






void InitAV()
{
    av_register_all();  // all formats and codecs
}

AVCodecContext *OpenAndFindCodec(AVFormatContext *fc, int streamIndex)
{
    AVCodecContext *orig = fc->streams[streamIndex]->codec;
    AVCodec *codec = avcodec_find_decoder(orig->codec_id);
    AVCodecContext *result = avcodec_alloc_context3(codec);  // todo: check if this is null?
    if (!codec)
        { MsgBox("ffmpeg: Unsupported codec. Yipes."); return false; }
    if (avcodec_copy_context(result, orig) != 0)
        { MsgBox("ffmpeg: Codec context copy failed."); return false; }
    if (avcodec_open2(result, codec, 0) < 0)
        { MsgBox("ffmpeg: Couldn't open codec."); return false; }
    return result;
}

bool OpenMovieAV(char *videopath, char *audiopath, MovieAV *outMovie)
{

    MovieAV file;

    file.vfc = 0;  // = 0 or call avformat_alloc_context before opening?
    file.afc = 0;  // = 0 or call avformat_alloc_context before opening?

    int open_result1 = avformat_open_input(&file.vfc, videopath, 0, 0);
    int open_result2 = avformat_open_input(&file.afc, audiopath, 0, 0);
    if (open_result1 != 0 || open_result2 != 0)
    {
        char averr[1024];
        av_strerror(open_result1, averr, 1024);
        char msg[2048];
        sprintf(msg, "ffmpeg: Can't open file: %s\n", averr);
        MsgBox(msg);
        return false;
    }

    // populate fc->streams
    if (avformat_find_stream_info(file.vfc, 0) < 0 ||
        avformat_find_stream_info(file.afc, 0) < 0)
    {
        MsgBox("ffmpeg: Can't find stream info in file.");
        return false;
    }

    // this must be sending to stdout or something? (not showing up anywhere)
    av_dump_format(file.vfc, 0, videopath, 0);


    // find first video and audio stream
    // todo: use av_find_best_stream?
    file.video.index = -1;
    for (int i = 0; i < file.vfc->nb_streams; i++)
    {
        if (file.vfc->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            if (file.video.index != -1)
                MsgBox("More than one video stream!");

            if (file.video.index == -1)
                file.video.index = i;
        }
    }
    file.audio.index = -1;
    for (int i = 0; i < file.afc->nb_streams; i++)
    {
        if (file.afc->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
        {
            if (file.audio.index != -1)
                MsgBox("More than one audio stream!");

            if (file.audio.index == -1)
                file.audio.index = i;
        }
    }
    if (file.video.index != -1)
    {
        file.video.codecContext = OpenAndFindCodec(file.vfc, file.video.index);
    }
    if (file.audio.index != -1)
    {
        file.audio.codecContext = OpenAndFindCodec(file.afc, file.audio.index);
    }
    // todo: handle less than one or more than one of each properly

    *outMovie = file;
    return true;
}






// adapted from ffmpeg dump.c
void logFormatContextDuration(AVFormatContext *fc)
{
    int methodEnum = fc->duration_estimation_method;
    char buf[1024];
    if (fc->duration != AV_NOPTS_VALUE) {
        int hours, mins, secs, us;
        int64_t duration = fc->duration + (fc->duration <= INT64_MAX - 5000 ? 5000 : 0); // ?
        secs  = duration / AV_TIME_BASE;
        us    = duration % AV_TIME_BASE;
        mins  = secs / 60;
        secs %= 60;
        hours = mins / 60;
        mins %= 60;
        sprintf(buf, "DURATION:\n%02d:%02d:%02d.%02d\nmethod:%i\n", hours, mins, secs, (100 * us) / AV_TIME_BASE, methodEnum);
    } else {
        sprintf(buf, "DURATION:\nN/A\nmethod:%i\n", methodEnum);
    }
    OutputDebugString(buf);
}




void SetupSoundBuffer(AVCodecContext *acc, SoundBuffer *sound)
{
    int bytes_per_sample = av_get_bytes_per_sample(acc->sample_fmt) * acc->channels;

    // how big of chunks do we want to decode and queue up at a time
    // int buffer_seconds = int(targetMsPerFrame * 1000 * 5); //10 frames ahead
    int buffer_seconds = 1; // this is what limits how long we spend decoding audio each frame
    int samples_in_buffer = buffer_seconds * acc->sample_rate;
    sound->size_in_bytes = samples_in_buffer * bytes_per_sample;

    if (sound->data) free(sound->data);
    sound->data = (u8*)malloc(sound->size_in_bytes);

}


