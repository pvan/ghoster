#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>


// for drag drop
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")


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

int progressBarH = 22;
int progressBarB = 12;
double msOfLastMouseMove = -1000;
bool drawProgressBar = false;
double progressBarDisapearAfterThisManyMSOfInactivity = 1000;



// char *INPUT_FILE = "D:/Users/phil/Desktop/sync3.mp4";
char *INPUT_FILE = "D:/Users/phil/Desktop/sync1.mp4";
// char *INPUT_FILE = "D:/Users/phil/Desktop/test4.mp4";
// char *INPUT_FILE = "D:/Users/phil/Desktop/test.mp4";
// char *INPUT_FILE = "D:/Users/phil/Desktop/test3.avi";
// char *INPUT_FILE = "D:/Users/phil/Desktop/test.3gp"; // proper audio resampler for this to work




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
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_ARRAY_BUFFER                   0x8892
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_STATIC_DRAW                    0x88E4
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_TEXTURE0                       0x84C0

// or just dl glext.h
typedef void (APIENTRY * PFGL_GEN_FBO) (GLsizei n, GLuint *ids);
typedef void (APIENTRY * PFGL_BIND_FBO) (GLenum target, GLuint framebuffer);
typedef void (APIENTRY * PFGL_FBO_TEX2D) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY * PFGL_BLIT_FBO) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRY * PFGL_DEL_FBO) (GLsizei n, const GLuint * framebuffers);
typedef GLuint (APIENTRY * PFGL_CS) (GLenum shaderType);
typedef void (APIENTRY * PFGL_SS) (GLuint shader, GLsizei count, const char **string, const GLint *length);
typedef GLuint (APIENTRY * PFGL_CP) (void);
typedef void (APIENTRY * PFGL_AS) (GLuint program, GLuint shader);
typedef void (APIENTRY * PFGL_LP) (GLuint program);
typedef void (APIENTRY * PFGL_VA) (GLsizei n, GLuint *arrays);
typedef void (APIENTRY * PFGL_GB) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY * PFGL_UP) (GLuint program);
typedef void (APIENTRY * PFGL_BB) (GLenum target, GLuint buffer);
typedef void (APIENTRY * PFGL_BA) (GLuint array);
typedef void (APIENTRY * PFGL_BD) (GLenum target, ptrdiff_t size, const GLvoid *data, GLenum usage);
typedef void (APIENTRY * PFGL_BS) (GLenum target, int* offset, ptrdiff_t size, const GLvoid * data);
typedef void (APIENTRY * PFGL_CMS) (GLuint shader);
typedef void (APIENTRY * PFGL_SL) (GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
typedef void (APIENTRY * PFGL_GS) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY * PFGL_PL) (GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
typedef void (APIENTRY * PFGL_GP) (GLuint shader, GLenum pname, GLint *params);
typedef GLint (APIENTRY * PFGL_GUL) (GLuint program, const char *name);
typedef GLint (APIENTRY * PFGL_GAL) (GLuint program, const char *name);
typedef void (APIENTRY * PFGL_VAP) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY * PFGL_EVA) (GLuint index);
typedef void (APIENTRY * PFGL_UNI) (GLint location, GLint v0);
typedef void (APIENTRY * PFGL_UNIF) (GLint location, GLfloat v0);
typedef void (APIENTRY * PFGL_TEX) (GLenum texture);


static PFGL_GEN_FBO glGenFramebuffers;
static PFGL_BIND_FBO glBindFramebuffer;
static PFGL_FBO_TEX2D glFramebufferTexture2D;
static PFGL_BLIT_FBO glBlitFramebuffer;
static PFGL_DEL_FBO glDeleteFramebuffers;
static PFGL_CS glCreateShader;
static PFGL_SS glShaderSource;
static PFGL_CP glCreateProgram;
static PFGL_AS glAttachShader;
static PFGL_LP glLinkProgram;
static PFGL_VA glGenVertexArrays;
static PFGL_GB glGenBuffers;
static PFGL_UP glUseProgram;
static PFGL_BB glBindBuffer;
static PFGL_BA glBindVertexArray;
static PFGL_BD glBufferData;
static PFGL_BS glBufferSubData;
static PFGL_CMS glCompileShader;
static PFGL_SL glGetShaderInfoLog;
static PFGL_GS glGetShaderiv;
static PFGL_PL glGetProgramInfoLog;
static PFGL_GP glGetProgramiv;
static PFGL_GUL glGetUniformLocation;
static PFGL_GAL glGetAttribLocation;
static PFGL_VAP glVertexAttribPointer;
static PFGL_EVA glEnableVertexAttribArray;
static PFGL_UNI glUniform1i;
static PFGL_UNIF glUniform1f;
static PFGL_TEX glActiveTexture;



