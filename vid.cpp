#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>


#include <gl/gl.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")


#include <SDL.h>
#include <SDL_thread.h>
#pragma comment(lib, "SDL2main.lib")
#pragma comment(lib, "SDL2.lib")

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
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






const int samples_per_second = 44100;



// char *INPUT_FILE = "D:/Users/phil/Desktop/sync3.mp4";
char *INPUT_FILE = "D:/Users/phil/Desktop/sync1.mp4";
// char *INPUT_FILE = "D:/Users/phil/Desktop/test4.mp4";
// char *INPUT_FILE = "D:/Users/phil/Desktop/test.mp4";
// char *INPUT_FILE = "D:/Users/phil/Desktop/test3.avi";




#define uint unsigned int

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t



#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_COLOR_ATTACHMENT0              0x8CE0
//#define GL_SRC_ALPHA

// there's probably established names for these but w/e for now
typedef void (APIENTRY * PFGL_GEN_FBO) (GLsizei n, GLuint *ids);
typedef void (APIENTRY * PFGL_BIND_FBO) (GLenum target, GLuint framebuffer);
typedef void (APIENTRY * PFGL_FBO_TEX2D) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY * PFGL_BLIT_FBO) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRY * PFGL_DEL_FBO) (GLsizei n, const GLuint * framebuffers);

static PFGL_GEN_FBO glGenFramebuffers;
static PFGL_BIND_FBO glBindFramebuffer;
static PFGL_FBO_TEX2D glFramebufferTexture2D;
static PFGL_BLIT_FBO glBlitFramebuffer;
static PFGL_DEL_FBO glDeleteFramebuffers;



// ok place for these??
static GLuint tex;
static GLuint tex2;
static GLuint readFboId = 0;



void MsgBox(char* s) {
    MessageBox(0, s, "vid player", MB_OK);
}



void DisplayAudioBuffer(u32 *buf, int wid, int hei, float *audio, int audioLen)
{
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    for (int x = 0; x < wid; x++)
    {
        float audioSample = audio[x*5];
        float audioScaled = audioSample * hei/2;
        audioScaled += hei/2;
        for (int y = 0; y < hei; y++)
        {
            if (y < hei/2) {
                if (y < audioScaled) r = 0;
                else r = 255;
            } else {
                if (y > audioScaled) r = 0;
                else r = 255;
            }
            buf[x + y*wid] = (r<<16) | (g<<8) | (b<<0) | (0xff<<24);
        }
    }
    // for (int i = 0; i < 20; i++)
    // {
       //  char temp[256];
       //  sprintf(temp, "value: %f\n", (float)audio[i]);
       //  OutputDebugString(temp);
    // }
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
    u8 *outBuffer,
    int outBufferSize,
    double msSinceStart)
{

    // todo: support non-float format source? qwer
    SwrContext *swr = swr_alloc_set_opts(NULL,  // we're allocating a new context
                      AV_CH_LAYOUT_STEREO,  // out_ch_layout   // AV_CH_LAYOUT_STEREO  AV_CH_LAYOUT_MONO
                      AV_SAMPLE_FMT_FLT,    // out_sample_fmt
                      samples_per_second,   // out_sample_rate
                      cc->channel_layout,   // in_ch_layout
                      cc->sample_fmt,       // in_sample_fmt
                      cc->sample_rate,      // in_sample_rate
                      0,                    // log_offset
                      NULL);                // log_ctx
    swr_init(swr);
    if (!swr_is_initialized(swr)) {
        OutputDebugString("ffmpeg: Audio resampler has not been properly initialized\n");
        return -1;
    }

        // to actually resample...
        // see https://rodic.fr/blog/libavcodec-tutorial-decode-audio-file/
        // // resample frames
        // double* buffer;
        // av_samples_alloc((uint8_t**) &buffer, NULL, 1, frame->nb_samples, AV_SAMPLE_FMT_DBL, 0);
        // int frame_count = swr_convert(swr, (uint8_t**) &buffer, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples);
        // // append resampled frames to data
        // *data = (double*) realloc(*data, (*size + frame->nb_samples) * sizeof(double));
        // memcpy(*data + *size, buffer, frame_count * sizeof(double));
        // *size += frame_count;



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

                    int additional_bytes = frame->nb_samples * av_get_bytes_per_sample(cc->sample_fmt);


                    // keep a check here so we don't overflow outBuffer??
                    // (ie, in case we guessed when to quit wrong below)
                    // if (bytes_written+additional_bytes > outBufferSize)
                    // {
                    //  return bytes_written;
                    // }


                    double msToPlayFrame = 1000 * frame->pts *
                        fc->streams[streamIndex]->time_base.num /
                        fc->streams[streamIndex]->time_base.den;

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


        // // resample frames
        // // double* raw_channels;
        // // av_samples_alloc((u8**) &raw_channels, NULL, 1, frame->nb_samples, AV_SAMPLE_FMT_DBL, 0);
        // int frame_count = swr_convert(
        //     swr,
        //     (u8**)&outBuffer,
        //     frame->nb_samples,
        //     (const u8**)&frame->extended_data,
        //     frame->nb_samples);

        // // append resampled frames to data
        // *data = (double*) realloc(*data, (*size + frame->nb_samples) * sizeof(double));
        // memcpy(*data + *size, buffer, frame_count * sizeof(double));
        // *size += frame_count;


                    // todo: this will only work with 2 channel planar float -> 2 chan interleaved float
                    // better would be a resampler?
                    float *out = (float*)outBuffer;
                    float *inL = (float*)frame->data[0];
                    float *inR = (float*)frame->data[1];
                    int j = 0;
                    for (int i = 0; i < frame->nb_samples; i++) // todo: need all samples
                    {
                        out[j++] = inL[i];
                        out[j++] = inR[i];
                    }

                    // old method with no resampling.. (dropped R channel)
                    // memcpy(outBuffer, frame->data[0], additional_bytes);

                    additional_bytes = j*sizeof(float);
                    outBuffer+=additional_bytes;
                    bytes_written+=additional_bytes;


                    // now try to guess when we're done based on the size of the last frame
                    if (bytes_written+additional_bytes > outBufferSize)
                    {
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
                av_frame_unref(frame);  // clear allocs made by avcodec_decode_audio4 ?

                // don't this right, because we may be pulling a second frame from it?
                // av_packet_unref(&decodingPacket);
            }

            // don't need because it's just a shallow copy of readingP??
            // av_free_packet(&decodingPacket);
            // av_packet_unref(&decodingPacket); // crash?? need to reuse over multi frames
        }
        av_packet_unref(&readingPacket);  // clear allocs made by av_read_frame ?
    }

    // Some codecs will cause frames to be buffered up in the decoding process. If the CODEC_CAP_DELAY flag
    // is set, there can be buffered up frames that need to be flushed, so we'll do that
    if (cc->codec->capabilities & CODEC_CAP_DELAY)
    {
        assert(false);
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
    return bytes_written; // ever get here?
}

