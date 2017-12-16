





static HANDLE global_asyn_load_thread;


// todo: move into a system obj
// static char *global_title_buffer;
const int TITLE_BUFFER_SIZE = 256;

const int URL_BUFFER_SIZE = 1024;  // todo: what to use for this?





// progress bar position
const int PROGRESS_BAR_H = 22;
const int PROGRESS_BAR_B = 0;

// hide progress bar after this many ms
const double PROGRESS_BAR_TIMEOUT = 1000;


// how long to leave messages on screen (including any fade time ATM)
const double MS_TO_DISPLAY_MSG = 3000;






#include "util.h"

#include "urls.h"


#include "ffmpeg.cpp"

#include "sdl.cpp"

#include "timer.cpp"




struct RollingMovie
{

    MovieReel reel;

    // better place for these?
    struct SwsContext *sws_context;
    AVFrame *frame_output;

    double duration;
    double elapsed;

    // todo: not used anymore
    // if we need another timer, could probably rename this and comment it back it
    // but might want to set it each frame to the ts_audio that we use for syncing a/v
    // Stopwatch audio_stopwatch;

    double aspect_ratio; // feels like it'd be better to store this as a rational

    i64 ptsOfLastVideo;
    i64 ptsOfLastAudio;

    // lines between runnningmovie and appstate are blurring a bit to me right now
    bool is_paused = false;
    bool was_paused = false;

    double targetMsPerFrame;

    char *cached_url;

    u8 *vid_buffer;
    int vidWID;
    int vidHEI;
};


// todo: split into appstate and moviestate? rename RollingMovie to moviestate? rename state to app_state? or win_state?

struct AppState {

    bool lock_aspect = true;
    bool repeat = true;

    double volume = 1.0;

    bool bufferingOrLoading = true;

    Timer app_timer;

    char *exe_directory;

};



struct AppMessages
{


    bool setSeek = false;
    double seekProportion = 0;

    bool loadNewMovie = false;

    int startAtSeconds = 0;


    char file_to_load[1024]; // todo what max
    bool load_new_file = false;
    void QueueLoadMovie(char *path)
    {
        strcpy_s(file_to_load, 1024, path);
        load_new_file = true;
    }

    void QueuePlayRandom()
    {
        int r = getUnplayedIndex();
        QueueLoadMovie(RANDOM_ON_LAUNCH[r]);
    }


};





// todo: kind of a mess, hard to initialize with its dependence on timeBase and fps
struct timestamp
{
    i64 secondsInTimeBase; // this/timeBase = seconsd.. eg when this = timeBase, it's 1 second
    i64 timeBase;
    double framesPerSecond;

    double seconds()
    {
        return (double)secondsInTimeBase / (double)timeBase;
    }
    double ms()
    {
        return seconds() * 1000.0;
    }
    double frame()
    {
        return seconds() * framesPerSecond;
    }
    double i64InUnits(i64 base)
    {
        return nearestI64(((double)secondsInTimeBase / (double)timeBase) * (double)base);
    }

    static timestamp FromPTS(i64 pts, AVFormatContext *fc, int streamIndex, RollingMovie movie)
    {
        i64 base_num = fc->streams[streamIndex]->time_base.num;
        i64 base_den = fc->streams[streamIndex]->time_base.den;
        return {pts * base_num, base_den, movie.targetMsPerFrame};
    }
    static timestamp FromVideoPTS(RollingMovie movie)
    {
        return timestamp::FromPTS(movie.ptsOfLastVideo, movie.reel.vfc, movie.reel.video.index, movie);
    }
    static timestamp FromAudioPTS(RollingMovie movie)
    {
        return timestamp::FromPTS(movie.ptsOfLastAudio, movie.reel.afc, movie.reel.audio.index, movie);
    }
};