static bool appRunning = true;
static bool vid_paused = false;
static bool vid_was_paused = false;



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

    // // todo: support non-float format source? qwer
    // SwrContext *swr = swr_alloc_set_opts(NULL,  // we're allocating a new context
    //                   AV_CH_LAYOUT_STEREO,  // out_ch_layout   // AV_CH_LAYOUT_STEREO  AV_CH_LAYOUT_MONO
    //                   AV_SAMPLE_FMT_FLT,    // out_sample_fmt
    //                   samples_per_second,   // out_sample_rate
    //                   cc->channel_layout,   // in_ch_layout
    //                   cc->sample_fmt,       // in_sample_fmt
    //                   cc->sample_rate,      // in_sample_rate
    //                   0,                    // log_offset
    //                   NULL);                // log_ctx
    // swr_init(swr);
    // if (!swr_is_initialized(swr)) {
    //     OutputDebugString("ffmpeg: Audio resampler has not been properly initialized\n");
    //     return -1;
    // }

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

                    additional_bytes = j*sizeof(float);
                    outBuffer+=additional_bytes;
                    bytes_written+=additional_bytes;


                    // old method with no resampling.. (dropped R channel)
                    // memcpy(outBuffer, frame->data[0], additional_bytes);
                    // outBuffer+=additional_bytes;
                    // bytes_written+=additional_bytes;



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
                    // OutputDebugString("skipped a frame\n");
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






// adapted from ffmeg dump.c
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

SDL_AudioDeviceID CreateSDLAudioDeviceFor(AVCodecContext *audioContext)
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



typedef void (*GetLogFunc)(GLuint, GLsizei, GLsizei *, char *);
typedef void (*GetParamFunc)(GLuint, GLenum, GLint *);
void shader_error_check(GLuint object, const char *kind, GetLogFunc getLog, GetParamFunc getParam, GLenum param)
{
    char log[1024];
    GLsizei length;
    getLog(object, 1024, &length, log);

    GLint status;
    getParam(object, param, &status);


    if (length || status == GL_FALSE)
    {
        if (length == 0)
        {
            sprintf(log, "No error log: forgot to compile?");
        }
        char err[1024];
        sprintf(err, "\n\n%s log:\n%s\n\n", kind, log);
        OutputDebugString(err);
    }


    if (status == GL_FALSE)
    {
        exit(1);
    }
}


void check_gl_error(char *lastCall)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        char errbuf[123];
        sprintf(errbuf,"GL ERR: 0x%x (%s)\n", err, lastCall);
        OutputDebugString(errbuf);
    }
}

static GLuint vao;
static GLuint vbo;
static GLuint shader_program;
static GLuint tex;

