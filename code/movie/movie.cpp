


void LogError(char *str) { MessageBox(0,str,0,0); }
void LogMessage(char *str) { OutputDebugString(str); }


#include "utils.h"


#include "ffmpeg.cpp"

#include "sdl.cpp"



// todo: support multiple threads launched
// and use results from the most recent working one?
static HANDLE global_asyn_load_thread;




float GetWallClockSeconds()
{
    LARGE_INTEGER counter; QueryPerformanceCounter(&counter);
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    return (float)counter.QuadPart / (float)freq.QuadPart;
}


struct frame_buffer
{
    u8 *mem;
    int wid;
    int hei;
    int get_size() { return mem ? wid*hei*sizeof(u32) : 0; }
    void alloc_mem(int w, int h) {
        free_if_needed();
        mem = (u8*)malloc(w*h*sizeof(u32));
        wid=w;
        hei=h;
    }
    void free_if_needed() {
        if (mem)
            free(mem);
    }
    void resize_if_needed(int w, int h) {
        if (w!=wid || h!=hei)
            alloc_mem(w,h);
    }
};


// basically a movie source with a time component
struct RollingMovie
{
    ffmpeg_source reel;
    // ffmpeg_source lowq; //something like this?
    // ffmpeg_source medq;
    // ffmpeg_source highq;

    double elapsed;

    i64 ptsOfLastVideo; // "ptsOfLastVideoFrameDecoded" too verbose?
    i64 ptsOfLastAudio;

    // better names for these?
    // secondsFromLastDecodedVideoPTS ? hmm
    double secondsFromVideoPTS()
    {
        return ffmpeg_seconds_from_pts(ptsOfLastVideo, reel.vfc, reel.video.index);
    }
    double secondsFromAudioPTS()
    {
        return ffmpeg_seconds_from_pts(ptsOfLastAudio, reel.afc, reel.audio.index);
    }


    // todo: what to do with this, hmmm
    // what about a ffmpeg_rolling_movie ?
    // called "hard" because we flush the buffers and may have to grab a few frames to get the right one
    void hard_seek_to_timestamp(double seconds)
    {
        ffmpeg_source *source = &reel;

        // todo: measure the time this function takes and debug output it

        // not entirely sure if this flush usage is right
        if (source->video.codecContext) avcodec_flush_buffers(source->video.codecContext);
        if (source->audio.codecContext) avcodec_flush_buffers(source->audio.codecContext);

        i64 seekPos = nearestI64((double)seconds * (double)AV_TIME_BASE);

        // AVSEEK_FLAG_BACKWARD = find first I-frame before our seek position
        if (source->video.codecContext) av_seek_frame(source->vfc, -1, seekPos, AVSEEK_FLAG_BACKWARD);
        if (source->audio.codecContext) av_seek_frame(source->afc, -1, seekPos, AVSEEK_FLAG_BACKWARD);


        // step through video frames...
        int frames_skipped;
        source->GetNextVideoFrame(
            // source->vfc,
            // source->video.codecContext,
            // source->sws_context,
            // source->video.index,
            // source->frame_output,
            seconds * 1000.0,// - msAudioLatencyEstimate,
            0,
            true,
            &ptsOfLastVideo,
            &frames_skipped);


        // step through audio frames...
        // kinda awkward
        ffmpeg_sound_buffer dummyBufferJunkData;
        dummyBufferJunkData.data = (u8*)malloc(1024 * 10);
        dummyBufferJunkData.size_in_bytes = 1024 * 10;
        int bytes_queued_up = source->GetNextAudioFrame(
            // source->afc,
            // source->audio.codecContext,
            // source->audio.index,
            dummyBufferJunkData,
            1024,
            seconds * 1000.0,
            &ptsOfLastAudio);
        free(dummyBufferJunkData.data);
    }

    // TODO: move audio latency to this class???
    void load_new_source(ffmpeg_source *new_source, double startAt)
    {
        if (!new_source->vfc) MessageBox(0,"problem outside",0,0);

        reel.TransferFromReel(new_source);


        elapsed = startAt;

        // get first frame even if startAt is 0 because we could be paused
        hard_seek_to_timestamp(startAt);
    }

};