bool GetNextVideoFrame(
    AVFormatContext *fc,
    AVCodecContext *cc,
    SwsContext *sws_context,
    int streamIndex,
    AVFrame *outFrame,
    double msSinceStart)
{
    AVPacket packet;

    AVFrame *frame = av_frame_alloc();  // just metadata

    if (!frame) { MsgBox("ffmpeg: Couldn't alloc frame."); return false; }

                // char temp2[123];
                // sprintf(temp2, "time_base %i / %i\n",
                //         fc->streams[streamIndex]->time_base.num,
                //         fc->streams[streamIndex]->time_base.den
                //         );
                // OutputDebugString(temp2);


    bool skipped_a_frame_already = false;

    while(av_read_frame(fc, &packet) >= 0)
    {
        if (packet.stream_index == streamIndex)
        {
            int frame_finished;
            avcodec_decode_video2(cc, frame, &frame_finished, &packet);

            if (frame_finished)
            {

                // todo: is variable frame rate possible? (eg some frames shown twice?)
                // todo: is it possible to get these not in pts order?

                // char temp[123];
                // sprintf(temp, "frame->pts %lli\n", inFrame->pts);
                // OutputDebugString(temp);

                double msToPlayFrame = 1000 * frame->pts /
                    fc->streams[streamIndex]->time_base.den;

                // char zxcv[123];
                // sprintf(zxcv, "msToPlayVideoFrame: %.1f msSinceStart: %.1f\n",
                //         msToPlayFrame,
                //         msSinceStart
                //         );
                // OutputDebugString(zxcv);

                // todo: this should be somewhere else, also how to estimate?
                // and is there any video latency? just research how to properly sync
                //asdfasdf
                // double msAudioLatencyEstimate = 50;

                // this feels just a bit early? maybe we should double it? or more?
                double msAudioLatencyEstimate = 1024.0 / samples_per_second * 1000.0;
                msAudioLatencyEstimate *= 2; // feels just about right todo: could measure with screen recording?


                // skip frame if too far off
                double msDelayAllowed = 20;
                if (msToPlayFrame +
                    msAudioLatencyEstimate +
                    msDelayAllowed <
                    msSinceStart &&
                    !skipped_a_frame_already)
                {
                    OutputDebugString("skipped a frame\n");
                    skipped_a_frame_already = true;

                    // seems like we'd want this here right?
                    av_packet_unref(&packet);
                    av_frame_unref(frame);

                    continue;
                }

                sws_scale(
                    sws_context,
                    (u8**)frame->data,
                    frame->linesize,
                    0,
                    frame->height,
                    outFrame->data,
                    outFrame->linesize);
            }

            // as far as i can tell, these need to be freed before leaving
            // AND they need to be unref'd before every use below
            av_free_packet(&packet);
            av_frame_free(&frame);

            return true; // todo: or only when frame_finished??
        }
        // call these before reuse in avcodec_decode_video2 or av_read_frame
        av_packet_unref(&packet);
        av_frame_unref(frame);
    }

    return false;
}