// called "hard" because we flush the buffers and may have to grab a few frames to get the right one
void HardSeekToFrameForTimestamp(RollingMovie *movie, timestamp ts, double msAudioLatencyEstimate)
{
    // todo: measure the time this function takes and debug output it

    // not entirely sure if this flush usage is right
    if (movie->reel.video.codecContext) avcodec_flush_buffers(movie->reel.video.codecContext);
    if (movie->reel.audio.codecContext) avcodec_flush_buffers(movie->reel.audio.codecContext);

    i64 seekPos = ts.i64InUnits(AV_TIME_BASE);

    // AVSEEK_FLAG_BACKWARD = find first I-frame before our seek position
    if (movie->reel.video.codecContext) av_seek_frame(movie->reel.vfc, -1, seekPos, AVSEEK_FLAG_BACKWARD);
    if (movie->reel.audio.codecContext) av_seek_frame(movie->reel.afc, -1, seekPos, AVSEEK_FLAG_BACKWARD);


    // todo: special if at start of file?

    // todo: what if we seek right to an I-frame? i think that would still work,
    // we'd have to pull at least 1 frame to have something to display anyway

    double realTimeMs = ts.seconds() * 1000.0; //(double)seekPos / (double)AV_TIME_BASE;
    // double msSinceAudioStart = movie->audio_stopwatch.MsElapsed();
        // char msbuf[123];
        // sprintf(msbuf, "msSinceAudioStart: %f\n", msSinceAudioStart);
        // OutputDebugString(msbuf);


    // movie->audio_stopwatch.SetMsElapsedFromSeconds(ts.seconds());


    // step through frames for both contexts until we reach our desired timestamp

    int frames_skipped;
    GetNextVideoFrame(
        movie->reel.vfc,
        movie->reel.video.codecContext,
        movie->sws_context,
        movie->reel.video.index,
        movie->frame_output,
        ts.seconds() * 1000.0,// - msAudioLatencyEstimate,
        0,
        true,
        &movie->ptsOfLastVideo,
        &frames_skipped);


    // kinda awkward
    SoundBuffer dummyBufferJunkData;
    dummyBufferJunkData.data = (u8*)malloc(1024 * 10);
    dummyBufferJunkData.size_in_bytes = 1024 * 10;
    int bytes_queued_up = GetNextAudioFrame(
        movie->reel.afc,
        movie->reel.audio.codecContext,
        movie->reel.audio.index,
        dummyBufferJunkData,
        1024,
        realTimeMs,
        &movie->ptsOfLastAudio);
    free(dummyBufferJunkData.data);


    i64 streamIndex = movie->reel.video.index;
    i64 base_num = movie->reel.vfc->streams[streamIndex]->time_base.num;
    i64 base_den = movie->reel.vfc->streams[streamIndex]->time_base.den;
    timestamp currentTS = {movie->ptsOfLastVideo * base_num, base_den, ts.framesPerSecond};

    double totalFrameCount = (movie->reel.vfc->duration / (double)AV_TIME_BASE) * (double)ts.framesPerSecond;
    double durationSeconds = movie->reel.vfc->duration / (double)AV_TIME_BASE;

        // char morebuf[123];
        // sprintf(morebuf, "dur (s): %f * fps: %f = %f frames\n", durationSeconds, ts.framesPerSecond, totalFrameCount);
        // OutputDebugString(morebuf);

        // char morebuf2[123];
        // sprintf(morebuf2, "dur: %lli / in base: %i\n", movie->reel.vfc->duration, AV_TIME_BASE);
        // OutputDebugString(morebuf2);

        // char ptsbuf[123];
        // sprintf(ptsbuf, "at: %lli / want: %lli of %lli\n",
        //         nearestI64(currentTS.frame())+1,
        //         nearestI64(ts.frame())+1,
        //         nearestI64(totalFrameCount));
        // OutputDebugString(ptsbuf);

}


// // todo: what to do with these?
// textured_quad movie_screen;
// textured_quad progress_gray;
// textured_quad progress_red;



bool SwapInNewReel(MovieReel *movie, RollingMovie *outMovie);

void SetWindowToAspectRatio(HWND hwnd, double aspect_ratio);



struct GhosterWindow
{

    AppState state;
    // AppSystemState system;

    SoundBuffer ffmpeg_to_sdl_buffer;
    SoundBuffer volume_adjusted_buffer;

    SDLStuff sdl_stuff;

    RollingMovie rolling_movie;
    MovieReel next_reel;

    double msLastFrame; // todo: replace this with app timer? make timer usage more obvious


    // mostly flags, basic way to communicate between threads etc
    AppMessages message;


    // MessageOverlay debug_overlay;
    // MessageOverlay splash_overlay;