struct AppState
{
    char *exe_directory;


    bool lock_aspect = true;
    bool repeat = true;

    double volume = 1.0;

    bool bufferingOrLoading = true;


    bool is_paused = false;
    bool was_paused = false;

    double targetSecPerFrame;
};



struct AppMessages
{
    bool setSeek = false;
    double seekProportion = 0;

    bool new_source_ready = false;
    ffmpeg_source new_reel;

    int startAtSeconds = 0;


    char file_to_load[1024]; // todo what max
    bool load_new_file = false;

};





DWORD WINAPI AsyncMovieLoad( LPVOID lpParam );

DWORD WINAPI StartProjectorChurnThread( LPVOID lpParam );


// not crazy about the api for this
// i think longer-term this would
// continuously decode / dl frames ahead/behind
// (just ahead or all over file?)
// (eg make halfway point between I-frames new I-frames?)
// (custom in-memory (on drive?) compression?)
// but improving dl > improving decoding which is already fast
//
// thinking eventually something like this:
// .StartBackgroundChurn()
// .CheckMoviePath(path) ?
// .TryLoadMoviePath(path)
// dt? = GetBestAvailableNextFrame(&outFrame)
// GetBestAvailableFrameAtTime(seconds) ?
// GetBestAvailableFrameForFrameX(frameCount) ?
// if (MovieLoaded) PlayMovie ?
// .PauseChurn() ?
// .Destroy()
//
// with these controls:
// .TryLoadingNewMoviePath()
// .PauseVideoPlayback() / PlayVideoPlayback()
// .SeekTo(percent)
// .Step(+1 / -1) ?
// .SetVolume(amt)
// .SetSpeed() ?
// .GetSourcePathWithTimestamp()
// .SetCPUUsage()?
//
// but not sure how to handle sound
// maybe circular buffer constantly being filled while churn()
// and do we query for data by timestamp?
// or let decoder control speed/playback?
// and just query for what data/timestamp decoder is at? hmm
// .AdjustTargetAudioLagTime(ms) ?
// .SetMaxAudioLagTime(ms) ?
// .SetMaxAudioLeadTime(ms) ?
// .SetMaxAudioLagLeadTime(ms, ms) ?
struct MovieProjector
{
    bool is_churning = false;  // are we decoding in the background?
    HANDLE churn_thread;

    AppState state; // todo: drop this sub struct?

    ffmpeg_sound_buffer ffmpeg_to_sdl_buffer;
    ffmpeg_sound_buffer volume_adjusted_buffer;

    SDLStuff sdl_stuff;

    RollingMovie rolling_movie;


    // for now just copy v output to this every frame
    frame_buffer fb1;
    frame_buffer fb2;
    frame_buffer *front_buffer = 0;
    frame_buffer *back_buffer = 0;


    // mostly flags, basic way to communicate between threads etc
    AppMessages message;