struct StreamAV
{
    int index;
    AVCodecContext *codecContext;
    // AVCodec *codec;
};

struct VideoFile
{
    AVFormatContext *vfc;
    AVFormatContext *afc;  // kind of a hack? to read audio and video in sep loops
    StreamAV video;
    StreamAV audio;
};

void InitAV()
{
    av_register_all();  // all formats and codecs
}

AVCodecContext *OpenAndFindCodec(AVFormatContext *fc, int streamIndex)
{
    AVCodecContext *orig = fc->streams[streamIndex]->codec;
    AVCodec *codec = avcodec_find_decoder(orig->codec_id);
    AVCodecContext *result = avcodec_alloc_context3(codec);
    if (!codec)
        { MsgBox("ffmpeg: Unsupported codec. Yipes."); return false; }
    if (avcodec_copy_context(result, orig) != 0)
        { MsgBox("ffmpeg: Codec context copy failed."); return false; }
    if (avcodec_open2(result, codec, 0) < 0)
        { MsgBox("ffmpeg: Couldn't open codec."); return false; }
    return result;
}

VideoFile OpenVideoFileAV(char *filepath)
{
    VideoFile file;

    file.vfc = 0;  // = 0 or call avformat_alloc_context before opening?
    file.afc = 0;  // = 0 or call avformat_alloc_context before opening?

    int open_result1 = avformat_open_input(&file.vfc, filepath, 0, 0);
    int open_result2 = avformat_open_input(&file.afc, filepath, 0, 0);
    if (open_result1 != 0 || open_result2 != 0)
    {
        char averr[1024];
        av_strerror(open_result1, averr, 1024);
        char msg[2048];
        sprintf(msg, "ffmpeg: Can't open file: %s\n", averr);
        MsgBox(msg);
        return file;
    }

    // populate fc->streams
    if (avformat_find_stream_info(file.vfc, 0) < 0 ||
        avformat_find_stream_info(file.afc, 0) < 0)
    {
        MsgBox("ffmpeg: Can't find stream info in file.");
        return file;
    }

    av_dump_format(file.vfc, 0, INPUT_FILE, 0);


    // find first video and audio stream
    // todo: use av_find_best_stream?
    file.video.index = -1;
    file.audio.index = -1;
    for (int i = 0; i < file.vfc->nb_streams; i++)
    {
        if (file.vfc->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            if (file.video.index == -1)
                file.video.index = i;
        }
        if (file.vfc->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
        {
            if (file.audio.index == -1)
                file.audio.index = i;
        }
    }
    if (file.video.index == -1)
    {
        MsgBox("ffmpeg: No video stream found.");
        return file;  // todo: support missing streams
    }
    if (file.audio.index == -1)
    {
        MsgBox("ffmpeg: No audio stream found.");
        return file;  // todo: support missing streams
    }

    file.video.codecContext = OpenAndFindCodec(file.vfc, file.video.index);
    file.audio.codecContext = OpenAndFindCodec(file.afc, file.audio.index);

    return file;
}








void logSpec(SDL_AudioSpec *as) {
    char log[1024];
    sprintf(log,
        " freq______%5d\n"
        " format____%5d\n"
        " channels__%5d\n"
        " silence___%5d\n"
        " samples___%5d\n"
        " size______%5d\n\n",
        (int) as->freq,
        (int) as->format,
        (int) as->channels,
        (int) as->silence,
        (int) as->samples,
        (int) as->size
    );
    OutputDebugString(log);
}

SDL_AudioDeviceID SetupAudioSDL(AVCodecContext *audioContext)
{
    SDL_AudioSpec wanted_spec, spec;
    wanted_spec.freq = samples_per_second;//acc->sample_rate;
    wanted_spec.format = AUDIO_F32;//AUDIO_F32;//AUDIO_S16SYS;
    wanted_spec.channels = 2; //acc->channels; qwer
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024; // SDL_AUDIO_BUFFER_SIZE // estimate latency based on this?
    wanted_spec.callback = 0;  // none to set samples ourself
    wanted_spec.userdata = 0;


    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(0, 0,
        &wanted_spec, &spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);

    if (audio_device == 0)
    {
        char audioerr[256];
        sprintf(audioerr, "SDL: Failed to open audio: %s\n", SDL_GetError());
        OutputDebugString(audioerr);
        return 0;
    }

    if (wanted_spec.format != spec.format)
    {
        // try another one instead of failing?
        char audioerr[256];
        sprintf(audioerr, "SDL: Couldn't find desired format: %s\n", SDL_GetError());
        OutputDebugString(audioerr);
        return 0;
    }

    OutputDebugString("SDL: audio spec wanted:\n");
    logSpec(&wanted_spec);
    OutputDebugString("SDL: audio spec got:\n");
    logSpec(&spec);

    return audio_device;
}




void InitOpenGL(HWND window)
{
    HDC hdc = GetDC(window);

    PIXELFORMATDESCRIPTOR pixel_format = {};
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cAlphaBits = 8;

    int format_index = ChoosePixelFormat(hdc, &pixel_format);
    SetPixelFormat(hdc, format_index, &pixel_format);

    HGLRC gl_rendering_context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, gl_rendering_context); // map future gl calls to our hdc

    ReleaseDC(window, hdc);


    // seem to be context dependent? so load after it?
    glGenFramebuffers = (PFGL_GEN_FBO)wglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFGL_BIND_FBO)wglGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFGL_FBO_TEX2D)wglGetProcAddress("glFramebufferTexture2D");
    glBlitFramebuffer = (PFGL_BLIT_FBO)wglGetProcAddress("glBlitFramebuffer");
    glDeleteFramebuffers = (PFGL_DEL_FBO)wglGetProcAddress("glDeleteFramebuffers");


    // create our texture and FBO just once right?
    glGenTextures(1, &tex);
    glGenTextures(1, &tex2);

    // could create and delete each frame maybe?
    glGenFramebuffers(1, &readFboId);


}



