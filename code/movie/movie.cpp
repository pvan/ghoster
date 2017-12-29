


void LogError(char *str);
void LogMessage(char *str);


#include "utils.h"


#include "ffmpeg.cpp"

#include "sdl.cpp"


const int LIMIT_QUAL = 720;
const int MAX_QUAL = 1440;   // when we're not set in 720p mode, also the default (atm)

// const int YTDL_TIMEOUT_MS = 5000;  // this amount actually triggered when we has iffy net
const int YTDL_TIMEOUT_MS = 10000;  // give up after this if taking too long to get urls (probably shoddy net)


// todo: support multiple threads launched?
// and use results from the most recent working one?
// update: current system seems to work pretty well
static HANDLE movie_asyn_load_thread = 0;
static bool movie_ytdl_running = false;
static HANDLE movie_ytdl_process = 0;


struct frame_buffer
{
    u8 *mem;
    int wid;
    int hei;
    int size() { return mem ? wid*hei*sizeof(u32) : 0; }
    void alloc(int w, int h) {
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
            alloc(w,h);
    }
};


// basically a movie source with a time component
struct RollingMovie
{
    ffmpeg_source reel = {0};
    // ffmpeg_source lowq; //something like this?
    // ffmpeg_source medq;
    // ffmpeg_source highq;

    double seconds_elapsed_at_last_decode;

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

    double percent_elapsed()
    {
        if (!reel.loaded) return 0;
        return seconds_elapsed_at_last_decode / reel.durationSeconds;
    }

    void get_url(char *output, int output_size, bool include_current_timestamp)
    {
        char *base_url = reel.path;
        if (StringIsUrl(base_url) && include_current_timestamp)
            snprintf(output, output_size, "%s&t=%i", base_url, (int)seconds_elapsed_at_last_decode);
        else
            snprintf(output, output_size, "%s", base_url);
    }

    // todo: what to do with this, hmmm
    // what about a ffmpeg_rolling_movie ?
    // called "hard" because we flush the buffers and may have to grab a few frames to get the right one
    void hard_seek_to_timestamp(double seconds)
    {
        ffmpeg_source *source = &reel;

        // todo: improve the time for urls here by dling lowest qual to start
        // and adaptively improving quality
        // PRINT("hard seeking...\n");
        // double start = GetWallClockSeconds();

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
            dummyBufferJunkData,
            1024,
            seconds * 1000.0,
            &ptsOfLastAudio);
        free(dummyBufferJunkData.data);

        // PRINT("..seeking took %f seconds\n", GetWallClockSeconds()-start);
    }

    // TODO: move audio latency to this class???
    void load_new_source(ffmpeg_source *new_source, double startAt)
    {
        reel.TransferFromReel(new_source);

        // seconds_elapsed_at_last_decode = startAt; // should be set by seek now

        // get first frame even if startAt is 0 because we could be paused
        hard_seek_to_timestamp(startAt);
    }

};


// todo: just put these straight in projector?
struct AppState
{
    char *exe_directory;


    bool lock_aspect = true;
    bool repeat = true;

    double volume = 1.0;

    bool bufferingOrLoading = true; // i think right now this is only ever set on creation?


    bool is_paused = false;
    bool was_paused = false;

    double targetSecPerFrame;
};



struct AppMessages
{
    bool setSeek = false;
    double seekProportion = 0;

    bool new_source_ready = false;

    // this doesn't seem needed if we prevent a new loading thread while new_source_ready is true
    // ffmpeg_source new_reel1 = {0};
    // ffmpeg_source new_reel2 = {0};
    // ffmpeg_source *new_read_reel; // for reading from when installing in decoder
    // ffmpeg_source *new_write_reel; // for writing to when loading from disk/net
    ffmpeg_source new_source = {0};

    int startAtSeconds = 0;


    char new_path_to_load[1024]; // todo what max
    bool new_load_path_queued = false;


    bool savestate_is_saved = false; // basically save everything that could be messed up by a slow double click
    // bool savestate_paused = false; // were we paused when mdown?

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


    int maxQuality = MAX_QUAL; // get video of this res or less (to improve dl performance)


    // mostly flags, basic way to communicate between threads etc
    // todo: make a proper msg queue system?? maybe not
    AppMessages message;

    void (*on_load_callback)(int,int) = 0;  // notify app video is done loading


    bool IsMovieLoaded() { return !state.bufferingOrLoading; }


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

