




#include <SDL.h>
#include <SDL_thread.h>
#pragma comment(lib, "SDL2main.lib")
#pragma comment(lib, "SDL2.lib")


struct SDLStuff
{
    bool setup_at_least_once = false;
    int desired_bytes_in_sdl_queue;
    int bytes_per_second;
    SDL_AudioDeviceID audio_device;
    // SDL_AudioSpec spec;
    SDL_AudioFormat format;
    int spec_size;
    double estimated_audio_latency_ms;
};


void logSpec(SDL_AudioSpec *as) {
    char format[1234];
    switch(as->format) {
        case AV_SAMPLE_FMT_U8:   sprintf(format, "AV_SAMPLE_FMT_U8");   break;
        case AV_SAMPLE_FMT_U8P:  sprintf(format, "AV_SAMPLE_FMT_U8P");  break;
        case AV_SAMPLE_FMT_S16:  sprintf(format, "AV_SAMPLE_FMT_S16");  break;
        case AV_SAMPLE_FMT_S16P: sprintf(format, "AV_SAMPLE_FMT_S16P"); break;
        case AV_SAMPLE_FMT_S32:  sprintf(format, "AV_SAMPLE_FMT_S32");  break;
        case AV_SAMPLE_FMT_S32P: sprintf(format, "AV_SAMPLE_FMT_S32P"); break;
        case AV_SAMPLE_FMT_FLT:  sprintf(format, "AV_SAMPLE_FMT_FLT");  break;
        case AV_SAMPLE_FMT_FLTP: sprintf(format, "AV_SAMPLE_FMT_FLTP"); break;
        case AV_SAMPLE_FMT_DBL:  sprintf(format, "AV_SAMPLE_FMT_DBL");  break;
        case AV_SAMPLE_FMT_DBLP: sprintf(format, "AV_SAMPLE_FMT_DBLP"); break;
        default: sprintf(format, "%5d", (int)as->format); break;
    }
    char log[1024];
    sprintf(log,
        " freq______%5d\n"
        " format____%s\n"
        " channels__%5d\n"
        " silence___%5d\n"
        " samples___%5d\n"
        " size______%5d\n\n",
        (int) as->freq,
              format,
        (int) as->channels,
        (int) as->silence,
        (int) as->samples,
        (int) as->size
    );
    LogMessage(log);
}



bool CreateSDLAudioDeviceFor(AVCodecContext *acc, SDLStuff *sdl_stuff)
{

    // note: the av planar formats are converted to interleaved in the decoder
    // sdl only likes interleaved

    int format;
    switch (acc->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:  format = AUDIO_U8; break;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P: format = AUDIO_S16; break;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P: format = AUDIO_S32; break;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP: format = AUDIO_F32; break;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP: // use float here and fix in decoder?
        default:
            char msg[1234];
            sprintf(msg, "SDL: Audio format not supported yet.\n%i", acc->sample_fmt);
            LogError(msg);
    }


    SDL_AudioSpec wanted_spec, spec;
    wanted_spec.freq = acc->sample_rate;
    wanted_spec.format = format;
    wanted_spec.channels = acc->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024; // SDL_AUDIO_BUFFER_SIZE // estimate latency based on this?
    wanted_spec.callback = 0;  // none to set samples ourself
    wanted_spec.userdata = 0;


    sdl_stuff->audio_device = SDL_OpenAudioDevice(0, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);

    // sdl_stuff->spec = spec;
    // for now use twice the buffer length, seems about right (but haven't tried changing the buffer length)
    sdl_stuff->estimated_audio_latency_ms = (double)spec.samples / (double)spec.freq * 1000.0;
    sdl_stuff->estimated_audio_latency_ms *= 2;

    if (sdl_stuff->audio_device == 0)
    {
        char audioerr[256];
        sprintf(audioerr, "SDL: Failed to open audio: %s\n", SDL_GetError());
        LogError(audioerr);
        return false;
    }

    if (wanted_spec.format != spec.format)
    {
        // try another one instead of failing?
        char audioerr[256];
        sprintf(audioerr, "SDL: Couldn't find desired format: %s\n", SDL_GetError());
        LogError(audioerr);
        return false;
    }

    // need to know when remixing volume
    sdl_stuff->format = spec.format;

    // this seems to be how many bytes we consume per frame
    // good for estimating how many bytes to send to sdl
    sdl_stuff->spec_size = spec.size;

    LogMessage("SDL: audio spec wanted:\n");
    logSpec(&wanted_spec);
    LogMessage("SDL: audio spec got:\n");
    logSpec(&spec);

    return true;
}




int nearestFactorOf4096(double input)
{
    if (input < 0) assert(false);  // something went wrong
    for (int i = 0; i < 10; i++)
    {
        if (input < 4096*i) return 4096*i;
    }
    assert(false); // todo: for now alert us about this
    return 4096*10;
}

// todo: what to do with this
void SetupSDLSoundFor(AVCodecContext *acc, SDLStuff *sdl_stuff, double video_fps)
{
    // todo: remove the globals from this function (return an audio_buffer object?)

    if (sdl_stuff->setup_at_least_once)
    {
        SDL_PauseAudioDevice(sdl_stuff->audio_device, (int)true);
        SDL_CloseAudioDevice(sdl_stuff->audio_device);
    }
    else
    {
        // should un-init if we do want to call this again
        if (SDL_Init(SDL_INIT_AUDIO))
        {
            char err[256];
            sprintf(err, "SDL: Couldn't initialize: %s", SDL_GetError());
            LogError(err);
            //return false;
        }
        sdl_stuff->setup_at_least_once = true;
    }

    if (!CreateSDLAudioDeviceFor(acc, sdl_stuff))
    {
        LogError("SDL: Failed to create audio device.");
        // return false; ?
    }


    // this isn't the same as video_fps... hmm
    // double targetFPS = ((double)acc->time_base.den /
    //                     (double)acc->time_base.num) /
    //                     (double)acc->ticks_per_frame;

    // right place for this or better place??
    int bytes_per_sample = av_get_bytes_per_sample(acc->sample_fmt) * acc->channels;
    sdl_stuff->bytes_per_second = bytes_per_sample * acc->sample_rate;

    double bytes_per_frame = sdl_stuff->bytes_per_second / video_fps;

    // char buf[123];
    // sprintf(buf, "fps: %f  bytes_per_frame: %.4f\n", video_fps, bytes_per_frame);
    // LogMessage(buf);

    // todo: test: we should be able to adjust amount in queue without messing up our sync
    int frames_ahead_to_queue = 2; // or should we do more? (more = slower volume response)
    sdl_stuff->desired_bytes_in_sdl_queue = nearestFactorOf4096(bytes_per_frame) * frames_ahead_to_queue;

    // bytes_per_frame works so well we haven't tried spec_size
    // sdl_stuff->desired_bytes_in_sdl_queue = nearestFactorOf4096sdl_stuff->spec_size) * frames_ahead_to_queue;
}