// trick for easy shader strings
#define MULTILINE_STRING(...) #__VA_ARGS__

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
    glCreateShader = (PFGL_CS)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFGL_SS)wglGetProcAddress("glShaderSource");
    glCreateProgram = (PFGL_CP)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFGL_AS)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFGL_LP)wglGetProcAddress("glLinkProgram");
    glGenVertexArrays = (PFGL_VA)wglGetProcAddress("glGenVertexArrays");
    glGenBuffers = (PFGL_GB)wglGetProcAddress("glGenBuffers");
    glUseProgram = (PFGL_UP)wglGetProcAddress("glUseProgram");
    glBindBuffer = (PFGL_BB)wglGetProcAddress("glBindBuffer");
    glBindVertexArray = (PFGL_BA)wglGetProcAddress("glBindVertexArray");
    glBufferData = (PFGL_BD)wglGetProcAddress("glBufferData");
    glBufferSubData = (PFGL_BS)wglGetProcAddress("glBufferSubData");
    glCompileShader = (PFGL_CMS)wglGetProcAddress("glCompileShader");
    glGetShaderInfoLog = (PFGL_SL)wglGetProcAddress("glGetShaderInfoLog");
    glGetShaderiv = (PFGL_GS)wglGetProcAddress("glGetShaderiv");
    glGetProgramInfoLog = (PFGL_PL)wglGetProcAddress("glGetProgramInfoLog");
    glGetProgramiv = (PFGL_GP)wglGetProcAddress("glGetProgramiv");
    glGetUniformLocation = (PFGL_GUL)wglGetProcAddress("glGetUniformLocation");
    glGetAttribLocation = (PFGL_GUL)wglGetProcAddress("glGetAttribLocation");
    glVertexAttribPointer = (PFGL_VAP)wglGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFGL_EVA)wglGetProcAddress("glEnableVertexAttribArray");
    glUniform1i = (PFGL_UNI)wglGetProcAddress("glUniform1i");
    glUniform1f = (PFGL_UNIF)wglGetProcAddress("glUniform1f");
    glActiveTexture = (PFGL_TEX)wglGetProcAddress("glActiveTexture");


    const char *vertex_shader = MULTILINE_STRING
    (
        #version 330 core \n
        layout(location = 0) in vec2 position;
        out vec2 texCoord;
        void main() {
            texCoord = position.xy*vec2(0.5,-0.5)+vec2(0.5,0.5);
            gl_Position = vec4(position, 0, 1);
        }
    );
    // OutputDebugString(vertex_shader);
    // OutputDebugString("\n");

    const char *fragment_shader = MULTILINE_STRING
    (
        #version 330 core \n
        out vec4 color;
        in vec2 texCoord;
        uniform sampler2D tex;
        uniform float alpha;
        void main()
        {
            color = texture2D(tex, texCoord);
            color.a = alpha;
        }
    );


    // compile shaders
    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, 1, &vertex_shader, 0);
    glCompileShader(vshader);
    shader_error_check(vshader, "vertex shader", glGetShaderInfoLog, glGetShaderiv, GL_COMPILE_STATUS);

    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, 1, &fragment_shader, 0);
    glCompileShader(fshader);
    shader_error_check(fshader, "fragment shader", glGetShaderInfoLog, glGetShaderiv, GL_COMPILE_STATUS);

    // create program that sitches shaders together
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vshader);
    glAttachShader(shader_program, fshader);
    glLinkProgram(shader_program);
    shader_error_check(shader_program, "program", glGetProgramInfoLog, glGetProgramiv, GL_LINK_STATUS);


    // dfdf
    glGenVertexArrays(1, &vao);
            check_gl_error("glGenVertexArrays");
    glGenBuffers(1, &vbo);
            check_gl_error("glGenBuffers");

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
        float points[2*4] = {-1,1, -1,-1, 1,-1, 1,1};
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW); //GL_DYNAMIC_DRAW
        glBindVertexArray(vao);
            GLuint loc_position = glGetAttribLocation(shader_program, "position");
            glVertexAttribPointer(loc_position, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(loc_position);
        glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);



    // create our texture
    glGenTextures(1, &tex);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}




// TODO: pull out progress bar rendering from this function
// need to render to fbo to do so?
//dfdf
void RenderToScreenGL(void *memory, int sWID, int sHEI, int dWID, int dHEI, HWND window, float proportion, bool drawProgressBar)
{
    HDC hdc = GetDC(window);



    // if window size changed.. could also call in WM_SIZE and not pass dWID here
    glViewport(0, 0, dWID, dHEI);


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sWID, sHEI,
                 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, memory);
        check_gl_error("glTexImage2D");

    GLuint tex_loc = glGetUniformLocation(shader_program, "tex");
    glUniform1i(tex_loc, 0);   // texture id of 0

    GLuint alpha_loc = glGetUniformLocation(shader_program, "alpha");
    glUniform1f(alpha_loc, 1);


    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0, 1, 1, 1);
    glUseProgram(shader_program);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);


    if (drawProgressBar)
    {
        // todo: mimic youtube show/hide
        // todo? mimic youtube size adjustment?? (looks funny full screen.. just go back to drawing onto source???)
        // fakey way to draw rects
        int pos = (int)(proportion * (double)dWID);

        glViewport(pos, progressBarB, dWID, progressBarH);
        glUniform1f(alpha_loc, 0.4);
        u32 gray = 0xaaaaaaaa;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &gray);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        glViewport(0, progressBarB, pos, progressBarH);
        glUniform1f(alpha_loc, 0.6);
        u32 red = 0xffff0000;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &red);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // unbind and cleanup

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    SwapBuffers(hdc);

    ReleaseDC(window, hdc);
}




struct Timer
{
    i64 starting_ticks;
    i64 ticks_per_second;  // set via QueryPerformanceFrequency on Start()
    i64 ticks_last_frame;
    bool started = false;
    void Start()
    {
        started = true;
        LARGE_INTEGER now;    // .QuadPart to get number as int64
        LARGE_INTEGER freq;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&freq);
        starting_ticks = now.QuadPart;
        ticks_per_second = freq.QuadPart;
    }
    void Stop()
    {
        started = false;
    }
    i64 TicksNow()
    {
        LARGE_INTEGER ticks_now;
        QueryPerformanceCounter(&ticks_now);
        return ticks_now.QuadPart;
    }
    i64 TicksElapsedSinceStart()
    {
        if (!started) return 0;
        return TicksNow() - starting_ticks;
    }
    double MsElapsedBetween(i64 old_ticks, i64 ticks_now)
    {
        int64_t ticks_elapsed = ticks_now - old_ticks;
        ticks_elapsed *= 1000; // tip from msdn: covert to ms before to help w/ precision
        double delta_ms = (double)ticks_elapsed / (double)ticks_per_second;
        return delta_ms;
    }
    double MsElapsedSince(i64 old_ticks)
    {
        return MsElapsedBetween(old_ticks, TicksNow());
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
        ticks_last_frame = TicksNow();
    }

};