void RenderToScreenGDI(void *memory, int sWid, int sHei, int dWid, int dHei, HWND window)
{

    HDC hdc = GetDC(window);

    RECT clientRect;
    GetClientRect(window, &clientRect);
    int winWidth = clientRect.right - clientRect.left;
    int winHeight = clientRect.bottom - clientRect.top;

    // here we clear out the edges
    // PatBlt(hdc, width, 0, winWidth-width, winHeight, BLACKNESS);
    // PatBlt(hdc, 0, height, width, winHeight-height, BLACKNESS);

    BITMAPINFO info;
    info.bmiHeader.biSize = sizeof(info);
    info.bmiHeader.biWidth = sWid;
    info.bmiHeader.biHeight = -sHei;  // negative to set origin in top left
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;


    float zoomFactor = 1;

    // this is what actually puts the buffer on the screen
    int result = StretchDIBits(
                  hdc,
                  0, 0, dWid, dHei,
                  0, 0, sWid, sHei,
                  memory,
                  &info,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    if (!result)
    {
        OutputDebugString("StretchDIBits failed!");
        // DWORD msg = GetLastError();
    }

    ReleaseDC(window, hdc);
}




// todo: resizing here or resize with ffmpeg?
void RenderToScreenGL(void *memory, int sWID, int sHEI, int dWID, int dHEI, HWND window, float proportion)
{
    HDC hdc = GetDC(window);

    glBindTexture(GL_TEXTURE_2D, tex);

    // update our texture with new data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sWID, sHEI,
                 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, memory);


    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFboId);

    // should this only be needed once?
    // (note texture needs to be bound at least once before calling this though??)
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);

    // still not quite sure how this gets the memory bytes to the screen...
    // i guess TexImage puts mem into tex2D
    // then framebufferTex ties readfbo to that tex2d
    // then this blit writes the readfbo to the screen (our bound hdc)
    glBlitFramebuffer(0, 0, sWID, sHEI,
                      0, dHEI, dWID, 0,   // flip vertically
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);


    // // progress bar
    // int pos = (int)(proportion * (double)dWID);
    // u32 red = 0xffff0000;
    // glBindTexture(GL_TEXTURE_2D, tex2);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
    //              0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &red);
    // glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
    //                        GL_TEXTURE_2D, tex2, 0);
    // glBlitFramebuffer(0, 0, 1, 1,
    //                   0, 12, pos, 37,
    //                   GL_COLOR_BUFFER_BIT, GL_LINEAR);
    // u32 gray = 0x99999999;
    // glBindTexture(GL_TEXTURE_2D, tex2);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
    //              0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &gray);
    // glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
    //                        GL_TEXTURE_2D, tex2, 0);
    // glBlitFramebuffer(0, 0, 1, 1,
    //                   pos, 12, dWID, 37,
                      // GL_COLOR_BUFFER_BIT, GL_LINEAR);


    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // i think also flushes things?
    // glDeleteFramebuffers(1, &readFboId);


    SwapBuffers(hdc);
    // glFinish();  // should never need this?

    ReleaseDC(window, hdc);
}


