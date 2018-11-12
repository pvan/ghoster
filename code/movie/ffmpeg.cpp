


const bool FFMPEG_LOADING_MSG = true;

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
// struct VideoBuffer
// {
//     AVFrame frame;  // this is setup to use raw_mem
//     void *raw_mem;
// };

struct ffmpeg_sound_buffer
{
    u8* data;
    int size_in_bytes;

    void setup(AVCodecContext *acc, int buffer_seconds = 1)
    {
        int bytes_per_sample = av_get_bytes_per_sample(acc->sample_fmt) * acc->channels;

        // how big of chunks do we want to decode and queue up at a time
        // int buffer_seconds = int(targetMsPerFrame * 1000 * 5); //10 frames ahead
        int samples_in_buffer = buffer_seconds * acc->sample_rate;
        size_in_bytes = samples_in_buffer * bytes_per_sample;

        if (data) free(data);
        data = (u8*)malloc(size_in_bytes);

    }

    void destroy()
    {
        if (data) free(data);
    }

};


struct ffmpeg_stream
{
    int index;
    AVCodecContext *codecContext;
    // AVCodec *codec;
};



AVCodecContext *ffmpeg_open_codec(AVFormatContext *fc, int streamIndex)
{
    AVCodecContext *orig = fc->streams[streamIndex]->codec;
    AVCodec *codec = avcodec_find_decoder(orig->codec_id);
    AVCodecContext *result = avcodec_alloc_context3(codec);  // todo: check if this is null?
    if (!codec)
        { LogError("ffmpeg: Unsupported codec. Yipes."); return false; }
    if (avcodec_copy_context(result, orig) != 0)
        { LogError("ffmpeg: Codec context copy failed."); return false; }
    if (avcodec_open2(result, codec, 0) < 0)
        { LogError("ffmpeg: Couldn't open codec."); return false; }
    return result;
}



const int FFMPEG_TITLE_SIZE = 256;

const int FFMPEG_PATH_SIZE = 1024;  // todo: what to use for this?






// adapted from ffmpeg dump.c
void ffmpeg_output_duration(AVFormatContext *fc)
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



// basically a static movie from ffmpeg source
struct ffmpeg_source
{
    bool loaded = false; // are our fields populated?

    AVFormatContext *vfc = 0;
    AVFormatContext *afc = 0;  // now seperate sources are allowed so this seems sort of ok
    ffmpeg_stream video;
    ffmpeg_stream audio;

    // move here for now but i don't think this is the best final position (for frameout at least)
    struct SwsContext *sws_context;
    AVFrame *frame_output;
    u8 *vid_buffer;
    int vid_width;
    int vid_height;

    char title[FFMPEG_TITLE_SIZE]; // todo: how big? todo: alloc this?
    char path[FFMPEG_PATH_SIZE]; // todo: how big? todo: alloc this?


    // metadata
    double fps;
    double durationSeconds;
    double totalFrameCount;

    int width;
    int height;
    double aspect_ratio;


    void MetadataToString(char *out)
    {
        if (!loaded) { sprintf(out, "not loaded"); return; }
        sprintf(out,
        "vid_width: %i\n"
        "vid_height: %i\n"
        "title: %s\n"
        "path: %s\n"
        "fps: %f\n"
        "durationSeconds: %f\n"
        "totalFrameCount: %f\n"
        "width: %i\n"
        "height: %i\n"
        "aspect_ratio: %f\n",
        vid_width,
        vid_height,
        title,
        path,
        fps,
        durationSeconds,
        totalFrameCount,
        width,
        height,
        aspect_ratio);
    }


    void FreeEverything()
    {
        if (vfc) avformat_close_input(&vfc);
        if (afc) avformat_close_input(&afc);
        if (video.codecContext) avcodec_free_context(&video.codecContext);
        if (audio.codecContext) avcodec_free_context(&audio.codecContext);
        if (sws_context) sws_freeContext(sws_context);
        if (frame_output) av_frame_free(&frame_output);
        if (vid_buffer) av_free(vid_buffer);
    }