    void TogglePause()
    {
        // todo: is it really necessary to check was_paused.. also, is it used anywhere else?
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



    // basically for undoing the effects of a slow doubleclick...

    // todo: if we have a "videostate" struct we could just copy/restore it for these functions? hmm
    // note we're saving this every time we click down (since it could be the start of a double click)
    // so don't make this too crazy
    void SaveVideoPosition()
    {
        message.savestate_is_saved = true;
        // message.savestate_paused = state.is_paused;

        // todo: this won't be sub-frame accurate (elapsed is really "elapsed at last decoder call")
        // but i guess if we're pausing for a split second it won't be exact anyway
        // double percent = rolling_movie.seconds_elapsed_at_last_decode / rolling_movie.reel.durationSeconds;
        message.seekProportion = rolling_movie.percent_elapsed(); // todo: make new variable rather than co-opt this one?
    }
    void RestoreVideoPosition()
    {
        if (message.savestate_is_saved)
        {
            message.savestate_is_saved = false;
            message.setSeek = true;

            // if (message.savestate_paused) PlayMovie();
            // else PauseMovie();
        }

        // // cancel any play/pause messages (todo: could cancel other valid msgs)
        // global_ghoster.ClearCurrentSplash();
    }




    void SetVolume(double volume)
    {
        if (volume < 0) volume = 0;
        if (volume > 1) volume = 1;
        state.volume = volume;
    }


    void QueueLoadFromPath(char *path)
    {
        PRINT("load new movie in queue");
        strcpy_s(message.new_path_to_load, 1024, path);
        message.new_load_path_queued = true;
    }
    void QueueReload(bool atCurrentTime)  // reloads current video, probably because quality change
    {
        char url_at_time[FFMPEG_PATH_SIZE]; // todo: stack alloc ok here?
        rolling_movie.get_url(url_at_time, FFMPEG_PATH_SIZE, atCurrentTime);
        QueueLoadFromPath(url_at_time);
    }

    void QueueSeekToPercent(double percent)
    {
        message.seekProportion = percent;
        message.setSeek = true;
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

        // message.new_read_reel = &message.new_reel1;
        // message.new_write_reel = &message.new_reel2;


    }


    // now running this on a sep thread from our msg loop so it's independent of mouse events / captures
    void Update()
    {
        if (message.new_source_ready)
        {
            message.new_source_ready = false;
            OutputDebugString("Ready to load new movie...\n");
            // LoadNewMovieSource(&message.new_reel);
            // rolling_movie.load_new_source(message.new_read_reel, message.startAtSeconds);
            rolling_movie.load_new_source(&message.new_source, message.startAtSeconds);

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

            if (on_load_callback) on_load_callback(rolling_movie.reel.width, rolling_movie.reel.height);
        }


        if (message.setSeek)
        {
            message.setSeek = false;

            SDL_ClearQueuedAudio(sdl_stuff.audio_device);

            // int seekPos = message.seekProportion * rolling_movie.reel.vfc->duration;
            double seconds = message.seekProportion * rolling_movie.reel.durationSeconds;

            // update trackbar position immediately
            // (actually now also calling in app code to be even quicker, since this thread could hang dling vids)
            rolling_movie.seconds_elapsed_at_last_decode = seconds;

            rolling_movie.hard_seek_to_timestamp(seconds);
        }


        // state.bufferingOrLoading = true;
        if (state.bufferingOrLoading)
        {
            PauseMovie();  // i think pretty much just to stop sound
        }
        else
        {

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
                    // note above is tracking frames SKIPPED, below is tracking frames REPEATED
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
                    if (!repeatedLastFrame)
                        OutputDebugString("repeating a frame... ");
                    repeatedLastFrame = true;
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


                // update trackbar position
                if (rolling_movie.reel.IsAudioAvailable())
                    rolling_movie.seconds_elapsed_at_last_decode = rolling_movie.secondsFromAudioPTS();
                else if (rolling_movie.reel.IsVideoAvailable())
                    rolling_movie.seconds_elapsed_at_last_decode = rolling_movie.secondsFromVideoPTS();


            } // not paused

        } // not loading




        // "RENDER" basically (to an offscreen buffer)

        u8 *src = rolling_movie.reel.vid_buffer;
        int bw = rolling_movie.reel.vid_width;
        int bh = rolling_movie.reel.vid_height;
        back_buffer->resize_if_needed(bw, bh);
        assert(back_buffer->size() == bw*bh*sizeof(u32));
        memcpy(back_buffer->mem, src, back_buffer->size());

        frame_buffer *old_front = front_buffer;
        front_buffer = back_buffer;
        back_buffer = old_front;


        // REPEAT
        if (state.repeat && rolling_movie.percent_elapsed() > 1.0)  // note percent will keep ticking up even after vid is done
        {
            rolling_movie.hard_seek_to_timestamp(0);
        }


    }