struct Stopwatch
{
    Timer timer;
    bool paused = false;
    i64 ticks_elapsed_at_pause;
    i64 TicksElapsed()
    {
        if (!timer.started)
            return 0;
        if (paused)
            return ticks_elapsed_at_pause;
        return timer.TicksElapsedSinceStart();
    }
    void ResetCompletely()
    {
        timer.started = false;
        paused = false;
        ticks_elapsed_at_pause = 0;
    }
    void Start()
    {
        if (timer.started)
        {
            if (paused)
            {
                // OutputDebugString("Stopwatch: Unpausing.");
                // restart and subtract our already elapsed time
                timer.Start();
                timer.starting_ticks -= ticks_elapsed_at_pause;
            }
            else
            {
                OutputDebugString("Stopwatch: tried starting an already running stopwatch.");
            }
        }
        else
        {
            // OutputDebugString("Stopwatch: Starting for the first time.");
            timer.Start();
        }
        paused = false;
    }
    void Pause()
    {
        ticks_elapsed_at_pause = TicksElapsed();
        paused = true; // set after getting ticks or we'll get old ticks_elap value
    }
    double MsElapsed()
    {
        return (double)(TicksElapsed()*1000) / (double)timer.ticks_per_second;
    }
};




// todo: cleanup all these globals.. pass what you can and maybe an app global for some?
//fdsa
static double duration;
static double elapsed;
static double percent;
static Timer app_timer;
static SDL_AudioDeviceID audio_device;
static int desired_bytes_in_queue;
static int bytes_in_buffer;
static VideoFile loaded_video;
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
static Stopwatch audio_stopwatch;
static double vid_aspect; // feels like it'd be better to store this as a rational
static bool lock_aspect = true;


static Timer menuCloseTimer;
static bool globalContextMenuOpen;


// void Render()
// {
//     RenderToScreenGL((void*)vid_buffer, vidWID, vidHEI, winWID, winHEI, window, percent, drawProgressBar);
// }



// todo: what to do with this
void SetupSDLSound()
{
    if (sound_buffer)  // aint our first rodeo // better way to track this???
    {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_CloseAudioDevice(audio_device);
    }
    else
    {
        // should un-init if we do want to call again
        if (SDL_Init(SDL_INIT_AUDIO))
        {
            char err[256];
            sprintf(err, "SDL: Couldn't initialize: %s", SDL_GetError());
            MsgBox(err);
            //return false;
        }
    }

    audio_device = CreateSDLAudioDeviceFor(loaded_video.audio.codecContext);

    AVCodecContext *acc = loaded_video.audio.codecContext;

    // try to keep this full
    int audio_channels = acc->channels;

    // how big of chunks do we want to decode and queue up at a time
    int buffer_seconds = 2; // no reason not to just keep this big, right?
    // int buffer_seconds = int(targetMsPerFrame * 1000 * 5); //10 frames ahead
    int samples_in_buffer = acc->sample_rate * buffer_seconds;
    // int
    bytes_in_buffer = samples_in_buffer * sizeof(float) * audio_channels;

    if (sound_buffer) free(sound_buffer);  // is doing above better?
    sound_buffer = (void*)malloc(bytes_in_buffer);

    // how far ahead do we want our sdl queue to be? (we'll try to keep it full)
    int desired_seconds_in_queue = 4; // how far ahead in seconds do we queue sound data?
    int desired_samples_in_queue = desired_seconds_in_queue * acc->sample_rate;
    // int
    desired_bytes_in_queue = desired_samples_in_queue * sizeof(float) * audio_channels;
}