    // destroys the reel passed in (we basically take the source and point our reel to it)
    void TransferFromReel(ffmpeg_source *other)
    {
        FreeEverything();

        *this = *other;
        memset(other, 0, sizeof(ffmpeg_source)); // destroy original copies
    }


    void PopulateMetadata()
    {
        //fps
        if (video.codecContext) {
            fps = ((double)video.codecContext->time_base.den /
                   (double)video.codecContext->time_base.num) /
                   (double)video.codecContext->ticks_per_frame;
        } else if (audio.codecContext) {
            // todo: test this, might get some weird fps from audio
            // update: fps is usually too quick on audio steams, just use a default
            fps = 30;
            // fps = ((double)audio.codecContext->time_base.den /
            //        (double)audio.codecContext->time_base.num) /
            //        (double)audio.codecContext->ticks_per_frame;
        } else {
            fps = 30; // default if no video or audio.. probably shouldn't get here though
            assert(false); // thinking we should have at least one a or v stream
        }

        // duration
        if (vfc) {
            durationSeconds = vfc->duration / (double)AV_TIME_BASE;
            totalFrameCount = durationSeconds * fps;
        } else if (afc) { //fallback to duration from audio
            durationSeconds = afc->duration / (double)AV_TIME_BASE;
            totalFrameCount = durationSeconds * fps;
        } else {
            // uuh..
            assert(false); // don't call populate metadata until we've loaded the sources
        }

        // w / h / aspect_ratio
        if (video.codecContext)
        {
            width = video.codecContext->width;
            height = video.codecContext->height;
            aspect_ratio = (double)width / (double)height;
        }
        else
        {
            // todo: what to use here?
            width = 400;
            height = 400;
            aspect_ratio = 1;
        }
    }

    // todo: these two should only ever be called together, combine?
    void SetupSwsContex(int w, int h)
    {
        if (sws_context) sws_freeContext(sws_context);

        sws_context = {0};

        if (video.codecContext)
        {
            // add this workaround to bypass sws_scale's no-resize special case
            // that seems to be causing extra band on right when width isn't multiple of 8
            // can non-multiple of 8 height also cause this?
            // more info here: https://lists.ffmpeg.org/pipermail/libav-user/2012-July/002451.html
            // the other thing that seems to be unique to vids with the extra bar
            // is a linesize[0] > width in the codecContext and the decoded frame linesize similarly larger
            int non_multiple_of_8_workaround = 0;
            if (w % 8 != 0)
                non_multiple_of_8_workaround = SWS_ACCURATE_RND;

            sws_context = sws_getCachedContext(
                sws_context,
                video.codecContext->width,
                video.codecContext->height,
                video.codecContext->pix_fmt,
                w,
                h,
                AV_PIX_FMT_RGB32,
                non_multiple_of_8_workaround |
                SWS_POINT,  // since src+dst should be the same size, just use linear
                0, 0, 0);
        }
    }
    void SetupFrameOutput(int w, int h)
    {
        if (frame_output) av_frame_free(&frame_output);
        frame_output = av_frame_alloc();  // just metadata

        if (!frame_output)
        {
            LogError("ffmpeg: Couldn't alloc frame");
            return;
        }

        // actual mem for frame
        // int numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, w,h); //deprecated
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32, w,h,1);
        if (vid_buffer) av_free(vid_buffer);
        vid_buffer = (u8*)av_malloc(numBytes);

        // set up frame to use buffer memory...
        av_image_fill_arrays(
            frame_output->data,
            frame_output->linesize,
            vid_buffer,
            AV_PIX_FMT_RGB32,
            w, h, 1);