u32 AlphaBlend(u32 p1, u32 p2)
{
    static const int AMASK = 0xFF000000;
    static const int RBMASK = 0x00FF00FF;
    static const int GMASK = 0x0000FF00;
    static const int AGMASK = AMASK | GMASK;
    static const int ONEALPHA = 0x01000000;
    unsigned int a = (p1 & AMASK) >> 24;
    unsigned int na = 255 - a;
    unsigned int rb = ((a * (p1 & RBMASK)) + (na * (p2 & RBMASK))) >> 8;
    unsigned int ag = (a * ((p1 & AGMASK) >> 8)) + (na * (ONEALPHA | ((p2 & GMASK) >> 8)));
    return ((rb & RBMASK) | (ag & AGMASK));
}

void BlendProgressBar(u32 *pixels, int width, int height, double proportion)
{
    int pos = (int)(proportion * (double)width);
    for (int x = 0; x < width; x++)
    {
        // for (int y = 0; y < height; y++)
        for (int y = height-37; y < height-12; y++)
        {
            if (x < pos)
                pixels[x + y*width] = AlphaBlend(0xffff0000, pixels[x + y*width]);
            else
                pixels[x + y*width] = AlphaBlend(0x66999999, pixels[x + y*width]);
        }
    }
}



struct Timer
{
    LARGE_INTEGER starting_ticks;    // .QuadPart to get number as int64
    LARGE_INTEGER ticks_per_second;  // via QueryPerformanceFrequency
    LARGE_INTEGER ticks_last_frame;
    bool started = false;
    void Start()
    {
        started = true;
        QueryPerformanceFrequency(&ticks_per_second);
        QueryPerformanceCounter(&starting_ticks);
    }
    double MsElapsedBetween(LARGE_INTEGER old_ticks, LARGE_INTEGER ticks_now)
    {
        int64_t ticks_elapsed = ticks_now.QuadPart - old_ticks.QuadPart;
        ticks_elapsed *= 1000; // tip from msdn: covert to ms before to help w/ precision
        double delta_ms = (double)ticks_elapsed / (double)ticks_per_second.QuadPart;
        return delta_ms;
    }
    double MsElapsedSince(LARGE_INTEGER old_ticks)
    {
        LARGE_INTEGER ticks_now;
        QueryPerformanceCounter(&ticks_now);
        return MsElapsedBetween(old_ticks, ticks_now);;
    }
    double MsSinceStart()
    {
        if (!started) {
            OutputDebugString("Timer: Tried to get time without starting first.\n");
            return 0;
        }
        return MsElapsedSince(starting_ticks);
    }

    // these feel a little state-y
    double MsSinceLastFrame()
    {
        return MsElapsedSince(ticks_last_frame);
    }
    void EndFrame()
    {
        QueryPerformanceCounter(&ticks_last_frame);
    }

};







void RenderWeird(u32* buf, int width, int height, int t)
{
    u8 r = 0;
    u8 g = (sin((float)t/25.f)/2 + 0.5) * 256;
    u8 b = 0;
    for (int y = 0; y < height; y++)
    {
        r = sin(y*M_PI / height) * 255;
        for (int x = 0; x < width; x++)
        {
            b = sin(x*M_PI / width) * 255;
            buf[x + y*width] = (r<<16) | (g<<8) | (b<<0) | (0xff<<24);
        }
    }
}


//fdsa
static double duration;
static double elapsed;
static double percent;
static Timer timer;
static Timer audioTimer;
static SDL_AudioDeviceID audio_device;
static int desired_bytes_in_queue;
static int bytes_in_buffer;
static VideoFile video_file;
static void *sound_buffer;
static struct SwsContext *sws_context;
static AVFrame *frame_output;
static HWND window;
static u8 *vid_buffer;
static int vidWID;
static int vidHEI;
static int winWID;
static int winHEI;
static double targetMsPerFrame;
static u32 *secondary_buf;