// todo: peruse this for memory leaks. also: weird deja vu
bool LoadVideoFile(char *path)
{

    // VideoFile
    loaded_video = OpenVideoFileAV(path);


    // set window size on video source resolution
    winWID = loaded_video.video.codecContext->width;
    winHEI = loaded_video.video.codecContext->height;
    RECT winRect;
    GetWindowRect(window, &winRect);
    //keep top left of window in same pos for now, change to keep center in same position?
    MoveWindow(window, winRect.left, winRect.top, winWID, winHEI, true);  // ever non-zero opening position? launch option?

    vid_aspect = (double)winWID / (double)winHEI;


    // MAKE NOTE OF VIDEO LENGTH

    // todo: add handling for this
    assert(loaded_video.vfc->start_time==0);
        // char qwer2[123];
        // sprintf(qwer2, "start: %lli\n", start_time);
        // OutputDebugString(qwer2);


    duration = (double)loaded_video.vfc->duration / (double)AV_TIME_BASE;
    logFormatContextDuration(loaded_video.vfc);
    elapsed = 0;

    audio_stopwatch.ResetCompletely();

//asdf

    // SET FPS BASED ON LOADED VIDEO

    double targetFPS = (loaded_video.video.codecContext->time_base.den /
                        loaded_video.video.codecContext->time_base.num) /
                        loaded_video.video.codecContext->ticks_per_frame;

    char vidfps[123];
    sprintf(vidfps, "video frame rate: %i / %i  (%.2f FPS)\nticks_per_frame: %i\n",
        loaded_video.video.codecContext->time_base.num,
        loaded_video.video.codecContext->time_base.den,
        targetFPS,
        loaded_video.video.codecContext->ticks_per_frame
    );
    OutputDebugString(vidfps);

    // double
    targetMsPerFrame = 1000 / targetFPS;






    // SDL, for sound atm

    SetupSDLSound();




    // MORE FFMPEG

    // AVFrame *
    if (frame_output) av_frame_free(&frame_output);
    frame_output = av_frame_alloc();  // just metadata

    if (!frame_output)
        { MsgBox("ffmpeg: Couldn't alloc frame."); return false; }


    // actual mem for frame
    int numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, vidWID, vidHEI);
    if (vid_buffer) av_free(vid_buffer);
    vid_buffer = (u8*)av_malloc(numBytes * sizeof(u8)); // is this right?

    // frame is now using buffer memory
    avpicture_fill((AVPicture *)frame_output, vid_buffer, AV_PIX_FMT_RGB32,
        vidWID,
        vidHEI);

    // for converting between frames i think
    // struct SwsContext *
    if (sws_context) sws_freeContext(sws_context);
    sws_context = 0;
    sws_context = sws_getContext(
        loaded_video.video.codecContext->width,
        loaded_video.video.codecContext->height,
        loaded_video.video.codecContext->pix_fmt,
        vidWID,
        vidHEI,
        AV_PIX_FMT_RGB32, //(AVPixelFormat)frame_output->format,
        SWS_BILINEAR,
        0, 0, 0);



    return true;

}


bool setSeek = false;
double seekProportion = 0;

// seems like we need to keep this sep if we want to
// use HTCAPTION to drag the window around
void Update()
{

    // TODO: option to update as fast as possible and hog cpu? hmmm

    // WHY NOT TIME FIRST

    // if (dt < targetMsPerFrame)
    //     return;


    if (globalContextMenuOpen && menuCloseTimer.started && menuCloseTimer.MsSinceStart() > 150)
    {
        globalContextMenuOpen = false;
        // OutputDebugString("SETTING false\n");
    }


    if (app_timer.MsSinceStart() - msOfLastMouseMove > progressBarDisapearAfterThisManyMSOfInactivity)
    {
        drawProgressBar = false;
    }
    else
    {
        drawProgressBar = true;
    }

    POINT mPos;
    GetCursorPos(&mPos);
    RECT winRect;
    GetWindowRect(window, &winRect);
    if (!PtInRect(&winRect, mPos))
    {
        // OutputDebugString("outside window\n");
        drawProgressBar = false;
    }


    if (vid_paused)
    {
        if (!vid_was_paused)
        {
            audio_stopwatch.Pause();
            SDL_PauseAudioDevice(audio_device, 1);
        }
    }
    else
    {
        if (vid_was_paused || !audio_stopwatch.timer.started)
        {
            audio_stopwatch.Start();
            SDL_PauseAudioDevice(audio_device, 0);
        }

        if (setSeek)
        {//asdf

            //SetupSDLSound();
            SDL_ClearQueuedAudio(audio_device);

            setSeek = false;
            int seekPos = seekProportion * loaded_video.vfc->duration;
            av_seek_frame(loaded_video.vfc, -1, seekPos, 0);
            av_seek_frame(loaded_video.afc, -1, seekPos, 0);

            double realTime = (double)seekPos / (double)AV_TIME_BASE;
            int timeTicks = realTime * audio_stopwatch.timer.ticks_per_second;
            audio_stopwatch.timer.starting_ticks = audio_stopwatch.timer.TicksNow() - timeTicks;

        }

        elapsed = audio_stopwatch.MsElapsed() / 1000.0;
        percent = elapsed/duration;
            // char durbuf[123];
            // sprintf(durbuf, "elapsed: %.2f  /  %.2f  (%.f%%)\n", elapsed, duration, percent*100);
            // OutputDebugString(durbuf);


        // SOUND

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
                loaded_video.afc,
                loaded_video.audio.codecContext,
                loaded_video.audio.index,
                (u8*)sound_buffer,
                wanted_bytes,
                audio_stopwatch.MsElapsed());

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

        double msSinceAudioStart = audio_stopwatch.MsElapsed();

        GetNextVideoFrame(
            loaded_video.vfc,
            loaded_video.video.codecContext,
            sws_context,
            loaded_video.video.index,
            frame_output,
            msSinceAudioStart);

    }
    vid_was_paused = vid_paused;


    RenderToScreenGL((void*)vid_buffer, vidWID, vidHEI, winWID, winHEI, window, percent, drawProgressBar);

    // // DisplayAudioBuffer_FLOAT(secondary_buf, vidWID, vidHEI,
    // //                    (float*)sound_buffer, bytes_in_buffer);
    // static int increm = 0;
    // RenderWeird(secondary_buf, vidWID, vidHEI, increm++);
    // RenderToScreenGL((void*)secondary_buf, vidWID, vidHEI, winWID, winHEI, window);
    // RenderToScreenGDI((void*)secondary_buf, vidWID, vidHEI, window);




    // HIT FPS

    // something seems off with this... ?
    double dt = app_timer.MsSinceLastFrame();

    if (dt < targetMsPerFrame)
    {
        double msToSleep = targetMsPerFrame - dt;
        Sleep(msToSleep);
        while (dt < targetMsPerFrame)  // is this weird?
        {
            dt = app_timer.MsSinceLastFrame();
        }
        // char msg[256]; sprintf(msg, "fps: %.5f\n", 1000/dt); OutputDebugString(msg);
        // char msg[256]; sprintf(msg, "ms: %.5f\n", dt); OutputDebugString(msg);
    }
    else
    {
        // todo: seems to happen a lot with just clicking a bunch?
        // missed fps target
        char msg[256];
        sprintf(msg, "!! missed fps !! target ms: %.5f, frame ms: %.5f\n",
                targetMsPerFrame, dt);
        OutputDebugString(msg);
    }
    app_timer.EndFrame();  // make sure to call for MsSinceLastFrame() to work.. feels weird

}