        vid_width = w;
        vid_height = h;
    }

    bool IsAudioAvailable() { return afc && audio.codecContext; }
    bool IsVideoAvailable() { return vfc && video.codecContext; }

    void DebugPrintInfo()
    {
        // todo: implement this function

        OutputDebugString("\nvideo format ctx:\n");
        ffmpeg_output_duration(vfc);
        OutputDebugString("\naudio format ctx:\n");
        ffmpeg_output_duration(afc);

        char fpsbuf[123];
        if (video.codecContext)
        {
            sprintf(fpsbuf, "\nvideo frame rate: %i / %i  (%.2f FPS)\nticks_per_frame: %i\n",
                video.codecContext->time_base.num,
                video.codecContext->time_base.den,   fps,
                video.codecContext->ticks_per_frame
            );
        }
        else
        {
            sprintf(fpsbuf, "\nno video found, fps defaulted to %.2f fps\n", fps);
        }

        // (etc)
    }

    bool SetFromPaths(char *videopath, char *audiopath)
    {
        if (FFMPEG_LOADING_MSG) PRINT("Creating new source from paths...\n");

        if (FFMPEG_LOADING_MSG) PRINT("Checking if just a text file...\n");

        // special case to skip text files for now,
        // ffmpeg likes to eat these up and then our code doesn't know what to do with them
        if (StringEndsWith(videopath, ".txt") || StringEndsWith(audiopath, ".txt"))
        {
            LogError("Any txt file not supported yet.");
            return false;
        }

        if (FFMPEG_LOADING_MSG) PRINT("Freeing old pointers...\n");

        // free current video if exists
        FreeEverything();

        if (FFMPEG_LOADING_MSG) PRINT("Opening format contexts...\n");

        // now set from calling code so we get url correctly
        // if (strcmp(videopath, audiopath) == 0) sprintf(path, videopath);
        // else sprintf(path, "various");

        // file.vfc = 0;  // = 0 or call avformat_alloc_context before opening?
        // file.afc = 0;  // = 0 or call avformat_alloc_context before opening?
        int open_result1 = avformat_open_input(&vfc, videopath, 0, 0);
        if (FFMPEG_LOADING_MSG) PRINT("vfc loaded...\n");
        int open_result2 = avformat_open_input(&afc, audiopath, 0, 0);
        if (FFMPEG_LOADING_MSG) PRINT("afc loaded...\n");
        if (open_result1 != 0 || open_result2 != 0)
        {
            PRINT("Unable to load a format context...\n");
            char averr[1024];
            av_strerror(open_result1, averr, 1024);
            char msg[2048];
            sprintf(msg, "ffmpeg: Can't open file: %s\n", averr);
            LogError(msg);
            return false;
        }


        if (FFMPEG_LOADING_MSG) PRINT("Opening streams...\n");

        // todo: need to call avformat_close_input() at some point after avformat_open_input
        // (when no longer using the file maybe?)

        // populate fc->streams
        if (avformat_find_stream_info(vfc, 0) < 0 ||
            avformat_find_stream_info(afc, 0) < 0)
        {
            LogError("ffmpeg: Can't find stream info in file.");
            return false;
        }

        // this must be sending to stdout or something? (not showing up anywhere)
        // av_dump_format(vfc, 0, videopath, 0);

        if (FFMPEG_LOADING_MSG) PRINT("Searching for streams...\n");

        // find first video and audio stream
        // todo: use av_find_best_stream?
        video.index = -1;
        for (int i = 0; i < vfc->nb_streams; i++)
        {
            if (vfc->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
            {
                if (video.index != -1)
                    LogError("ffmpeg: More than one video stream!");

                if (video.index == -1)
                    video.index = i;
            }
        }
        audio.index = -1;
        for (int i = 0; i < afc->nb_streams; i++)
        {
            if (afc->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
            {
                if (audio.index != -1)
                    LogError("ffmpeg: More than one audio stream!");

                if (audio.index == -1)
                    audio.index = i;
            }
        }
        if (video.index != -1)
        {
            video.codecContext = ffmpeg_open_codec(vfc, video.index);
        }
        if (audio.index != -1)
        {
            audio.codecContext = ffmpeg_open_codec(afc, audio.index);
        }
        //todo: handle less than one or more than one of each properly


        // edit: this doesn't seem to trigger even on a text file?
        // if neither, this probably isn't a video file
        if (video.index == -1 && audio.index == -1)
        {
            LogError("ffmpeg: No audio or video streams in file.");
            return false;
        }

        // try this to catch bad files..
        // edit: also doesn't work
        if (!video.codecContext && !audio.codecContext)
        {
            LogError("ffmpeg: No audio or video streams in file.");
            return false;
        }

        if (FFMPEG_LOADING_MSG) PRINT("Populating metadata...\n");
        PopulateMetadata();

        if (FFMPEG_LOADING_MSG) PRINT("Setting up output frame...\n");
        SetupSwsContex(width, height);   // width/height here depend on metadata being populated first
        SetupFrameOutput(width, height);

        loaded = true;

        LogMessage("End of movie source creation\n");
        return true;
    }





    // todo: decode with newest api?
    // avcodec_send_packet / avcodec_receive_frame
    // (for video too)
    // avcodec_decode_audio4 is deprecated

    // return bytes (not samples) written to outBuffer
    int GetNextAudioFrame(
        ffmpeg_sound_buffer outBuf,
        int requestedBytes,
        double startAtThisMsTimestamp, // throw out data until this TS, used for seeking, not used if < 0
        i64 *outPTS)
    {
        AVFormatContext *fc = afc;
        AVCodecContext *cc = audio.codecContext;
        int streamIndex = audio.index;

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
        //         LogError("ffmpeg: Audio resampler has not been properly initialized\n");
        //         return -1;
        //     }
        // }

        // char tempy[123];
        // sprintf(tempy, "%lli\n", cc->sample_fmt);
        // LogMessage(tempy);

        AVPacket readingPacket;
        av_init_packet(&readingPacket);



        AVFrame* frame = av_frame_alloc();
        if (!frame)
        {
            LogError("ffmpeg: Error allocating the frame.\n");
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
                            // LogMessage("skipped a frame\n");
                            // frames_skipped++;

                            // if (!displayedSkipMsg) { displayedSkipMsg = true; LogMessage("skip: "); }

                            // double msTimestamp = msToPlayFrame + msAudioLatencyEstimate;
                            // i64 frame_count = nearestI64(msTimestamp/1000.0 * 30.0);
                            //     char frambuf[123];
                            //     sprintf(frambuf, "%lli ", frame_count+1);
                            //     LogMessage(frambuf);

                            // seems like we'd want this here right?
                            // av_packet_unref(&packet);
                            av_frame_unref(frame);

                            // continue;
                            goto next_frame;
                        }
                        // if (frames_skipped > 0) {
                        //     char skipbuf[256];
                        //     sprintf(skipbuf, "frames skipped: %i\n", frames_skipped);
                        //     LogMessage(skipbuf);
                        // }


                        // little fail-safe check so we don't overflow outBuffer
                        // (ie, in case we guessed when to quit wrong below)
                        if (bytes_written+additional_bytes > outBuf.size_in_bytes)
                        {
                            // assert(false); // for now we want to know if this ever happens
                            // update: happened on https://www.youtube.com/watch?v=w-NXWQ9QZwo
                            // todo: i wouldn't expect it to work to just return here, but it seems to in above vid
                            // maybe because it appears to only happen once, some time near startup
                            PRINT("Unexpected early return from audio decoder\n");
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
                        // LogMessage(zxcv);

                        // // todo: stretch or shrink this buffer
                        // // to external clock? but tough w/ audio latency right?
                        // double msDelayAllowed = 20;  // feels janky
                        // // console yourself with the thought that this should only happen if
                        // // our sound library or driver is playing audio out of sync w/ the system clock???
                        // if (msToPlayFrame + msDelayAllowed < msSinceStart)
                        // {
                        //     LogMessage("skipping some audio bytes.. tempy\n");
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

        // now that we're syncing a/v so well, we need to flush these or our video will freeze at the end?
        if (cc->codec->capabilities & CODEC_CAP_DELAY)
        {
            // for now, use this code to let caller know we're at end of file
            bytes_written *= -1;
            // below not seeming to work as expected

            // // assert(false);
            // av_free_packet(&readingPacket);
            // av_init_packet(&readingPacket);

            // int gotFrame = 0;
            // while (avcodec_decode_audio4(cc, frame, &gotFrame, &readingPacket) >= 0 && gotFrame)
            // {
            //     // todo: actually decode and use the data here
            //     // todo: just replace this whole function with send_packet/receive_frame api?
            // }
            // // note we need to set this to sync right or we'll still freeze
            // *outPTS = av_frame_get_best_effort_timestamp(frame);
        }

        av_free_packet(&readingPacket);
        av_frame_free(&frame);
        return bytes_written; // ever get here? yes, at the end of a file
    }

    bool GetNextVideoFrame(
        double msOfDesiredFrame,
        double msAllowableAudioLead,
        bool skip_if_behind_audio,
        i64 *outPTS,
        int *frames_skipped)
    {
        AVFormatContext *fc = vfc;
        AVCodecContext *cc = video.codecContext;
        SwsContext *sws = sws_context;
        int streamIndex = video.index;
        AVFrame *outFrame = frame_output;

        if (!fc || !cc)
        {
            *outPTS = 0;
            *frames_skipped = 0;
            return false;
        }


        AVPacket packet;

        AVFrame *frame = av_frame_alloc();  // just metadata

        if (!frame) { LogError("ffmpeg: Couldn't alloc frame."); return false; }

                    // char temp2[123];
                    // sprintf(temp2, "time_base %i / %i\n",
                    //         fc->streams[streamIndex]->time_base.num,
                    //         fc->streams[streamIndex]->time_base.den
                    //         );
                    // LogMessage(temp2);


        // i64 frame_want = nearestI64(msSinceStart /1000.0 * 30.0);
        //     char frambuf2[123];
        //     sprintf(frambuf2, "want: %lli \n", frame_want+1);
        //     LogMessage(frambuf2);
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
                    // LogMessage(temp);

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
                    // LogMessage(zxcv);


                    if (msToPlayThisFrame < msOfDesiredFrame - msAllowableAudioLead
                        && skip_if_behind_audio)
                    {
                        // LogMessage("skipped a frame\n");
                        *frames_skipped++;

                        // if (!displayedSkipMsg) { displayedSkipMsg = true; LogMessage("skip: "); }

                        // double msTimestamp = msToPlayFrame + msAudioLatencyEstimate;
                        // i64 frame_count = nearestI64(msTimestamp/1000.0 * 30.0);
                        //     char frambuf[123];
                        //     sprintf(frambuf, "%lli ", frame_count+1);
                        //     LogMessage(frambuf);

                        // seems like we'd want this here right?
                        av_packet_unref(&packet);
                        av_frame_unref(frame);

                        continue;
                    }
                    // if (frames_skipped > 0) {
                    //     char skipbuf[256];
                    //     sprintf(skipbuf, "frames skipped: %i\n", frames_skipped);
                    //     LogMessage(skipbuf);
                    // }

                    *outPTS = av_frame_get_best_effort_timestamp(frame);

                    // char linebuf[123];
                    // sprintf(linebuf, "frame->linesize[0]: %i outFrame->linesize[0]: %i \n",
                    //         frame->linesize[0],
                    //         outFrame->linesize[0]
                    //         );
                    // LogMessage(linebuf);

                    sws_scale(
                        sws,
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






};




double ffmpeg_seconds_from_pts(i64 pts, AVFormatContext *fc, int streamIndex)
{
    i64 base_num = fc->streams[streamIndex]->time_base.num;
    i64 base_den = fc->streams[streamIndex]->time_base.den;
    return ((double)pts * (double)base_num) / (double)base_den;
}



void ffmpeg_init()
{
    av_register_all();  // all formats and codecs
}