// seems like we need to keep this sep if we want to
// use HTCAPTION to drag the window around
void Update()
{


        // WHY NOT TIME FIRST

        // if (dt < targetMsPerFrame)
        //     return;




        elapsed = audioTimer.MsSinceStart() / 1000.0;
        percent = elapsed/duration;

        char durbuf[123];
        sprintf(durbuf, "elapsed: %.2f  /  %.2f  (%.f%%)\n", elapsed, duration, percent*100);
        OutputDebugString(durbuf);




        // SOUND

        static bool playingAudio = false;
        if (!playingAudio)
        {
            SDL_PauseAudioDevice(audio_device, 0);
            audioTimer.Start();
            playingAudio = true;
        }

        int bytes_left_in_queue = SDL_GetQueuedAudioSize(audio_device);
            // char msg[256];
            // sprintf(msg, "bytes_left_in_queue: %i\n", bytes_left_in_queue);
            // OutputDebugString(msg);


        int wanted_bytes = desired_bytes_in_queue - bytes_left_in_queue;
            // char msg3[256];
            // sprintf(msg3, "wanted_bytes: %i\n", wanted_bytes);
            // OutputDebugString(msg3);

        if (wanted_bytes >= 0)
        {
            if (wanted_bytes > bytes_in_buffer)
            {
                // char errq[256];
                // sprintf(errq, "want to queue: %i, but only %i in buffer\n", wanted_bytes, bytes_in_buffer);
                // OutputDebugString(errq);

                wanted_bytes = bytes_in_buffer;
            }

            // ideally a little bite of sound, every frame
            // todo: how to sync this right, pts dts?
            int bytes_queued_up = GetNextAudioFrame(
                video_file.afc,
                video_file.audio.codecContext,
                video_file.audio.index,
                (u8*)sound_buffer,
                wanted_bytes,
                audioTimer.MsSinceStart());

            if (bytes_queued_up > 0)
            {
                if (SDL_QueueAudio(audio_device, sound_buffer, wanted_bytes) < 0)
                {
                    char audioerr[256];
                    sprintf(audioerr, "SDL: Error queueing audio: %s\n", SDL_GetError());
                    OutputDebugString(audioerr);
                }
                   // char msg2[256];
                   // sprintf(msg2, "bytes_queued_up: %i\n", bytes_queued_up);
                   // OutputDebugString(msg2);
            }
        }





        // VIDEO

        double msSinceAudioStart = audioTimer.MsSinceStart();

        GetNextVideoFrame(
            video_file.vfc,
            video_file.video.codecContext,
            sws_context,
            video_file.video.index,
            frame_output,
            msSinceAudioStart);

        BlendProgressBar((u32*)vid_buffer, vidWID, vidHEI, percent);
// }
        RenderToScreenGL((void*)vid_buffer, vidWID, vidHEI, winWID, winHEI, window, percent);
        // RenderToScreenGDI((void*)buffer, vidWID, vidHEI, winWID, winHEI, window);


// }

        // // DisplayAudioBuffer_FLOAT(secondary_buf, vidWID, vidHEI,
        // //                    (float*)sound_buffer, bytes_in_buffer);
        // static int increm = 0;
        // RenderWeird(secondary_buf, vidWID, vidHEI, increm++);
        // RenderToScreenGL((void*)secondary_buf, vidWID, vidHEI, winWID, winHEI, window);
        // RenderToScreenGDI((void*)secondary_buf, vidWID, vidHEI, window);




        // HIT FPS

        // something seems off with this... ?
        double dt = timer.MsSinceLastFrame();

        if (dt < targetMsPerFrame)
        {
            double msToSleep = targetMsPerFrame - dt;
            Sleep(msToSleep);
            while (dt < targetMsPerFrame)  // is this weird?
            {
                dt = timer.MsSinceLastFrame();
            }
            // char msg[256]; sprintf(msg, "fps: %.5f\n", 1000/dt); OutputDebugString(msg);
            // char msg[256]; sprintf(msg, "ms: %.5f\n", dt); OutputDebugString(msg);
        }
        else
        {
            // missed fps target
            char msg[256];
            sprintf(msg, "!! missed fps !! target ms: %.5f, frame ms: %.5f\n",
                    targetMsPerFrame, dt);
            OutputDebugString(msg);
        }
        timer.EndFrame();  // make sure to call for MsSinceLastFrame() to work.. feels weird

}