    void appPlay(bool userRequest = true)
    {
        if (userRequest)
        {
            // QueueNewSplash("Play", 0x7cec7aff);
        }
        // rolling_movie.audio_stopwatch.Start();
        if (rolling_movie.reel.IsAudioAvailable())
            SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)false);
        rolling_movie.is_paused = false;
    }

    void appPause(bool userRequest = true)
    {
        if (userRequest)
        {
            // QueueNewSplash("Pause", 0xfa8686ff);
        }
        // rolling_movie.audio_stopwatch.Pause();
        SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)true);
        rolling_movie.is_paused = true;
    }

    void appTogglePause(bool userRequest = true)
    {
        rolling_movie.is_paused = !rolling_movie.is_paused;

        if (rolling_movie.is_paused && !rolling_movie.was_paused)
        {
            appPause(userRequest);
        }
        if (!rolling_movie.is_paused)
        {
            if (rolling_movie.was_paused)// || !rolling_movie.audio_stopwatch.timer.started)
            {
                appPlay(userRequest);
            }
        }

        rolling_movie.was_paused = rolling_movie.is_paused;
    }



    // bool clientPointIsOnProgressBar(int x, int y)
    // {
    //     return y >= system.winHEI-(PROGRESS_BAR_H+PROGRESS_BAR_B) &&
    //            y <= system.winHEI-PROGRESS_BAR_B;
    // }

    // void appSetProgressBar(int clientX, int clientY)
    // {
    //     if (clientPointIsOnProgressBar(clientX, clientY)) // check here or outside?
    //     {
    //         double prop = (double)clientX / (double)system.winWID;

    //         message.setSeek = true;
    //         message.seekProportion = prop;
    //     }
    // }

    void setVolume(double volume)
    {
        if (volume < 0) volume = 0;
        if (volume > 1) volume = 1;
        state.volume = volume;
    }


    // void EmptyMsgQueue()
    // {
    //     ClearScrollingDisplay(debug_overlay.text.memory);
    // }
    // void QueueNewMsg(POINT val, char *msg, u32 col = 0xff888888)
    // {
    //     char buf[123];
    //     sprintf(buf, "%s: %i, %i\n", msg, val.x, val.y);

    //     AddToScrollingDisplay(buf, debug_overlay.text.memory);
    // }
    // void QueueNewMsg(double val, char *msg, u32 col = 0xff888888)
    // {
    //     char buf[123];
    //     sprintf(buf, "%s: %f\n", msg, val);

    //     AddToScrollingDisplay(buf, debug_overlay.text.memory);
    // }
    // void QueueNewMsg(bool val, char *msg, u32 col = 0xff888888)
    // {
    //     char buf[123];
    //     sprintf(buf, "%s: %i\n", msg, val);

    //     AddToScrollingDisplay(buf, debug_overlay.text.memory);
    // }
    // void QueueNewMsg(char *msg, u32 col = 0xff888888)
    // {
    //     AddToScrollingDisplay(msg, debug_overlay.text.memory);
    // }

    // void QueueNewSplash(char *msg, u32 col = 0xff888888)
    // {
    //     TransmogrifyTextInto(splash_overlay.text.memory, msg); // todo: check length somehow hmm...

    //     splash_overlay.msLeftOfDisplay = MS_TO_DISPLAY_MSG;
    //     // message.splashBackgroundCol.hex = col;

    // }
    // void ClearCurrentSplash()
    // {
    //     QueueNewSplash("", 0x0);  // no message
    //     splash_overlay.msLeftOfDisplay = -1;
    // }


    void LoadNewMovie()
    {
        OutputDebugString("Ready to load new movie...\n");

        // ClearCurrentSplash();

        if (!SwapInNewReel(&next_reel, &rolling_movie))
        {
            // assert(false);
            MessageBox(0,"Error in setup for new movie.\n",0,0);
            // QueueNewMsg("invalid file", 0x7676eeff);
        }

        // if (system.fullscreen)
        // {
        //     // kind of hacky solution
        //     setFullscreen(false);
        //     SetWindowToAspectRatio(system.window, rolling_movie.aspect_ratio);
        //     setFullscreen(true);  // we need to set this anyway in the case that we launch fullscreen
        // }
        // else
        {
            // SetWindowToAspectRatio(system.window, rolling_movie.aspect_ratio);
        }


        if (message.startAtSeconds != 0)
        {
            double videoFPS = 1000.0 / rolling_movie.targetMsPerFrame;
            timestamp ts = {message.startAtSeconds, 1, videoFPS};
            HardSeekToFrameForTimestamp(&rolling_movie, ts, sdl_stuff.estimated_audio_latency_ms);
        }

        state.bufferingOrLoading = false;
        appPlay(false);

        // // need to recreate wallpaper window basically
        // if (state.wallpaperMode)
        //     setWallpaperMode(system.window, state.wallpaperMode);
    }


    // void UpdateWindowSize(int wid, int hei)
    // {
    //     system.winWID = wid;
    //     system.winHEI = hei;
    //     // message.resizeWindowBuffers = true;
    // }

    void Init(char *exedir)
    {

        InitAV();  // basically just registers all codecs..

        state.app_timer.Start();  // now started in ghoster.init


        state.exe_directory = exedir;


        rolling_movie.targetMsPerFrame = 30; // for before video loads

        // debug_overlay.Allocate(system.winWID, system.winHEI, 1024*5); // todo: add length checks during usage
        // splash_overlay.Allocate(system.winWID, system.winHEI, 1024*5);

        // debug_overlay.bgAlpha = 125;
        // splash_overlay.bgAlpha = 0;

        // todo: move this to ghoster app
        // space we can re-use for title strings
        // global_title_buffer = (char*)malloc(TITLE_BUFFER_SIZE); //remember this includes space for \0

        // maybe alloc ghoster app all at once? is this really the only mem we need for it?
        rolling_movie.cached_url = (char*)malloc(URL_BUFFER_SIZE);
    }


    // now running this on a sep thread from our msg loop so it's independent of mouse events / captures
    void Update()
    {

        // needed when using trackpopupmenu for our context menu
        // if (system.contextMenuOpen &&
        //     state.menuCloseTimer.started &&
        //     state.menuCloseTimer.MsSinceStart() > 300)
        // {
        //     system.contextMenuOpen = false;
        // }


        // todo: replace this with the-one-dt-to-rule-them-all, maybe from app_timer
        double temp_dt = state.app_timer.MsSinceStart() - msLastFrame;
        msLastFrame = state.app_timer.MsSinceStart();




        // EmptyMsgQueue();
        // QueueNewMsg(system.contextMenuOpen, "system.contextMenuOpen");
        // QueueNewMsg(system.mDownPoint, "system.mDownPoint");
        // QueueNewMsg(system.mDown, "system.mDown");
        // // QueueNewMsg(system.ctrlDown, "system.ctrlDown");
        // QueueNewMsg(system.clickingOnProgressBar, "system.clickingOnProgressBar");
        // QueueNewMsg(system.mouseHasMovedSinceDownL, "system.mouseHasMovedSinceDownL");
        // QueueNewMsg(system.msOfLastMouseMove, "system.msOfLastMouseMove");
        // QueueNewMsg(state.app_timer.MsSinceStart(), "state.app_timer.MsSinceStart()");
        // QueueNewMsg(" ");
        // // for (int i = 0; i < alreadyPlayedCount; i++)
        // // {
        // //     QueueNewMsg((double)alreadyPlayedRandos[i], "alreadyPlayed");
        // // }
        // QueueNewMsg(global_popup_window, "global_popup_window");
        // QueueNewMsg(global_icon_menu_window, "global_icon_menu_window");
        // QueueNewMsg((double)selectedItem, "selectedItem");
        // QueueNewMsg((double)subMenuSelectedItem, "subMenuSelectedItem");
        // QueueNewMsg(global_is_submenu_shown, "global_is_submenu_shown");
        // QueueNewMsg(message.next_mup_was_closing_menu, "message.next_mup_was_closing_menu");
        // QueueNewMsg(" ");





        if (message.loadNewMovie)
        {
            message.loadNewMovie = false;
            LoadNewMovie();
        }




        // POINT mPos;   GetCursorPos(&mPos);
        // RECT winRect; GetWindowRect(system.window, &winRect);
        // if (!PtInRect(&winRect, mPos))
        // {
        //     // // OutputDebugString("mouse outside window\n");
        //     // drawProgressBar = false;

        //     // if we mouse up while not on window all our mdown etc flags will be wrong
        //     // so we just force an "end of click" when we leave the window
        //     system.mDown = false;
        //     system.mouseHasMovedSinceDownL = false;
        //     system.clickingOnProgressBar = false;
        // }



        // // awkward way to detect if mouse leaves the menu (and hide highlighting)
        // GetWindowRect(global_icon_menu_window, &winRect);
        // if (!PtInRect(&winRect, mPos)) {
        //     subMenuSelectedItem = -1;
        //     RedrawWindow(global_icon_menu_window, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
        // }
        // GetWindowRect(global_popup_window, &winRect);
        // if (!PtInRect(&winRect, mPos)) {
        //     selectedItem = -1;
        //     RedrawWindow(global_popup_window, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
        // }


        double percent; // make into method?


        // state.bufferingOrLoading = true;
        if (state.bufferingOrLoading)
        {
            appPause(false);
        }
        else
        {

            if (message.setSeek)
            {
                SDL_ClearQueuedAudio(sdl_stuff.audio_device);

                message.setSeek = false;
                int seekPos = message.seekProportion * rolling_movie.reel.vfc->duration;


                double videoFPS = 1000.0 / rolling_movie.targetMsPerFrame;
                    // char fpsbuf[123];
                    // sprintf(fpsbuf, "fps: %f\n", videoFPS);
                    // OutputDebugString(fpsbuf);

                timestamp ts = {nearestI64(message.seekProportion*rolling_movie.reel.vfc->duration), AV_TIME_BASE, videoFPS};

                HardSeekToFrameForTimestamp(&rolling_movie, ts, sdl_stuff.estimated_audio_latency_ms);

            }


            if (!rolling_movie.is_paused)
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
                    int bytes_queued_up = GetNextAudioFrame(
                        rolling_movie.reel.afc,
                        rolling_movie.reel.audio.codecContext,
                        rolling_movie.reel.audio.index,
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

                timestamp ts_audio = timestamp::FromAudioPTS(rolling_movie);

                // if no audio, use video pts (we should basically never skip or repeat in this case)
                if (!rolling_movie.reel.IsAudioAvailable())
                {
                    ts_audio = timestamp::FromVideoPTS(rolling_movie);
                }

                // use ts audio to get track bar position
                rolling_movie.elapsed = ts_audio.seconds();
                percent = rolling_movie.elapsed/rolling_movie.duration;

                // assuming we've filled the sdl buffer, we are 1 second ahead
                // but is that actually accurate? should we instead use SDL_GetQueuedAudioSize again to est??
                // and how consistently do we pull audio data? is it sometimes more than others?
                // update: i think we always put everything we get from decoding into sdl queue,
                // so sdl buffer should be a decent way to figure out how far our audio decoding is ahead of "now"
                double aud_seconds = ts_audio.seconds() - seconds_left_in_queue;
                    // char audbuf[123];
                    // sprintf(audbuf, "raw: %.1f  aud_seconds: %.1f  seconds_left_in_queue: %.1f\n",
                    //         ts_audio.seconds(), aud_seconds, seconds_left_in_queue);
                    // OutputDebugString(audbuf);


                timestamp ts_video = timestamp::FromVideoPTS(rolling_movie);
                double vid_seconds = ts_video.seconds();

                double estimatedVidPTS = vid_seconds + rolling_movie.targetMsPerFrame/1000.0;


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

                // time for a new frame if audio is this far behind
                if (aud_seconds > estimatedVidPTS - allowableAudioLag/1000.0
                    || !rolling_movie.reel.IsAudioAvailable()
                )
                {
                    int frames_skipped = 0;
                    GetNextVideoFrame(
                        rolling_movie.reel.vfc,
                        rolling_movie.reel.video.codecContext,
                        rolling_movie.sws_context,
                        rolling_movie.reel.video.index,
                        rolling_movie.frame_output,
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

                }
                else
                {
                    OutputDebugString("repeating a frame\n");
                }


                ts_video = timestamp::FromVideoPTS(rolling_movie);
                vid_seconds = ts_video.seconds();


                // how far ahead is the sound?
                double delta_ms = (aud_seconds - vid_seconds) * 1000.0;

                // the real audio being transmitted is sdl's write buffer which will be a little behind
                double aud_sec_corrected = aud_seconds - estimatedSDLAudioLag/1000.0;
                double delta_with_correction = (aud_sec_corrected - vid_seconds) * 1000.0;

                // char ptsbuf[123];
                // sprintf(ptsbuf, "vid ts: %.1f, aud ts: %.1f, delta ms: %.1f, correction: %.1f\n",
                //         vid_seconds, aud_seconds, delta_ms, delta_with_correction);
                // OutputDebugString(ptsbuf);

            }
        }



        // debug_overlay.bitmap.SetFromText(system.window, debug_overlay.text.memory, 20, {0xffffffff}, {0x00000000}, false);

        // if (state.displayDebugText)
        // {
        //     debug_overlay.alpha = 1;
        //     debug_overlay.msLeftOfDisplay = 1;
        // }
        // else
        // {
        //     debug_overlay.alpha = 0;
        //     debug_overlay.msLeftOfDisplay = 0;
        // }


        // // splash_overlay.bitmap.SetFromText(system.window, splash_overlay.text.memory, 36, {0xffffffff}, message.splashBackgroundCol, true);
        // splash_overlay.alpha = 0;
        // if (splash_overlay.msLeftOfDisplay > 0)
        // {
        //     splash_overlay.msLeftOfDisplay -= temp_dt;
        //     double maxA = 0.65; // implicit min of 0
        //     splash_overlay.alpha = ((-cos(splash_overlay.msLeftOfDisplay*M_PI / MS_TO_DISPLAY_MSG) + 1) / 2) * maxA;
        // }



        // HWND destWin = system.window;
        // if (state.wallpaperMode)
        // {
        //     destWin = global_wallpaper_window;
        // }



        // rendering was here



        // REPEAT

        if (state.repeat && percent > 1.0)  // note percent will keep ticking up even after vid is done
        {
            double targetFPS = 1000.0 / rolling_movie.targetMsPerFrame;
            HardSeekToFrameForTimestamp(&rolling_movie, {0,1,targetFPS}, sdl_stuff.estimated_audio_latency_ms);
        }



        // HIT FPS

        // something seems off with this... ? i guess it's, it's basically ms since END of last frame
        double dt = state.app_timer.MsSinceLastFrame();

        // todo: we actually don't want to hit a certain fps like a game,
        // but accurately track our continuous audio timer
        // (eg if we're late one frame, go early the next?)

        if (dt < rolling_movie.targetMsPerFrame)
        {
            double msToSleep = rolling_movie.targetMsPerFrame - dt;
            Sleep(msToSleep);
            while (dt < rolling_movie.targetMsPerFrame)  // is this weird?
            {
                dt = state.app_timer.MsSinceLastFrame();
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
                    rolling_movie.targetMsPerFrame, dt);
            OutputDebugString(msg);
        }
        state.app_timer.EndFrame();  // make sure to call for MsSinceLastFrame() to work.. feels weird

    }


};