    bool CreateAsycLoadingThread()
    {
        // todo: we could check if it's the same path as we are currently loading and not restart in that case

        // try waiting on this until we confirm it's a good path/file ?

        // SplashMessage("fetching...", 0xaaaaaaff);
        SetSplash("Fetching", 0xaaaaaaaa);

        // todo: we should check for certain fails here
        // so we don't cancel the loading thread if we don't have to
        // e.g. we could know right away if this is a text file,
        // we don't need to wait for youtube-dl for that at least
        // ideally we wouldn't cancel the loading thread for ANY bad input
        // maybe we start a new thread for every load attempt
        // and timestamp them or something so the most recent valid one is loaded?

        // stop previous thread if already loading
        // maybe better would be to finish each thread but not use the movie it retrieves
        // unless it was the last thread started? (based on some unique identifier?)
        // but is starting a thread each time we load a new movie really what we want? seems like overkill
        // now that we guard against dangling ytdl, this seems to work decently well
        OutputDebugString("\n");
        if (movie_asyn_load_thread != 0)
        {
            DWORD exitCode;
            GetExitCodeThread(movie_asyn_load_thread, &exitCode);
            if (exitCode == STILL_ACTIVE)
            {
                OutputDebugString("ytdl started, forcing it closed too...\n");
                if (movie_ytdl_running)
                {
                    OutputDebugString("waiting for ytdl process handle...\n");
                    while (!movie_ytdl_process); // wait for handle to process
                    OutputDebugString("closing ytdl process...\n");
                    TerminateProcess(movie_ytdl_process, 0); // kill youtube-dl if still running
                }
                OutputDebugString("forcing old thread to close\n");
                TerminateThread(movie_asyn_load_thread, exitCode);
            }
        }

        movie_asyn_load_thread = CreateThread(0, 0, AsyncMovieLoad, (void*)this, 0, 0);

        return true;
    }




    void StartBackgroundChurn()
    {
        churn_thread = CreateThread(0, 0, StartProjectorChurnThread, (void*)this, 0, 0);
    }