// u8 *ReAllocScreenBuffer(u8 *buf, int wid, int hei)
// {
//     if (buf) av_free(buf);
//     // int numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, wid, hei);
//     // return (u8*)av_malloc(numBytes * sizeof(u8));
//     int numBytes = sizeof(u32)*sizeof(u32) * wid * hei;
//     return (u8*)av_malloc(numBytes);
// }



#define ID_EXIT 1001
#define ID_PAUSE 1002


RECT MainRect;
POINT point;
POINT curpoint;


static bool appRunning = true;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_CLOSE: {
            appRunning = false;
        } break;


        case WM_SIZE: {
            winWID = LOWORD(lParam);
            winHEI = HIWORD(lParam);
            return 0;
        }


        case WM_NCHITTEST: {

            RECT win;
            if (!GetWindowRect(hwnd, &win))
                return HTNOWHERE;

            POINT pos = { LOWORD(lParam), HIWORD(lParam) }; // todo: won't work on multiple monitors! use GET_X_LPARAM
            POINT pad = { GetSystemMetrics(SM_CXFRAME), GetSystemMetrics(SM_CYFRAME) };

            bool left   = pos.x < win.left   + pad.x;
            bool right  = pos.x > win.right  - pad.x -1;  // isn't win.right 1 pixel beyond window?
            bool top    = pos.y < win.top    + pad.y;
            bool bottom = pos.y > win.bottom - pad.y -1;

            // RenderToScreenGL((void*)vid_buffer, vidWID, vidHEI, winWID, winHEI, window);

            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left)            return HTLEFT;
            if (right)           return HTRIGHT;
            if (top)             return HTTOP;
            if (bottom)          return HTBOTTOM;

            return HTCAPTION;
        } break;

        case WM_PAINT: {
            Update();
            // PAINTSTRUCT ps;
            // BeginPaint(hwnd, &ps);
            // EndPaint(hwnd, &ps);
            // return DefWindowProc(hwnd, message, wParam, lParam);
            return 0;
        } break;



        case WM_RBUTTONDOWN:      // rclicks in client area (HTCLIENT), probably won't ever fire
        case WM_NCRBUTTONDOWN: {  // rclick in non-client area (everywhere due to our WM_NCHITTEST method)
            OutputDebugString("HERE");
            HMENU hPopupMenu = CreatePopupMenu();
            InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_EXIT, L"Exit");
            InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
            InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_PAUSE, L"Pause/Play");
            SetForegroundWindow(hwnd);
            // TrackPopupMenu(hPopupMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, 0, 0, 0, hwnd, NULL);
            TrackPopupMenu(hPopupMenu, 0, LOWORD(lParam), HIWORD(lParam), 0, hwnd, NULL);
        } break;
        case WM_COMMAND: {
            switch (wParam)
            {
                case ID_EXIT: {
                    appRunning = false;
                } break;
            }
        } break;

    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}