static GhosterWindow global_ghoster;


// // char last_splash[1024];

// textured_quad splash_quad;
// textured_quad debug_quad;

// double last_render_time;
// int cachedW;
// int cachedH;
// void the_one_render_call_to_rule_them_all(GhosterWindow app)
// {
//     if (!app.state.appRunning) return;  // kinda smells

//     RECT wr; GetWindowRect(app.system.window, &wr);
//     int sw = wr.right-wr.left;
//     int sh = wr.bottom-wr.top;

//     if (sw!=cachedW || sh!=cachedH)
//     {
//         cachedW = sw;
//         cachedH = sh;
//         r_resize(sw, sh);
//     }

//     // todo: replace this with the-one-dt-to-rule-them-all, maybe from app_timer
//     double temp_dt = app.state.app_timer.MsSinceStart() - last_render_time;
//     last_render_time = app.state.app_timer.MsSinceStart();


//     // use ts audio to get track bar position
//     // app.rolling_movie.elapsed = ts_audio.seconds();
//     double percent = app.rolling_movie.elapsed/app.rolling_movie.duration;


//     bool drawProgressBar;
//     if (app.state.app_timer.MsSinceStart() - app.system.msOfLastMouseMove > PROGRESS_BAR_TIMEOUT
//         && !app.system.clickingOnProgressBar)
//     {
//         drawProgressBar = false;
//     }
//     else
//     {
//         drawProgressBar = true;
//     }