    void PlayMovie()
    {
        if (rolling_movie.reel.IsAudioAvailable())
            SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)false);
        state.is_paused = false;
    }

    void PauseMovie()
    {
        SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)true);
        state.is_paused = true;
    }

    void TogglePlayPauseMovie()
    {
        state.is_paused = !state.is_paused;
        if (state.is_paused && !state.was_paused)
        {
            PauseMovie();
        }
        if (!state.is_paused && state.was_paused)
        {
            PlayMovie();
        }
        state.was_paused = state.is_paused;
    }




    void SetVolume(double volume)
    {
        if (volume < 0) volume = 0;
        if (volume > 1) volume = 1;
        state.volume = volume;
    }



    void QueueLoadFromPath(char *path)
    {
        strcpy_s(message.file_to_load, 1024, path);
        message.load_new_file = true;
    }


    // something like this to replace "percent" etc?
    double _elapsed_time_at_last_audio_decode() { return rolling_movie.secondsFromAudioPTS(); }
    double _elapsed_time_at_last_video_decode() { return rolling_movie.secondsFromVideoPTS(); }


    void Init(char *exedir)
    {

        ffmpeg_init();  // basically just registers all codecs..

        state.exe_directory = exedir;

        state.targetSecPerFrame = 1.0/30.0; // for before video loads

        front_buffer = &fb1;
        back_buffer = &fb2;

    }


    // now running this on a sep thread from our msg loop so it's independent of mouse events / captures
    void Update()
    {
        if (message.new_source_ready)
        {
            message.new_source_ready = false;
            OutputDebugString("Ready to load new movie...\n");
            // LoadNewMovieSource(&message.new_reel);
            rolling_movie.load_new_source(&message.new_reel, message.startAtSeconds);

            // SDL, for sound atm (todo: move sound playing out of projector and into calling application?)
            if (rolling_movie.reel.audio.codecContext)
            {
                SetupSDLSoundFor(rolling_movie.reel.audio.codecContext, &sdl_stuff, rolling_movie.reel.fps);
                ffmpeg_to_sdl_buffer.setup(rolling_movie.reel.audio.codecContext);
                volume_adjusted_buffer.setup(rolling_movie.reel.audio.codecContext);
            }

            state.targetSecPerFrame = 1.0 / rolling_movie.reel.fps;
            state.bufferingOrLoading = false;
            PlayMovie();
        }

        double percent; // make into method?

        // state.bufferingOrLoading = true;
        if (state.bufferingOrLoading)
        {
            PauseMovie();
        }
        else
        {

            if (message.setSeek)
            {
                SDL_ClearQueuedAudio(sdl_stuff.audio_device);

                message.setSeek = false;
                int seekPos = message.seekProportion * rolling_movie.reel.vfc->duration;

                double seconds = message.seekProportion*rolling_movie.reel.vfc->duration;
                rolling_movie.hard_seek_to_timestamp(seconds);
            }


            if (!state.is_paused)
            {

                // SOUND

                int bytes_left_in_queue = SDL_GetQueuedAudioSize(sdl_stuff.audio_device);
                    // char msg[256];
                    // sprintf(msg, "bytes_left_in_queue: %i\n", bytes_left_in_queue);
                    // OutputDebugString(msg);


                int wanted_bytes = sdl_stuff.desired_bytes_in_sdl_queue - bytes_left_in_queue;
                    // char msg3[256];
                    // sprintf(msg3, "wanted_bytes: %i\n", wanted_bytes);
                    // OutputDebugString(msg3);

                if (wanted_bytes >= 0)
                {
                    if (wanted_bytes > ffmpeg_to_sdl_buffer.size_in_bytes)
                    {
                        // char errq[256];
                        // sprintf(errq, "want to queue: %i, but only space for %i in buffer\n", wanted_bytes, ffmpeg_to_sdl_buffer.size_in_bytes);
                        // OutputDebugString(errq);

                        wanted_bytes = ffmpeg_to_sdl_buffer.size_in_bytes;
                    }

                    // ideally a little bite of sound, every frame
                    // todo: better way to track all the pts, both a/v and seeking etc?
                    int bytes_queued_up = rolling_movie.reel.GetNextAudioFrame(
                        ffmpeg_to_sdl_buffer,
                        wanted_bytes,
                        -1,
                        &rolling_movie.ptsOfLastAudio);


                    if (bytes_queued_up > 0)
                    {
                        // remix to adjust volume
                        // need a second buffer
                        memset(volume_adjusted_buffer.data, 0, bytes_queued_up); // make sure we mix with silence
                        SDL_MixAudioFormat(
                            volume_adjusted_buffer.data,
                            ffmpeg_to_sdl_buffer.data,
                            sdl_stuff.format,
                            bytes_queued_up,
                            nearestInt(state.volume * SDL_MIX_MAXVOLUME));

                        // a raw copy would just be max volume
                        // memcpy(volume_adjusted_buffer.data,
                        //        ffmpeg_to_sdl_buffer.data,
                        //        bytes_queued_up);

                        // if (SDL_QueueAudio(sdl_stuff.audio_device, ffmpeg_to_sdl_buffer.data, bytes_queued_up) < 0)
                        if (SDL_QueueAudio(sdl_stuff.audio_device, volume_adjusted_buffer.data, bytes_queued_up) < 0)
                        {
                            char audioerr[256];
                            sprintf(audioerr, "SDL: Error queueing audio: %s\n", SDL_GetError());
                            OutputDebugString(audioerr);
                        }
                           // char msg2[256];
                           // sprintf(msg2, "bytes_queued_up: %i  seconds: %.3f\n", bytes_queued_up,
                           //         (double)bytes_queued_up / (double)sdl_stuff.bytes_per_second);
                           // OutputDebugString(msg2);
                    }
                }




                // TIMINGS / SYNC



                int bytes_left = SDL_GetQueuedAudioSize(sdl_stuff.audio_device);
                double seconds_left_in_queue = (double)bytes_left / (double)sdl_stuff.bytes_per_second;
                    // char secquebuf[123];
                    // sprintf(secquebuf, "seconds_left_in_queue: %.3f\n", seconds_left_in_queue);
                    // OutputDebugString(secquebuf);

                double ts_audio = rolling_movie.secondsFromAudioPTS();

                // if no audio, use video pts (we should basically never skip or repeat in this case)
                if (!rolling_movie.reel.IsAudioAvailable())
                {
                    ts_audio = rolling_movie.secondsFromVideoPTS();
                }

                // use ts audio to get track bar position
                rolling_movie.elapsed = ts_audio;
                percent = rolling_movie.elapsed/rolling_movie.reel.durationSeconds;

                // assuming we've filled the sdl buffer, we are 1 second ahead
                // but is that actually accurate? should we instead use SDL_GetQueuedAudioSize again to est??
                // and how consistently do we pull audio data? is it sometimes more than others?
                // update: i think we always put everything we get from decoding into sdl queue,
                // so sdl buffer should be a decent way to figure out how far our audio decoding is ahead of "now"
                double aud_seconds = ts_audio - seconds_left_in_queue;
                    // char audbuf[123];
                    // sprintf(audbuf, "raw: %.1f  aud_seconds: %.1f  seconds_left_in_queue: %.1f\n",
                    //         ts_audio.seconds(), aud_seconds, seconds_left_in_queue);
                    // OutputDebugString(audbuf);


                double ts_video = rolling_movie.secondsFromVideoPTS();
                double vid_seconds = ts_video;

                double estimatedVidPTS = vid_seconds + state.targetSecPerFrame;


                // better to have audio lag than lead, these based loosely on:
                // https://en.wikipedia.org/wiki/Audio-to-video_synchronization#Recommendations
                double idealMaxAudioLag = 80;
                double idealMaxAudioLead = 20;

                // todo: make this / these adjustable?
                // based on screen recording the sdl estimate seems pretty accurate, unless
                // that's just balancing out our initial uneven tolerances? dunno
                // double estimatedSDLAudioLag = sdl_stuff.estimated_audio_latency_ms;
                // it almost feels better to actually have a little audio delay though...
                double estimatedSDLAudioLag = 0;
                double allowableAudioLag = idealMaxAudioLag - estimatedSDLAudioLag;
                double allowableAudioLead = idealMaxAudioLead + estimatedSDLAudioLag;


                // VIDEO

                // seems like all the skipping/repeating/seeking/starting etc sync code is a bit scattered...
                // i guess seeking/starting -> best effort sync, and auto-correct in update while running
                // but the point stands about skipping/repeating... should we do both out here? or ok to keep sep?

                // todo: want to do any special handling here if no video?

                static int repeatCount = 0;
                static bool repeatedLastFrame = false;

                // time for a new frame if audio is this far behind
                if (aud_seconds > estimatedVidPTS - allowableAudioLag/1000.0
                    || !rolling_movie.reel.IsAudioAvailable()
                )
                {
                    int frames_skipped = 0;
                    rolling_movie.reel.GetNextVideoFrame(
                        aud_seconds * 1000.0,  // rolling_movie.audio_stopwatch.MsElapsed(), // - sdl_stuff.estimated_audio_latency_ms,
                        allowableAudioLead,
                        rolling_movie.reel.IsAudioAvailable(),
                        &rolling_movie.ptsOfLastVideo,
                        &frames_skipped);

                    if (frames_skipped > 0) {
                        char skipbuf[64];
                        sprintf(skipbuf, "frames skipped: %i\n", frames_skipped);
                        OutputDebugString(skipbuf);
                    }
                    if (repeatedLastFrame)
                    {
                        char buf[123];
                        sprintf(buf, "%i times\n", repeatCount);
                        OutputDebugString(buf);
                        repeatCount = 0;
                    }
                    repeatedLastFrame = false;
                }
                else
                {
                    repeatCount++;
                    repeatedLastFrame = true;
                    OutputDebugString("repeating a frame... ");
                }


                ts_video = rolling_movie.secondsFromVideoPTS();
                vid_seconds = ts_video;


                // how far ahead is the sound?
                double delta_ms = (aud_seconds - vid_seconds) * 1000.0;

                // the real audio being transmitted is sdl's write buffer which will be a little behind
                double aud_sec_corrected = aud_seconds - estimatedSDLAudioLag/1000.0;
                double delta_with_correction = (aud_sec_corrected - vid_seconds) * 1000.0;

                // char ptsbuf[123];
                // sprintf(ptsbuf, "vid ts: %.1f, aud ts: %.1f, delta ms: %.1f, correction: %.1f\n",
                //         vid_seconds, aud_seconds, delta_ms, delta_with_correction);
                // OutputDebugString(ptsbuf);



                // "RENDER" basically (to an offscreen buffer)

                u8 *src = rolling_movie.reel.vid_buffer;
                int bw = rolling_movie.reel.vid_width;
                int bh = rolling_movie.reel.vid_height;
                back_buffer->resize_if_needed(bw, bh);
                assert(back_buffer->get_size() == bw*bh*sizeof(u32));
                memcpy(back_buffer->mem, src, back_buffer->get_size());

                frame_buffer *old_front = front_buffer;
                front_buffer = back_buffer;
                back_buffer = old_front;

            }
        }


        // REPEAT
        if (state.repeat && percent > 1.0)  // note percent will keep ticking up even after vid is done
        {
            rolling_movie.hard_seek_to_timestamp(0);
        }


    }


    bool CreateAsycLoadingThread()
    {
        // try waiting on this until we confirm it's a good path/file
        // projector.state.bufferingOrLoading = true;
        // projector.PauseMovie(false); // stop playing movie as well, we'll auto start the next one
        // SplashMessage("fetching...", 0xaaaaaaff);

        // // todo: move this into some parse subcall?
        // char *timestamp = strstr(path, "&t=");
        // if (timestamp == 0) timestamp = strstr(path, "#t=");
        // if (timestamp != 0) {
        //     int startSeconds = SecondsFromStringTimestamp(timestamp);
        //     message.startAtSeconds = startSeconds;
        //         // char buf[123];
        //         // sprintf(buf, "\n\n\nstart seconds: %i\n\n\n", startSeconds);
        //         // OutputDebugString(buf);
        // }
        // else
        // {
        //     message.startAtSeconds = 0; // so we don't inherit start time of prev video
        // }

        // todo: we should check for certain fails here
        // so we don't cancel the loading thread if we don't have to
        // e.g. we could know right away if this is a text file,
        // we don't need to wait for youtube-dl for that at least
        // ideally we wouldn't cancel the loading thread for ANY bad input
        // maybe we start a new thread for every load attempt
        // and timestamp them or something so the most recent valid one is loaded?

        // stop previous thread if already loading
        // todo: audit this, are we ok to stop this thread at any time? couldn't there be issues?
        // maybe better would be to finish each thread but not use the movie it retrieves
        // unless it was the last thread started? (based on some unique identifier?)
        // but is starting a thread each time we load a new movie really what we want? seems odd
        if (global_asyn_load_thread != 0)
        {
            DWORD exitCode;
            GetExitCodeThread(global_asyn_load_thread, &exitCode);
            TerminateThread(global_asyn_load_thread, exitCode);
        }

        // TODO: any reason we don't just move this whole function into the async call?

        global_asyn_load_thread = CreateThread(0, 0, AsyncMovieLoad, (void*)this, 0, 0);


        // // strip off timestamp before caching path
        // if (timestamp != 0)
        //     timestamp[0] = '\0';

        // // save url for later (is rolling_movie the best place for cached_url?)
        // // is this the best place to set cached_url?
        // strcpy_s(projector.rolling_movie.cached_url, URL_BUFFER_SIZE, path);


        return true;
    }




    void StartBackgroundChurn()
    {
        churn_thread = CreateThread(0, 0, StartProjectorChurnThread, (void*)this, 0, 0);
    }

    void Cleanup()
    {
        // sometimes crash on exit when not calling this? todo: confirm
        SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)true);
        SDL_CloseAudioDevice(sdl_stuff.audio_device);
    }

    void KillBackgroundChurn()
    {
        is_churning = false;

        // wait for thread to end so it can cleanup properly
        // we could cleanup here instead?
        // then this would be the only way to stop the churn cleanly, but that's probably fine
        // we should wait for thread to end anyway in that case, so probably doesn't matter
        DWORD res = WaitForSingleObject(churn_thread, 500/*ms timeout*/);
        // if (res == WAIT_OBJECT_0) thread closed
        if (res == WAIT_TIMEOUT) OutputDebugString("Timeout reached waiting for churn thread to end...");
        // if (res == WAIT_FAILED) some other error
    }

};