int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{


    // TIMER

    if (timeBeginPeriod(1) != TIMERR_NOERROR) {
        char err[256];
        sprintf(err, "Unable to set resolution of Sleep to 1ms");
        OutputDebugString(err);
    }

    // Timer timer;
    timer.Start();




    // FFMPEG - load file right away to make window the same size

    InitAV();

    // VideoFile
    video_file = OpenVideoFileAV(INPUT_FILE);



    // MAKE NOTE OF VIDEO LENGTH

    // use this?
    assert(video_file.vfc->start_time==0);
    // char qwer2[123];
    // sprintf(qwer2, "start: %lli\n", start_time);
    // OutputDebugString(qwer2);

    duration = (double)video_file.vfc->duration / (double)AV_TIME_BASE; // not quite accurate??
    char qwer[123];
    sprintf(qwer, "duration: %f\n", duration);
    OutputDebugString(qwer);
    elapsed = 0;



    // SET FPS BASED ON LOADED VIDEO

    char vidfps[123];
    sprintf(vidfps, "video frame rate: %i / %i\nticks_per_frame: %i\n",
        video_file.video.codecContext->time_base.num,
        video_file.video.codecContext->time_base.den,
        video_file.video.codecContext->ticks_per_frame
    );
    OutputDebugString(vidfps);

    double targetFPS = (video_file.video.codecContext->time_base.den /
                        video_file.video.codecContext->time_base.num) /
                        video_file.video.codecContext->ticks_per_frame;
    // double
    targetMsPerFrame = 1000 / targetFPS;






    // WINDOW

    // register wndproc
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "best class";
    if (!RegisterClass(&wc)) { MsgBox("RegisterClass failed."); return 1; }

    // const int
    vidWID = video_file.video.codecContext->width;
    // const int
    vidHEI = video_file.video.codecContext->height;
    RECT neededRect = {};
    winWID = vidWID;//960;
    winHEI = vidHEI;//720;
    neededRect.right = winWID;//WID; //960;
    neededRect.bottom = winHEI;//HEI; //720;
    // adjust window size based on desired client size
    // AdjustWindowRectEx(&neededRect, WS_OVERLAPPEDWINDOW, 0, 0);

    // transparency options
    bool SEE_THROUGH   = false;
    bool CLICK_THROUGH = false;
    // bool SEE_THROUGH   = true;
    // bool CLICK_THROUGH = true;

    DWORD exStyles = 0;
    if (SEE_THROUGH)                  exStyles  = WS_EX_LAYERED;
    if (CLICK_THROUGH)                exStyles |= WS_EX_TRANSPARENT;
    if (SEE_THROUGH || CLICK_THROUGH) exStyles |= WS_EX_TOPMOST;

    // HWND
    window = CreateWindowEx(
        exStyles,
        wc.lpszClassName, "vid player",
        WS_POPUP | WS_VISIBLE,  // ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX
        CW_USEDEFAULT, CW_USEDEFAULT,
        neededRect.right - neededRect.left, neededRect.bottom - neededRect.top,
        0, 0, hInstance, 0);

    if (!window)
    {
        MsgBox("Couldn't open window.");
    }

    // set window transparent (if styles above are right)
    if (SEE_THROUGH || CLICK_THROUGH)
        SetLayeredWindowAttributes(window, 0, 122, LWA_ALPHA);



    // OPENGL

    InitOpenGL(window);


    // temp gfx
    // u32 *
    secondary_buf = (u32*)malloc(vidWID * vidHEI * sizeof(u32)*sizeof(u32));
    RenderWeird(secondary_buf, vidWID, vidHEI, 0);


    // SDL, for sound atm

    // if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    if (SDL_Init(SDL_INIT_AUDIO))
    {
        char err[256];
        sprintf(err, "SDL: Couldn't initialize: %s", SDL_GetError());
        MsgBox(err);
        return -1;
    }
    // SDL_AudioDeviceID
    audio_device = SetupAudioSDL(video_file.audio.codecContext);

    AVCodecContext *acc = video_file.audio.codecContext;

    // try to keep this full
    int audio_channels = acc->channels;

    // how big of chunks do we want to decode and queue up at a time
    int buffer_seconds = 2; // no reason not to just keep this big, right?
    // int buffer_seconds = int(targetMsPerFrame * 1000 * 5); //10 frames ahead
    int samples_in_buffer = acc->sample_rate * buffer_seconds;
    // int
    bytes_in_buffer = samples_in_buffer * sizeof(float) * audio_channels;
    // void *
    sound_buffer = (void*)malloc(bytes_in_buffer);

    // how far ahead do we want our sdl queue to be? (we'll try to keep it full)
    int desired_seconds_in_queue = 4; // how far ahead in seconds do we queue sound data?
    int desired_samples_in_queue = desired_seconds_in_queue * acc->sample_rate;
    // int
    desired_bytes_in_queue = desired_samples_in_queue * sizeof(float) * audio_channels;

    // track how long we've been playing audio?
    // better way to sync??
    // Timer
    audioTimer;



    // MORE FFMPEG

    // AVFrame *
    frame_output = av_frame_alloc();  // just metadata

    if (!frame_output)
        { MsgBox("ffmpeg: Couldn't alloc frame."); return -1; }


    // actual mem for frame
    int numBytes = avpicture_get_size(AV_PIX_FMT_RGB32,
        // video_file.video.codecContext->width,
        // video_file.video.codecContext->height);
        vidWID,
        vidHEI);
    // u8 *
    vid_buffer = (u8*)av_malloc(numBytes * sizeof(u8)); // is this right?

    // frame is now using buffer memory
    avpicture_fill((AVPicture *)frame_output, vid_buffer, AV_PIX_FMT_RGB32,
        vidWID,
        vidHEI);

    // for converting between frames i think
    // struct SwsContext *
    sws_context = 0;
    sws_context = sws_getContext(
        video_file.video.codecContext->width,
        video_file.video.codecContext->height,
        video_file.video.codecContext->pix_fmt,
        vidWID,
        vidHEI,
        AV_PIX_FMT_RGB32, //(AVPixelFormat)frame_output->format,
        SWS_BILINEAR,
        0, 0, 0);




    // av_frame_unref() //reset for next frame




    // MAIN LOOP

    // seed our first frame dt
    timer.EndFrame();

    while (appRunning)
    {
        MSG Message;
        if (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        // OutputDebugString("UPDATE\n");
        Update();

    }


    return 0;
}