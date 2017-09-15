#include <windows.h>
#include <stdio.h>

#include <SDL.h>
#include <SDL_thread.h>

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"

    #include "libavutil/avutil.h"
    #include "libavutil/avstring.h"
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "swscale.lib")

#pragma comment(lib, "avutil.lib")


#pragma comment(lib, "SDL2main.lib")
#pragma comment(lib, "SDL2.lib")




char *INPUT_FILE = "D:/Users/phil/Desktop/test.mp4";




#define u8 uint8_t


void MsgBox(char* s) {
    MessageBox(0, s, "vid player", MB_OK);
}

void SaveFrame(AVFrame *frame, int width, int height, int frame_index)
{
    FILE *file;
    char filename[256];
    int y;

    sprintf(filename, "frame%d.ppm", frame_index);
    file=fopen(filename, "wb");
    if (file==0) return;

        // file header
        fprintf(file, "P6\n%d %d\n255\n", width, height);

        // Write pixel data
        for(y = 0; y < height; y++)
        fwrite(frame->data[0]+y*frame->linesize[0], 1, width*3, file);

    fclose(file);
}







int main(int argc, char *argv[])
{

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        char err[1024];
        sprintf(err, "Couldn't initialize SDL - %s", SDL_GetError());
        MsgBox(err);
        return -1;
    }



    // register all formats and codecs
    av_register_all();


    AVFormatContext *fc = 0;  // =0 or call avformat_alloc_context?

    int open_result = avformat_open_input(&fc, INPUT_FILE, 0, 0);
    if (open_result != 0)
    {
        MsgBox("Can't open file.");
        char errbuf[1024];
        av_strerror(open_result, errbuf, 1024);
        MsgBox(errbuf);
        return -1;
    }

    // populate fc->streams
    if (avformat_find_stream_info(fc, 0) < 0)
    {
        MsgBox("Can't find stream info.");
        return -1;
    }

    av_dump_format(fc, 0, INPUT_FILE, 0);


    // find first video stream
    int videoStream = -1;
    for (int i = 0; i < fc->nb_streams; i++)
    {
        if (fc->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1)
    {
        MsgBox("No video stream found.");
        return -1;
    }


    // shortcut to the codec context
    AVCodecContext *cc_orig = fc->streams[videoStream]->codec;


    // find the actual decoder
    AVCodec *codec = avcodec_find_decoder(cc_orig->codec_id);
    if (!codec)
    {
        MsgBox("Unsupported codec. Yipes.");
        return -1;
    }

    // copy the codec context (don't use directly from cc_orig)
    AVCodecContext *cc = avcodec_alloc_context3(codec);
    if (avcodec_copy_context(cc, cc_orig) != 0)
    {
        MsgBox("Codec context copy failed.");
        return -1;
    }

    // open codec (don't use directly from cc_orig)
    if (avcodec_open2(cc, codec, 0) < 0)
    {
        MsgBox("Couldn't open codec.");
        return -1;
    }

    // frames
    AVFrame *frame = av_frame_alloc();
    AVFrame *frame_rgb = av_frame_alloc();

    if (!frame || !frame_rgb) { MsgBox("Couldn't allocate frame."); return -1; }


    // mem for frame
    int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, cc->width, cc->height);
    u8 *buffer = (u8 *)av_malloc(numBytes * sizeof(u8));


    // tie frame and buffer together i think
    avpicture_fill((AVPicture *)frame_rgb, buffer, AV_PIX_FMT_RGB24, cc->width, cc->height);







    SDL_Window *window = 0;
    window = SDL_CreateWindow(
        "test window",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        cc->width,
        cc->height,
        SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created: %s\n", SDL_GetError());
        return 1;
    }

    // SDL_Surface *screenSurface = 0;
    // screenSurface = SDL_GetWindowSurface(window);
    // if (!screenSurface) {
    //     fprintf(stderr, "Screen surface could not be created: %s\n", SDL_GetError());
    //     SDL_Quit();
    //     return 1;
    // }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting\n");
        exit(1);
    }

    // Allocate a place to put our YUV image on that screen
    SDL_Texture *texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            cc->width,
            cc->height
        );
    if (!texture) {
        fprintf(stderr, "SDL: could not create texture - exiting\n");
        exit(1);
    }

    // set up YV12 pixel array (12 bits per pixel)
    int yPlaneSz = cc->width * cc->height;
    int uvPlaneSz = cc->width * cc->height / 4;
    u8 *yPlane = (u8*)malloc(yPlaneSz);
    u8 *uPlane = (u8*)malloc(uvPlaneSz);
    u8 *vPlane = (u8*)malloc(uvPlaneSz);
    if (!yPlane || !uPlane || !vPlane) {
        fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
        exit(1);
    }
    int uvPitch = cc->width / 2;



    struct SwsContext *sws_context = 0;
    AVPacket packet;

    // init sws context (sws = software scaling?)
    sws_context= sws_getContext(
         cc->width,
         cc->height,
         cc->pix_fmt,
         cc->width,
         cc->height,
         AV_PIX_FMT_YUV420P,
         SWS_BILINEAR,
         0, 0, 0);


    int count = 0;
    while (av_read_frame(fc, &packet) >= 0)
    {
        if (packet.stream_index == videoStream)
        {
            int frame_finished;
            avcodec_decode_video2(cc, frame, &frame_finished, &packet);

            if (frame_finished)
            {

                AVPicture pict;
                pict.data[0] = yPlane;
                pict.data[1] = uPlane;
                pict.data[2] = vPlane;

                pict.linesize[0] = cc->width;
                pict.linesize[1] = uvPitch;
                pict.linesize[2] = uvPitch;

                // Convert the image into YUV format that SDL uses
                sws_scale(
                    sws_context,
                    (u8 const * const *)frame->data, frame->linesize,
                    0, cc->height,
                    pict.data, pict.linesize);

                SDL_UpdateYUVTexture(
                        texture,
                        0,
                        yPlane,
                        cc->width,
                        uPlane,
                        uvPitch,
                        vPlane,
                        uvPitch
                    );

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

            }

            // if (count < 5)
            // {
            //     SaveFrame(frame_rgb, cc->width, cc->height, count);
            //     count++;
            // }

            Sleep(42); //fake ~24fps

        }

        // Free the packet that was allocated by av_read_frame
        // is this really necessary?
        av_free_packet(&packet);

        SDL_Event event;
        SDL_PollEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            exit(0);
            break;
        default:
            break;
        }


    }


    return 1;
}