//     POINT mPos;   GetCursorPos(&mPos);
//     RECT winRect; GetWindowRect(app.system.window, &winRect);
//     if (!PtInRect(&winRect, mPos))
//     {
//         // OutputDebugString("mouse outside window\n");
//         drawProgressBar = false;
//     }


//     static float t = 0;
//     if (app.state.bufferingOrLoading)
//     {
//         t += temp_dt;
//         // float col = sin(t*M_PI*2 / 100);
//         // col = (col + 1) / 2; // 0-1
//         // col = 0.9*col + 0.4*(1-col); //lerp

//         // e^sin(x) very interesting shape, via
//         // http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
//         float col = pow(M_E, sin(t*M_PI*2 / 3000));  // cycle every 3000ms
//         float min = 1/M_E;
//         float max = M_E;
//         col = (col-min) / (max-min); // 0-1
//         col = 0.75*col + 0.2*(1-col); //lerp

//         r_clear(col, col, col, 1);  // r g b a
//     }
//     else
//     {
//         r_clear();

//         if (app.state.lock_aspect && app.system.fullscreen)
//         {
//             // todo: is it better to change the vbo or the viewport? maybe doesn't matter?
//             // certainly seems easier to change viewport
//             // update: now we're changing the verts
//             RECT subRect = r_CalcLetterBoxRect(app.system.winWID, app.system.winHEI, app.rolling_movie.aspect_ratio);
//             float ndcL = r_PixelToNDC(subRect.left,   app.system.winWID);
//             float ndcR = r_PixelToNDC(subRect.right,  app.system.winWID);
//             float ndcT = r_PixelToNDC(subRect.top,    app.system.winHEI);
//             float ndcB = r_PixelToNDC(subRect.bottom, app.system.winHEI);
//             movie_screen.update(app.rolling_movie.vid_buffer, 960,720,  ndcL, ndcT, ndcR, ndcB);
//         }
//         else
//         {
//             movie_screen.update(app.rolling_movie.vid_buffer, 960,720);
//         }
//         movie_screen.render();