void SetWindowToAspectRatio()
{
    RECT winRect;
    GetWindowRect(window, &winRect);
    int w = winRect.right - winRect.left;
    int h = winRect.bottom - winRect.top;
    // which to adjust tho?
    int nw = (int)((double)h * vid_aspect);
    int nh = (int)((double)w / vid_aspect);
    // i guess always make smaller for now
    if (nw < w)
        MoveWindow(window, winRect.left, winRect.top, nw, h, true);
    else
        MoveWindow(window, winRect.left, winRect.top, w, nh, true);
}




#define ID_EXIT 1001
#define ID_PAUSE 1002
#define ID_ASPECT 1003


void OpenRClickMenuAt(HWND hwnd, POINT point)
{
    UINT aspectChecked = lock_aspect ? MF_CHECKED : MF_UNCHECKED;
    HMENU hPopupMenu = CreatePopupMenu();
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_EXIT, L"Exit");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | aspectChecked, ID_ASPECT, L"Lock Aspect Ratio");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_PAUSE, L"Pause/Play");
    SetForegroundWindow(hwnd);

    globalContextMenuOpen = true;
    menuCloseTimer.Stop();
    TrackPopupMenu(hPopupMenu, 0, point.x, point.y, 0, hwnd, NULL);
    menuCloseTimer.Start();
}



POINT mDownPoint;
bool mDown;
bool itWasADrag;
bool clickingOnProgessBar;
bool wasNonClientHit;