// todo: consider a sep file / lib for a yotube-dl interface/wrapper?
bool GetStringFromYoutubeDL(char *url, char *options, char *outString, char *exe_dir)
{
    // setup our custom pipes...

    SECURITY_ATTRIBUTES sa;
    // Set up the security attributes struct.
    sa.nLength= sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;


    HANDLE outRead, outWrite;
    if (!CreatePipe(&outRead, &outWrite, &sa, 0))
    {
        OutputDebugString("Error with CreatePipe()");
        return false;
    }


    // actually run the cmd...

    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    // Set up the start up info struct.
    ZeroMemory(&si,sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = outWrite;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;

    char youtube_dl_path[MAX_PATH];  // todo: replace this with something else.. malloc perhaps
    sprintf(youtube_dl_path, "%syoutube-dl.exe", exe_dir);

    char args[MAX_PATH]; //todo: tempy
    sprintf(args, "%syoutube-dl.exe %s %s", exe_dir, options, url);
    // MsgBox(args);

    if (!CreateProcess(
        youtube_dl_path,
        //"youtube-dl.exe",
        args,  // todo: UNSAFE
        0, 0, TRUE,
        CREATE_NEW_CONSOLE,
        0, 0,
        &si, &pi))
    {
        LogError("youtube-dl: CreateProcess() error");
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    TerminateProcess(pi.hProcess, 0); // kill youtube-dl if still running
    // todo: what happens if main process is forced closed while youtube-dl is running? how to close ytdl?

    // close write end before reading from read end
    if (!CloseHandle(outWrite))
    {
        LogError("youtube-dl: CloseHandle() error");
        return false;
    }


    // // get the string out of the pipe...
    // // char *result = path; // huh??
    // int bigEnoughToHoldMessyUrlsFromYoutubeDL = 1024 * 30;
    // char *result = (char*)malloc(bigEnoughToHoldMessyUrlsFromYoutubeDL); // todo: leak

    DWORD bytesRead;
    if (!ReadFile(outRead, outString, 1024*8, &bytesRead, NULL))
    {
        LogError("youtube-dl: Read pipe error!");
        return false;
    }

    if (bytesRead == 0)
    {
        LogError("youtube-dl: Pipe empty");
        return false;
    }

    return true;
}




bool ParseOutputFromYoutubeDL(char *path, char *video, char *audio, char *outTitle, char *exe_dir)
{
    char *tempString = (char*)malloc(1024*30); // big enough for messy urls

    // -g gets urls (seems like two: video then audio)
    if (!GetStringFromYoutubeDL(path, "--get-title -g", tempString, exe_dir))
    {
        return false;
    }

    char *segments[3];

    int count = 0;
    segments[count++] = tempString;
    for (char *c = tempString; *c; c++)
    {
        if (*c == '\n')  // basically convert \n to \0 and make pointer to each line in segments
        {
            *c = '\0';

            if (count < 3)  // only bother with the first 3 though
            {
                segments[count++] = c+1;
            }
            else
            {
                break;
            }
        }
    }

    int titleLen = strlen(segments[0]);  // note this doesn't include the \0
    if (titleLen+1 > FFMPEG_TITLE_SIZE-1) // note -1 since [size-1] is \0  and +1 since titleLen doesn't count \0
    {
        segments[0][FFMPEG_TITLE_SIZE-1] = '\0';
        segments[0][FFMPEG_TITLE_SIZE-2] = '.';
        segments[0][FFMPEG_TITLE_SIZE-3] = '.';
        segments[0][FFMPEG_TITLE_SIZE-4] = '.';
    }

    strcpy_s(outTitle, FFMPEG_TITLE_SIZE, segments[0]);
    strcpy_s(video, 1024*10, segments[1]);  // todo: pass in these string limits?
    strcpy_s(audio, 1024*10, segments[2]);

    free(tempString);

    OutputDebugString("\n");
    OutputDebugString(outTitle); OutputDebugString("\n");
    OutputDebugString(video); OutputDebugString("\n");
    OutputDebugString(audio); OutputDebugString("\n");
    OutputDebugString("\n");

    return true;
}




// fill MovieReel with data from movie at path
// calls youtube-dl if needed so could take a sec
bool SlowCreateReelFromAnyPath(char *path, ffmpeg_source *newMovie, char *exe_dir)
{
    char loadingMsg[1234];
    sprintf(loadingMsg, "\nLoading %s\n", path);
    OutputDebugString(loadingMsg);

    // todo: check limits on title before writing here and below
    char *outTitle = newMovie->title;
    strcpy_s(outTitle, FFMPEG_TITLE_SIZE, "[no title]");

    if (StringIsUrl(path))
    {
        char *video_url = (char*)malloc(1024*10);  // big enough for some big url from youtube-dl
        char *audio_url = (char*)malloc(1024*10);  // todo: mem leak if we kill this thread before free()
        if(ParseOutputFromYoutubeDL(path, video_url, audio_url, outTitle, exe_dir))
        {
            if (!newMovie->SetFromPaths(video_url, audio_url))
            {
                free(video_url);
                free(audio_url);

                // SplashMessage("video failed");
                return false;
            }
            free(video_url);
            free(audio_url); // all these frees are a bit messy, better way?
        }
        else
        {
            free(video_url);
            free(audio_url);

            // SplashMessage("no video");
            return false;
        }
    }
    else if (path[1] == ':')
    {
        // *newMovie = OpenMovieReel(path, path);
        if (!newMovie->SetFromPaths(path, path))
        {
            // SplashMessage("invalid file");
            return false;
        }

        char *fileNameOnly = path;
        while (*fileNameOnly)
            fileNameOnly++; // find end
        while (*fileNameOnly != '\\' && *fileNameOnly != '/')
            fileNameOnly--; // backup till we hit a directory
        fileNameOnly++; // drop the / tho
        strcpy_s(outTitle, FFMPEG_TITLE_SIZE, fileNameOnly); // todo: what length to use?
    }
    else
    {
        // char buf[123];
        // sprintf(buf, "invalid url\n%s", path);
        // SplashMessage(buf);
        // SplashMessage("invalid url");
        return false;
    }

    return true;
}





DWORD WINAPI AsyncMovieLoad( LPVOID lpParam )
{
    MovieProjector *projector = (MovieProjector*)lpParam;

    char *path = projector->message.file_to_load;
    char *exe_dir = projector->state.exe_directory;

    if (!SlowCreateReelFromAnyPath(path, &projector->message.new_reel, exe_dir))
    {
        // now we get more specific error msg in function call,
        // for now don't override them with a new (since our queue is only 1 deep)
        // char errbuf[123];
        // sprintf(errbuf, "Error creating movie source from path:\n%s\n", path);
        LogError("load failed");
        return false;
    }

    // todo: better place for this? i guess it might be fine
    // SetTitle(projector.system.window, projector.message.new_reel.title);

    projector->message.new_source_ready = true;

    return 0;
}






// todo: pass in ghoster app to run here?
DWORD WINAPI StartProjectorChurnThread( LPVOID lpParam )
{
    MovieProjector *projector = (MovieProjector*)lpParam;

    // LOAD FILE
    if (!projector->message.load_new_file)
    {
        // projector.message.QueuePlayRandom();
        projector->QueueLoadFromPath("D:\\~phil\\projects\\ghoster\\test-vids\\test.mp4");
    }

    double timeLastFrame = GetWallClockSeconds();

    projector->is_churning = true;
    while (projector->is_churning)
    {
        // why is this outside of update?
        // maybe have message_check function or something eventually?
        // i guess it's out here because it's not really about playing a movie?
        // the whole layout of ghoster / loading / system needs to be re-worked
        if (projector->message.load_new_file)
        {
            // projector.state.buffering = true;
            projector->CreateAsycLoadingThread();
            projector->message.load_new_file = false;
        }


        projector->Update();


        // HIT FPS
        // todo: do we actually want to hit a certain fps like a game?
        // or accurately track our continuous audio timer...
        // (eg if we're late one frame, go early the next?) hmm
        double dt = GetWallClockSeconds() - timeLastFrame;
        double targetSec = projector->state.targetSecPerFrame;
        if (dt < targetSec)
        {
            Sleep(targetSec - dt);
            while (dt < targetSec)  // is this weird?
            {
                dt = GetWallClockSeconds() - timeLastFrame;
            }
        }
        else
        {
            // missed fps target
            char msg[256];
            sprintf(msg, "!! missed fps !! target ms: %.3f, actual ms: %.3f\n", targetSec*1000.0, dt*1000.0);
            OutputDebugString(msg);
        }
        timeLastFrame = GetWallClockSeconds();
    }

    projector->Cleanup(); // basically sdl at time of writing this

    // might be a good idea to make sure we see this message on exit or ending churn?
    // to make sure everything gets cleaned up right
    OutputDebugString("Ending churn process...\n");
    return 0;
}









bool PasteClipboard(MovieProjector projector)
{
    HANDLE h;
    if (!OpenClipboard(0))
    {
        OutputDebugString("Can't open clipboard.");
        return false;
    }
    h = GetClipboardData(CF_TEXT);
    if (!h) return false;
    int bigEnoughToHoldTypicalUrl = 1024 * 10; // todo: what max to use here?
    char *clipboardContents = (char*)malloc(bigEnoughToHoldTypicalUrl);
    sprintf(clipboardContents, "%s", (char*)h);
    CloseClipboard();
        char printit[MAX_PATH]; // should be +1
        sprintf(printit, "%s\n", (char*)clipboardContents);
        OutputDebugString(printit);
    projector.QueueLoadFromPath(clipboardContents);
    free(clipboardContents);
    return true; // todo: do we need a result from loadmovie?
}


bool CopyUrlToClipboard(MovieProjector projector, bool withTimestamp = false)
{
    char *url = projector.rolling_movie.reel.path;

    char output[FFMEPG_PATH_SIZE]; // todo: stack alloc ok here?
    if (StringIsUrl(url) && withTimestamp) {
        int secondsElapsed = projector.rolling_movie.elapsed;
        sprintf(output, "%s&t=%i", url, secondsElapsed);
    }
    else
    {
        sprintf(output, "%s", url);
    }

    const size_t len = strlen(output) + 1;
    HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
    memcpy(GlobalLock(hMem), output, len);
    GlobalUnlock(hMem);
    OpenClipboard(0);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();

    return true;
}