//         if (drawProgressBar)
//         {
//             int progress_bar_t = PROGRESS_BAR_H+PROGRESS_BAR_B;
//             double ndcB = r_PixelToNDC(PROGRESS_BAR_B, app.system.winHEI);
//             double ndcH = r_PixelToNDC(progress_bar_t, app.system.winHEI);

//             double neg1_to_1 = percent*2.0 - 1.0;

//             u32 gray = 0xffaaaaaa;
//             progress_gray.update((u8*)&gray, 1,1,  neg1_to_1, ndcB, 1, ndcH);
//             progress_gray.render(0.4);

//             u32 red = 0xffff0000;
//             progress_red.update((u8*)&red, 1,1,  -1, ndcB, neg1_to_1, ndcH);
//             progress_red.render(0.6);
//         }
//     }

//     // tt_print(system.winWID/2, system.winHEI/2, "H E L L O   W O R L D", system.winWID, system.winHEI);

//     // // todo: improve the args needed for these calls?
//     // r_render_msg(app.debug_overlay, 32, 0,0, app.system.winWID,app.system.winHEI, false, false);
//     // r_render_msg(app.splash_overlay, 64, app.system.winWID/2,app.system.winHEI/2, app.system.winWID,app.system.winHEI);

//     // if (strcmp(app.splash_overlay.text.memory, last_splash) != 0)
//     // {

//         debug_quad.destroy();
//         splash_quad.destroy();

//         debug_quad = r_create_msg(app.debug_overlay, 32, false, false);
//         splash_quad = r_create_msg(app.splash_overlay, 64);

//         // strcpy(last_splash, app.splash_overlay.text.memory);
//     // }

//     debug_quad.move_to_pixel_coords_TL(0, 0, sw, sh);
//     splash_quad.move_to_pixel_coords_center(sw/2, sh/2, sw, sh);

//     debug_quad.render(app.debug_overlay.alpha);
//     splash_quad.render(app.splash_overlay.alpha);

//     r_swap();
// }



bool GetStringFromYoutubeDL(char *url, char *options, char *outString)
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
    sprintf(youtube_dl_path, "%syoutube-dl.exe", global_ghoster.state.exe_directory);

    char args[MAX_PATH]; //todo: tempy
    sprintf(args, "%syoutube-dl.exe %s %s", global_ghoster.state.exe_directory, options, url);
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