    void Cleanup()
    {
        // sometimes crash on exit when not calling this? todo: confirm? update: seems completely fixed
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

    PRINT("Creating pipe...\n");

    HANDLE outRead, outWrite;
    if (!CreatePipe(&outRead, &outWrite, &sa, 0))
    {
        LogError("Error with CreatePipe()");
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

    PRINT("Starting youtube-dl process...\n");

    // try a guard like this so we don't force this thread closed while ytdl is running
    movie_ytdl_running = true;       // this actually shouldn't be needed since we close the prev thread
    {
        if (!CreateProcess(
            youtube_dl_path,
            args,  // todo: UNSAFE
            0, 0, TRUE,
            CREATE_NEW_CONSOLE,
            0, 0,
            &si, &pi))
        {
            LogError("youtube-dl: CreateProcess() error");
            return false;
        }
        // <-- if we close here, between these two commands, we could spawn a ytdl proc that doesn't get closed
        // so we use the movie_ytdl_running as a guard against closing this thread until it's ready
        movie_ytdl_process = pi.hProcess;

        PRINT("Waiting for youtube-dl process...\n");

        DWORD res = WaitForSingleObject(pi.hProcess, YTDL_TIMEOUT_MS);
        if (res==WAIT_TIMEOUT) { LogError("Youtube-dl process timeout"); return false; }
        if (res==WAIT_FAILED) { LogError("Youtube-dl process WAIT_FAILED"); return false; }

        PRINT("Terminating youtube-dl process...\n");

        TerminateProcess(pi.hProcess, 0); // kill youtube-dl if still running
    }
    movie_ytdl_running = false;
    movie_ytdl_process = 0;

    PRINT("Closing handle...\n");

    // close write end before reading from read end
    if (!CloseHandle(outWrite))
    {
        LogError("youtube-dl: CloseHandle() error");
        return false;
    }

    PRINT("Reading from pipe...\n");

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




bool ParseOutputFromYoutubeDL(char *path, char *video, char *audio, char *outTitle, char *exe_dir, int maxQual)
{
    char *tempString = (char*)malloc(1024*30); // big enough for messy urls

    PRINT("Calling youtube-dl...\n");

    // --get-title returns title of vid (first to have it be first line of output)
    // -g gets urls instead of dling (default will return two urls)
    // -f gets format specified, if + then multiple urls will be returned (default is bestvideo+bestaudio/best)
    // ("/" is a fallback.. so if no "bestaudio", "best" will be the second url)

    // todo: query formats and pick one ourselves? seem uncessary
    // todo: check could we lag on large files because we're dling the video in the audio url too?

    // now only get videos with maxQual resolution or less (default 1440 at time of writting, 720 for never stutter)
    char args[256];
    sprintf(args, "--get-title -g -f \"bestvideo[height<=?%i]+bestaudio/best\"", maxQual);
    if (!GetStringFromYoutubeDL(path, args, tempString, exe_dir))
    {
        return false;
    }

    if (StringBeginsWith(tempString, "ERROR")) return false; // probably no internet
    // todo: check for other possible error outcomes here? bad urls, etc?

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
bool SlowCreateMovieSourceFromAnyPath(char *path, ffmpeg_source *newSource, char *exe_dir, int maxQual)
{
    char loadingMsg[1234];
    sprintf(loadingMsg, "\nLoading %s\n", path);
    OutputDebugString(loadingMsg);

    // todo: check limits on title before writing here and below
    char *outTitle = newSource->title;
    strcpy_s(outTitle, FFMPEG_TITLE_SIZE, "[no title]");

    if (StringIsUrl(path))
    {
        char *video_url = (char*)malloc(1024*10);  // big enough for some big url from youtube-dl
        char *audio_url = (char*)malloc(1024*10);  // todo: mem leak if we kill this thread before free()
        if(ParseOutputFromYoutubeDL(path, video_url, audio_url, outTitle, exe_dir, maxQual))
        {
            // todo: sometimes fails in here somewhere when loading urls?
            // maybe bad internet?
            LogMessage("PARSED CORRECTLY\n");
            if (!newSource) PRINT("newSource NULL??\n");
            if (!newSource->SetFromPaths(video_url, audio_url))
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
        // *newSource = OpenMovieReel(path, path);
        if (!newSource->SetFromPaths(path, path))
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

    // cache orig path for user to copy later if wanted
    sprintf(newSource->path, path);

    return true;
}





DWORD WINAPI AsyncMovieLoad( LPVOID lpParam )
{
    MovieProjector *projector = (MovieProjector*)lpParam;

    PRINT("starting new loading thread... %x", GetThreadId(movie_asyn_load_thread));

    // projector->message.new_source_ready = false; // feels like this should not be needed here

    char *path = projector->message.new_path_to_load;
    char *exe_dir = projector->state.exe_directory;


    // look for a timestamp on the url
    char *timestamp = strstr(path, "&t=");
    if (timestamp == 0) timestamp = strstr(path, "#t=");
    if (timestamp == 0) timestamp = strstr(path, "?t=");
    if (timestamp != 0) {
        int startSeconds = SecondsFromStringTimestamp(timestamp);
        projector->message.startAtSeconds = startSeconds;
            // char buf[123];
            // sprintf(buf, "\n\n\nstart seconds: %i\n\n\n", startSeconds);
            // OutputDebugString(buf);
    }
    else
    {
        projector->message.startAtSeconds = 0; // so we don't inherit start time of prev video
    }

    // strip off timestamp before further processing (dl works with it, but don't want it for caching)
    if (timestamp != 0)
        timestamp[0] = '\0';


    // if (!SlowCreateMovieSourceFromAnyPath(path, projector->message.new_write_reel, exe_dir))
    if (!SlowCreateMovieSourceFromAnyPath(path, &projector->message.new_source, exe_dir, projector->maxQuality))
    {
        // now we get more specific error msg in function call,
        // for now don't override them with a new (since our queue is only 1 deep)
        // char errbuf[123];
        // sprintf(errbuf, "Error creating movie source from path:\n%s\n", path);
        LogError("load failed");
        return false;
    }

    projector->message.new_source_ready = true;
    // ffmpeg_source *old_read = projector->message.new_read_reel;
    // projector->message.new_read_reel = projector->message.new_write_reel;
    // projector->message.new_write_reel = projector->message.new_read_reel;

    return 0;
}






DWORD WINAPI StartProjectorChurnThread( LPVOID lpParam )
{
    MovieProjector *projector = (MovieProjector*)lpParam;

    // leave no file loaded on start for now
    // // LOAD FILE
    // if (!projector->message.new_load_path_queued)
    // {
    //     // projector.message.QueuePlayRandom();
    //     projector->QueueLoadFromPath("D:\\~phil\\projects\\ghoster\\test-vids\\test.mp4");
    // }

    double timeLastFrame = GetWallClockSeconds();

    projector->is_churning = true;
    while (projector->is_churning)
    {
        // why is this outside of update?
        // maybe have message_check function or something eventually?
        // i guess it's out here because it's not really about playing a movie?
        // the whole layout of ghoster / loading / system needs to be re-worked
        if (projector->message.new_load_path_queued
            && !projector->message.new_source_ready) // no new thread if we have to process results from one
        {
            // projector.state.buffering = true;
            projector->CreateAsycLoadingThread();
            projector->message.new_load_path_queued = false;
        }


        projector->Update();


        // HIT FPS
        // todo: do we actually want to hit a certain fps like a game?
        // or accurately track our continuous audio timer...
        // (eg if we're late one frame, go early the next?) hmm
        double dt = GetWallClockSeconds() - timeLastFrame;
        double targetSec = projector->state.targetSecPerFrame;
        if (dt < targetSec && targetSec != 0)
        {
            Sleep((targetSec - dt) * 1000.0);
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
    OutputDebugString("Ending churn thread...\n");
    return 0;
}