void onMouseUp()
{
    if (!itWasADrag)
    {
        if (!globalContextMenuOpen)
        {
            if (!clickingOnProgessBar) // starting to feel messy, maybe proper mouse even handlers? w/ timers etc?
            {
                if (!wasNonClientHit)
                {
                    // OutputDebugString("false, pause\n");
                    vid_paused = !vid_paused;  // TODO: only if we aren't double clicking? see ;lkj
                }
            }
        }
        else
        {
            // OutputDebugString("true, skip\n");
            globalContextMenuOpen = false; // force skip rest of timer
        }
    }
    // OutputDebugString("clickingOnProgessBar false\n");
    clickingOnProgessBar = false;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {

        case WM_CLOSE: {
            appRunning = false;
        } break;


        case WM_SIZE: {
            // int w = LOWORD(lParam);
            // int h = HIWORD(lParam);
            // int lockedW = (int)((double)h * vid_aspect);
            // int lockedH = (int)((double)w / vid_aspect);
            // if
            winWID = LOWORD(lParam);
            winHEI = HIWORD(lParam);
            return 0;
        }

        case WM_SIZING: {  // when dragging border
            if (lock_aspect)
            {
                RECT rc = *(RECT*)lParam;
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;

                switch (wParam)
                {
                    case WMSZ_LEFT:
                    case WMSZ_RIGHT:
                        rc.bottom = rc.top + (int)((double)w / vid_aspect);
                        break;

                    case WMSZ_TOP:
                    case WMSZ_BOTTOM:
                        rc.right = rc.left + (int)((double)h * vid_aspect);
                        break;

                    case WMSZ_LEFT + WMSZ_TOP:
                    case WMSZ_LEFT + WMSZ_BOTTOM:
                        rc.left = rc.right - (int)((double)h * vid_aspect);
                        break;

                    case WMSZ_RIGHT + WMSZ_TOP:
                        rc.top = rc.bottom - (int)((double)w / vid_aspect);
                        break;

                    case WMSZ_RIGHT + WMSZ_BOTTOM:
                        rc.bottom = rc.top + (int)((double)w / vid_aspect);
                        break;
                }
                *(RECT*)lParam = rc;
            }
        } break;


        case WM_NCHITTEST: {

            RECT win;
            if (!GetWindowRect(hwnd, &win))
                return HTNOWHERE;

            POINT pos = { LOWORD(lParam), HIWORD(lParam) }; // todo: won't work on multiple monitors! use GET_X_LPARAM
            POINT pad = { GetSystemMetrics(SM_CXFRAME), GetSystemMetrics(SM_CYFRAME) };

            bool left   = pos.x < win.left   + pad.x;
            bool right  = pos.x > win.right  - pad.x -1;  // win.right 1 pixel beyond window, right?
            bool top    = pos.y < win.top    + pad.y;
            bool bottom = pos.y > win.bottom - pad.y -1;

            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left)            return HTLEFT;
            if (right)           return HTRIGHT;
            if (top)             return HTTOP;
            if (bottom)          return HTBOTTOM;

            // return HTCAPTION;
            return HTCLIENT; // we now specifically call HTCAPTION in LBUTTONDOWN
        } break;

        // case WM_PAINT: {
        //  // Render();
        //  SwapBuffers((HDC)wParam);
        //     ValidateRect(hwnd, 0); // if we don't do this, we'll just keep getting wm_paint msgs
        //     return 0;
        // } break;


        // case WM_MOUSELEAVE:  // need to call TrackMouseEvents for this? just check in our loop

        case WM_LBUTTONDOWN:{
            // OutputDebugString("DOWN\n");
            wasNonClientHit = false;
            mDown = true;
            itWasADrag = false;
            mDownPoint = { LOWORD(lParam), HIWORD(lParam) };
            if (mDownPoint.y >= winHEI-(progressBarH+progressBarB) && mDownPoint.y <= winHEI-progressBarB)
            {
                double prop = (double)mDownPoint.x / (double)winWID;
                    // char propbuf[123];
                    // sprintf(propbuf, "prop: %f\n", prop);
                    // OutputDebugString(propbuf);
                // OutputDebugString("clickingOnProgessBar true\n");
                clickingOnProgessBar = true;

                setSeek = true;
                seekProportion = prop;
            }
            // mDownTimer.Start();
            // ReleaseCapture(); // still not sure if we should call this or not
            // SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        } break;
        case WM_NCLBUTTONDOWN: {
            wasNonClientHit = true;
            // OutputDebugString("DOWN\n");
            // mDownPoint = { LOWORD(lParam), HIWORD(lParam) };
            // return 0;
        } break;

        case WM_MOUSEMOVE: {
            msOfLastMouseMove = app_timer.MsSinceStart();
            if (mDown)
            {
                // OutputDebugString("MOUSEMOVE\n");

                WINDOWPLACEMENT winpos;
                winpos.length = sizeof(WINDOWPLACEMENT);
                if (GetWindowPlacement(hwnd, &winpos))
                {
                    if (winpos.showCmd == SW_MAXIMIZE)
                    {
                        ShowWindow(hwnd, SW_RESTORE);

                        int mouseX = LOWORD(lParam); // todo: GET_X_PARAM
                        int mouseY = HIWORD(lParam);
                        int winX = mouseX - winWID/2;
                        int winY = mouseY - winHEI/2;
                        MoveWindow(hwnd, winX, winY, winWID, winHEI, true);
                    }
                }

                // need to determine if click or drag here, not in buttonup
                // because mousemove will trigger (i think) at the first pixel of movement
                POINT mPos = { LOWORD(lParam), HIWORD(lParam) };
                double dx = (double)mPos.x - (double)mDownPoint.x;
                double dy = (double)mPos.y - (double)mDownPoint.y;
                double distance = sqrt(dx*dx + dy*dy);
                // not crazy about this...
                // consider:
                // -different m buttons for drag/pause
                // -add time element (fast is always click? hmm maybe)
                // -only 0 pixel movement allowed on pausing?
                    // char msg[123];
                    // sprintf(msg, "dist: %f\n", distance);
                    // OutputDebugString(msg);
                double MOVEMENT_ALLOWED_IN_CLICK = 2.5; // todo: another check to help with feel? speed?
                if (distance <= MOVEMENT_ALLOWED_IN_CLICK)
                {
                    // itWasADrag = false;
                }
                else
                {
                    itWasADrag = true;
                    // ReleaseCapture(); // still not sure if we should call this or not
                    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
            }
        } break;

        case WM_LBUTTONDBLCLK: {       // required CS_DBLCLKS window style

            // TODO: only to undo the pause that happens otherwise see ;lkj
            // if we are paused, double clicking will play a split second of video when max/min-ing video
            // vid_paused = !vid_paused;

            WINDOWPLACEMENT winpos;
            winpos.length = sizeof(WINDOWPLACEMENT);
            if (GetWindowPlacement(window, &winpos))
            {
                if (winpos.showCmd == SW_MAXIMIZE)
                {
                    ShowWindow(window, SW_RESTORE);

                    // make this an option... (we might want to keep it in the corner eg)
                    // int mouseX = LOWORD(lParam); // todo: GET_X_PARAM
                    // int mouseY = HIWORD(lParam);
                    // int winX = mouseX - winWID/2;
                    // int winY = mouseY - winHEI/2;
                    // MoveWindow(hwnd, winX, winY, winWID, winHEI, true);
                }
                else
                {
                    ShowWindow(window, SW_MAXIMIZE);
                }
            }

        } break;



        // timer to keep updating while dragging / resizing
        // feels a bit awkward, plus our framerate dips
        // because we're waiting our target Ms at minimum
        // (any calculation time is added on top)
        case WM_ENTERSIZEMOVE: {
            // targetMsPerFrame locks us in pretty good to the mouse, but our framerate nosedives
            // 10 keeps our framerate fine but lags the window behind the mouse.. why?
            SetTimer(hwnd, 1, 10, NULL);
        } break;

        case WM_TIMER: {
            // OutputDebugString("tick\n");
            Update();
        } break;



        case WM_EXITSIZEMOVE: {
            KillTimer(hwnd, 1);

            mDown = false;  // LBUTTONUP not triggering when captured
            onMouseUp();
        } break;

        case WM_LBUTTONUP: //{
        //     mDown = false;
        //     onMouseUp();
        // } break;
        case WM_NCLBUTTONUP: {
            mDown = false;
            onMouseUp();
        } break;

        case WM_RBUTTONDOWN: {    // rclicks in client area (HTCLIENT)
            POINT openPoint = { LOWORD(lParam), HIWORD(lParam) };
            ClientToScreen(hwnd, &openPoint);
            SetTimer(hwnd, 1, 10, NULL);
            OpenRClickMenuAt(hwnd, openPoint);
        } break;
        case WM_NCRBUTTONDOWN: {  // non-client area, apparently lParam is treated diff?
            POINT openPoint = { LOWORD(lParam), HIWORD(lParam) };
            SetTimer(hwnd, 1, 10, NULL);
            OpenRClickMenuAt(hwnd, openPoint);
        } break;

        case WM_COMMAND: {
            switch (wParam)
            {
                case ID_EXIT:
                    appRunning = false;
                    break;
                case ID_PAUSE:
                    vid_paused = !vid_paused;
                    break;
                case ID_ASPECT:
                    SetWindowToAspectRatio();
                    lock_aspect = !lock_aspect;
                    break;
            }
        } break;


        // can also implement IDropTarget, but who wants to do that?
        case WM_DROPFILES: {
            char filePath[MAX_PATH];
            // 0 = just take the first one
            if (DragQueryFile((HDROP)wParam, 0, (LPSTR)&filePath, MAX_PATH))
            {
                // OutputDebugString(filePath);
                LoadVideoFile(filePath);
            }
            else
            {
                MsgBox("Unable to determine file path of dropped file.");
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
    app_timer.Start();




    // FFMPEG

    InitAV();  // basically just registers all codecs.. call when needed instead?




    // WINDOW

    // register wndproc
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "best class";
    if (!RegisterClass(&wc)) { MsgBox("RegisterClass failed."); return 1; }

    int openWidth = 960;
    int openHeight = 720;
    vidWID = openWidth;
    vidHEI = openHeight;
    RECT neededRect = {};
    winWID = vidWID;
    winHEI = vidHEI;
    neededRect.right = winWID;
    neededRect.bottom = winHEI;
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



    // ENABLE DRAG DROP

    DragAcceptFiles(window, true);



    // LOAD FILE

    LoadVideoFile(INPUT_FILE);



    // MAIN LOOP

    // seed our first frame dt
    app_timer.EndFrame();

    while (appRunning)
    {
        MSG Message;
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        Update();

    }


    return 0;
}