bool FindAudioAndVideoUrls(char *path, char *video, char *audio, char *outTitle)
{

    char *tempString = (char*)malloc(1024*30); // big enough for messy urls

    // -g gets urls (seems like two: video then audio)
    if (!GetStringFromYoutubeDL(path, "--get-title -g", tempString))
    {
        return false;
    }

    char *segments[3];

    int count = 0;
    segments[count++] = tempString;
    for (char *c = tempString; *c; c++)
    {
        if (*c == '\n')
        {
            *c = '\0';

            if (count < 3)
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
    if (titleLen+1 > TITLE_BUFFER_SIZE-1) // note -1 since [size-1] is \0  and +1 since titleLen doesn't count \0
    {
        segments[0][TITLE_BUFFER_SIZE-1] = '\0';
        segments[0][TITLE_BUFFER_SIZE-2] = '.';
        segments[0][TITLE_BUFFER_SIZE-3] = '.';
        segments[0][TITLE_BUFFER_SIZE-4] = '.';
    }


    strcpy_s(outTitle, TITLE_BUFFER_SIZE, segments[0]);

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
bool LoadMovieReelFromPath(char *path, MovieReel *newMovie)
{
    char loadingMsg[1234];
    sprintf(loadingMsg, "\nLoading %s\n", path);
    OutputDebugString(loadingMsg);

    // todo: check limits on title before writing here and below
    char *outTitle = newMovie->title;
    strcpy_s(outTitle, TITLE_BUFFER_SIZE, "[no title]");

    if (StringIsUrl(path))
    {
        char *video_url = (char*)malloc(1024*10);  // big enough for some big url from youtube-dl
        char *audio_url = (char*)malloc(1024*10);  // todo: mem leak if we kill this thread before free()
        if(FindAudioAndVideoUrls(path, video_url, audio_url, outTitle))
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
        strcpy_s(outTitle, TITLE_BUFFER_SIZE, fileNameOnly); // todo: what length to use?
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


// todo: peruse this for memory leaks. also: better name!
// done and done, but maybe another pass over both

// make into a moveifile / staticmovie method?
bool SwapInNewReel(MovieReel *newMovie, RollingMovie *outMovie)
{

    // swap reels
    outMovie->reel.TransferFromReel(newMovie);


    // temp pointer for the rest of this function
    MovieReel *movie = &outMovie->reel;


    // set window size on video source resolution
    if (movie->video.codecContext)
    {
        outMovie->vidWID = movie->video.codecContext->width;
        outMovie->vidHEI = movie->video.codecContext->height;
    }
    else
    {
        outMovie->vidWID = 400;
        outMovie->vidHEI = 400;
    }
        // char hwbuf[123];
        // sprintf(hwbuf, "wid: %i  hei: %i\n", movie->video.codecContext->width, movie->video.codecContext->height);
        // OutputDebugString(hwbuf);

    // RECT winRect;
    // GetWindowRect(global_ghoster.system.window, &winRect);
    // //keep top left of window in same pos for now, change to keep center in same position?
    // MoveWindow(global_ghoster.system.window, winRect.left, winRect.top, global_ghoster.system.winWID, global_ghoster.system.winHEI, true);  // ever non-zero opening position? launch option?

    outMovie->aspect_ratio = (double)outMovie->vidWID / (double)outMovie->vidHEI;



    // MAKE NOTE OF VIDEO LENGTH

    // todo: add handling for this
    // edit: somehow we get this far even on a text file with no video or audio streams??
    assert(movie->vfc->start_time==0);
        // char rewq[123];
        // sprintf(rewq, "start: %lli\n", start_time);
        // OutputDebugString(rewq);


    OutputDebugString("\nvideo format ctx:\n");
    logFormatContextDuration(movie->vfc);
    OutputDebugString("\naudio format ctx:\n");
    logFormatContextDuration(movie->afc);

    outMovie->duration = (double)movie->vfc->duration / (double)AV_TIME_BASE;
    outMovie->elapsed = 0;

    // outMovie->audio_stopwatch.ResetCompletely();


    // SET FPS BASED ON LOADED VIDEO

    double targetFPS;
    char vidfps[123];
    if (movie->video.codecContext)
    {
        targetFPS = ((double)movie->video.codecContext->time_base.den /
                    (double)movie->video.codecContext->time_base.num) /
                    (double)movie->video.codecContext->ticks_per_frame;

        sprintf(vidfps, "\nvideo frame rate: %i / %i  (%.2f FPS)\nticks_per_frame: %i\n",
            movie->video.codecContext->time_base.num,
            movie->video.codecContext->time_base.den,
            targetFPS,
            movie->video.codecContext->ticks_per_frame
        );
    }
    else
    {
        targetFPS = 30;
        sprintf(vidfps, "\nno video found, default to %.2f fps\n", targetFPS);
    }

    OutputDebugString(vidfps);


    outMovie->targetMsPerFrame = 1000.0 / targetFPS;






    // SDL, for sound atm

    if (movie->audio.codecContext)
    {
        SetupSDLSoundFor(movie->audio.codecContext, &global_ghoster.sdl_stuff, targetFPS);

        SetupSoundBuffer(movie->audio.codecContext, &global_ghoster.ffmpeg_to_sdl_buffer);
        SetupSoundBuffer(movie->audio.codecContext, &global_ghoster.volume_adjusted_buffer);
    }




    // MORE FFMPEG

    // AVFrame *
    if (outMovie->frame_output) av_frame_free(&outMovie->frame_output);
    outMovie->frame_output = av_frame_alloc();  // just metadata

    if (!outMovie->frame_output)
    {
        LogError("ffmpeg: Couldn't alloc frame");
        return false;
    }


    // actual mem for frame
    int numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, 960,720); // todo: extra bar bug qwer
    // int numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, outMovie->vidWID, outMovie->vidHEI);
    if (outMovie->vid_buffer) av_free(outMovie->vid_buffer);
    outMovie->vid_buffer = (u8*)av_malloc(numBytes);



    // set up frame to use buffer memory...
    // avpicture_fill(  // deprecated
    //     (AVPicture *)outMovie->frame_output,
    //     outMovie->vid_buffer,
    //     AV_PIX_FMT_RGB32,
    //     // 540,
    //     // 320);
    //     outMovie->vidWID,
    //     outMovie->vidHEI);
    av_image_fill_arrays(
         outMovie->frame_output->data,
         outMovie->frame_output->linesize,
         outMovie->vid_buffer,
         AV_PIX_FMT_RGB32,
        960,
        720,
        // outMovie->vidWID,
        // outMovie->vidHEI,
        1);

    // for converting frame from file to a standard color format buffer (size doesn't matter so much)
    if (outMovie->sws_context) sws_freeContext(outMovie->sws_context);
    // outMovie->sws_context = sws_alloc_context(); // this instead of {0}? doesnt seem to be needed?
    outMovie->sws_context = {0};
    // outMovie->sws_context = sws_getContext(  // deprecated
    //     movie->video.codecContext->width,
    //     movie->video.codecContext->height,
    //     movie->video.codecContext->pix_fmt,
    //     // 540,
    //     // 320,
    //     outMovie->vidWID,
    //     outMovie->vidHEI,
    //     AV_PIX_FMT_RGB32,
    //     SWS_BILINEAR,
    //     0, 0, 0);
    if (movie->video.codecContext)
    {
        // this seems to be no help in our extra bar issue
        outMovie->sws_context = sws_getCachedContext(
            outMovie->sws_context,
            movie->video.codecContext->width,
            movie->video.codecContext->height,
            movie->video.codecContext->pix_fmt,
            960,
            720,
            // outMovie->vidWID,
            // outMovie->vidHEI,
            AV_PIX_FMT_RGB32,
            SWS_BILINEAR,
            0, 0, 0);
    }


    // char linbuf[123];
    // sprintf(linbuf, "linesize: %i\n", *outMovie->frame_output->linesize);
    // OutputDebugString(linbuf);



    // get first frame in case we are paused
    HardSeekToFrameForTimestamp(outMovie, {0,1,targetFPS}, global_ghoster.sdl_stuff.estimated_audio_latency_ms);


    return true;

}



DWORD WINAPI AsyncMovieLoad( LPVOID lpParam )
{
    char *path = (char*)lpParam;

    // todo: move title into MovieReel? rename to movieFile or something?
    // char *title = global_title_buffer;

    // MovieReel newMovie;
    if (!LoadMovieReelFromPath(path, &global_ghoster.next_reel))
    {
        // now we get more specific error msg in function call,
        // for now don't override them with a new (since our queue is only 1 deep)
        // char errbuf[123];
        // sprintf(errbuf, "Error creating movie source from path:\n%s\n", path);
        LogError("load failed");
        return false;
    }

    // todo: better place for this? i guess it might be fine
    // SetTitle(global_ghoster.system.window, global_ghoster.next_reel.title);

    // global_ghoster.message.newMovieToRun = DeepCopyMovieReel(newMovie);
    global_ghoster.message.loadNewMovie = true;

    return 0;
}



bool CreateNewMovieFromPath(char *path)
{
    // try waiting on this until we confirm it's a good path/file
    // global_ghoster.state.bufferingOrLoading = true;
    // global_ghoster.appPause(false); // stop playing movie as well, we'll auto start the next one
    // SplashMessage("fetching...", 0xaaaaaaff);

    char *timestamp = strstr(path, "&t=");
    if (timestamp == 0) timestamp = strstr(path, "#t=");
    if (timestamp != 0) {
        int startSeconds = SecondsFromStringTimestamp(timestamp);
        global_ghoster.message.startAtSeconds = startSeconds;
            // char buf[123];
            // sprintf(buf, "\n\n\nstart seconds: %i\n\n\n", startSeconds);
            // OutputDebugString(buf);
    }
    else
    {
        global_ghoster.message.startAtSeconds = 0; // so we don't inherit start time of prev video
    }

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

    global_asyn_load_thread = CreateThread(0, 0, AsyncMovieLoad, (void*)path, 0, 0);


    // strip off timestamp before caching path
    if (timestamp != 0)
        timestamp[0] = '\0';

    // save url for later (is rolling_movie the best place for cached_url?)
    // is this the best place to set cached_url?
    strcpy_s(global_ghoster.rolling_movie.cached_url, URL_BUFFER_SIZE, path);


    return true;
}




// todo: pass in ghoster app to run here?
DWORD WINAPI RunMainLoop( LPVOID lpParam )
{

    // LOAD FILE
    if (!global_ghoster.message.load_new_file)
    {
        global_ghoster.message.QueuePlayRandom();
    }


    // global_ghoster.state.app_timer.Start();  // now started in ghoster.init
    global_ghoster.state.app_timer.EndFrame();  // seed our first frame dt

    while (running)
    {
        // why is this outside of update?
        // maybe have message_check function or something eventually?
        // i guess it's out here because it's not really about playing a movie?
        // the whole layout of ghoster / loading / system needs to be re-worked
        if (global_ghoster.message.load_new_file)
        {
            // global_ghoster.state.buffering = true;
            CreateNewMovieFromPath(global_ghoster.message.file_to_load);
            global_ghoster.message.load_new_file = false;
        }

        global_ghoster.Update();
    }

    return 0;
}





bool PasteClipboard()
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
    global_ghoster.message.QueueLoadMovie(clipboardContents);
    free(clipboardContents);
    return true; // todo: do we need a result from loadmovie?
}


bool CopyUrlToClipboard(bool withTimestamp = false)
{
    char *url = global_ghoster.rolling_movie.cached_url;

    char output[URL_BUFFER_SIZE]; // todo: stack alloc ok here?
    if (StringIsUrl(url) && withTimestamp) {
        int secondsElapsed = global_ghoster.rolling_movie.elapsed;
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





// // todo: if we have a "videostate" struct we could just copy/restore it for these functions? hmm

// // note we're saving this every time we click down (since it could be the start of a double click)
// // so don't make this too crazy
// void saveVideoPositionForAfterDoubleClick()
// {
//     global_ghoster.message.savestate_is_saved = true;

//     // global_ghoster.rolling_movie.elapsed = global_ghoster.rolling_movie.audio_stopwatch.MsElapsed() / 1000.0;
//     // double percent = global_ghoster.rolling_movie.elapsed / global_ghoster.rolling_movie.duration;

//     // todo: this won't be sub-frame accurate
//     // but i guess if we're pausing for a split second it won't be exact anyway
//     double percent = global_ghoster.rolling_movie.elapsed/global_ghoster.rolling_movie.duration;
//     global_ghoster.message.seekProportion = percent; // todo: make new variable rather than co-opt this one?
// }

// void restoreVideoPositionAfterDoubleClick()
// {
//     if (global_ghoster.message.savestate_is_saved)
//     {
//         global_ghoster.message.savestate_is_saved = false;

//         global_ghoster.message.setSeek = true;
//     }

//     // cancel any play/pause messages (todo: could cancel other valid msgs)
//     global_ghoster.ClearCurrentSplash();
// }


