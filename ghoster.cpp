#include <windows.h>
#include <stdio.h>
#include <stdint.h> // types
#include <assert.h>
#include <math.h>


// for drag drop
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")




#define uint unsigned int

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t




char *TEST_FILES[] = {
    "D:/~phil/projects/ghoster/test-vids/testcounter30fps.webm",
    // "D:/~phil/projects/ghoster/test-vids/sync3.mp4",
    "D:/~phil/projects/ghoster/test-vids/sync1.mp4",
    "D:/~phil/projects/ghoster/test-vids/test4.mp4",
    "D:/~phil/projects/ghoster/test-vids/test.mp4",
    "D:/~phil/projects/ghoster/test-vids/test3.avi",
    "D:/~phil/projects/ghoster/test-vids/test.3gp",
    "https://www.youtube.com/watch?v=RYbe-35_BaA",
    "https://www.youtube.com/watch?v=ucZl6vQ_8Uo",
    "https://www.youtube.com/watch?v=tprMEs-zfQA",
    "https://www.youtube.com/watch?v=NAvOdjRsOoI"
};




// progress bar position
const int PROGRESS_BAR_H = 22;
const int PROGRESS_BAR_B = 0;

// hide progress bar after this many ms
const double PROGRESS_BAR_TIMEOUT = 1000;





void MsgBox(char* s) {
    MessageBox(0, s, "vid player", MB_OK);
}
// this is case sensitive
bool StringBeginsWith(const char *str, const char *front)
{
    while (*front)
    {
        if (!*str)
            return false;

        if (*front != *str)
            return false;

        front++;
        str++;
    }
    return true;
}

i64 nearestI64(double in)
{
    return floor(in + 0.5);
}


#include "ffmpeg.cpp"

#include "sdl.cpp"

#include "timer.cpp"



struct RunningMovie
{
    MovieAV av_movie;

    // better place for these?
    struct SwsContext *sws_context;
    AVFrame *frame_output;

    double duration;
    double elapsed;
    Stopwatch audio_stopwatch;
    double vid_aspect; // feels like it'd be better to store this as a rational

    i64 ptsOfLastVideo;
    i64 ptsOfLastAudio;

    // lines between runnningmovie and appstate are blurring a bit to me right now
    bool vid_paused = false;
    bool vid_was_paused = false;

    double targetMsPerFrame;

    u8 *vid_buffer;
    int vidWID;
    int vidHEI;
};


struct AppState {
    bool appRunning = true;

    double msOfLastMouseMove = -1000;
    bool clickingOnProgressBar = false;

    Timer menuCloseTimer;
    bool globalContextMenuOpen;

    bool lock_aspect = true;

    // a bit awkward, we set these when we want to seek somewhere
    // better way? a function method maybe?
    bool setSeek = false;
    double seekProportion = 0;


    HWND window;
    int winWID;
    int winHEI;

    Timer app_timer;
};



#include "opengl.cpp"


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

    static timestamp FromPTS(i64 pts, RunningMovie movie, int streamIndex)
    {
        i64 base_num = movie.av_movie.vfc->streams[streamIndex]->time_base.num;
        i64 base_den = movie.av_movie.vfc->streams[streamIndex]->time_base.den;
        return {pts * base_num, base_den, movie.targetMsPerFrame};
    }
    static timestamp FromVideoPTS(RunningMovie movie)
    {
        return timestamp::FromPTS(movie.ptsOfLastVideo, movie, movie.av_movie.video.index);
    }
    static timestamp FromAudioPTS(RunningMovie movie)
    {
        return timestamp::FromPTS(movie.ptsOfLastAudio, movie, movie.av_movie.audio.index);
    }
};


// called "hard" because we flush the buffers and may have to grab a few frames to get the right one
void HardSeekToFrameForTimestamp(RunningMovie *movie, timestamp ts, double msAudioLatencyEstimate)
{
    // not entirely sure if this flush usage is right
    avcodec_flush_buffers(movie->av_movie.video.codecContext);
    avcodec_flush_buffers(movie->av_movie.audio.codecContext);

    i64 seekPos = ts.i64InUnits(AV_TIME_BASE);

    // AVSEEK_FLAG_BACKWARD = find first I-frame before our seek position
    av_seek_frame(movie->av_movie.vfc, -1, seekPos, AVSEEK_FLAG_BACKWARD);
    av_seek_frame(movie->av_movie.afc, -1, seekPos, AVSEEK_FLAG_BACKWARD);


    // // basically how close will we consider a "success"
    // double secondsInHalfFrame = (movie->targetMsPerFrame / 1000.0) / 2.0;

    // todo: special if at start of file?

    // todo: what if we seek right to an I-frame? i think that would still work,
    // we'd have to pull at least 1 frame to have something to display anyway

    double realTimeMs = ts.seconds() * 1000.0; //(double)seekPos / (double)AV_TIME_BASE;
    // double msSinceAudioStart = movie->audio_stopwatch.MsElapsed();
        // char msbuf[123];
        // sprintf(msbuf, "msSinceAudioStart: %f\n", msSinceAudioStart);
        // OutputDebugString(msbuf);

    // i64 framePTS = 0;

    // step through video frames for both contexts until we reach our desired timestamp

    // todo: replace this with getnextaudioframe?
    GetNextVideoFrame(
        movie->av_movie.afc,
        movie->av_movie.video.codecContext,
        movie->sws_context,
        movie->av_movie.video.index,
        movie->frame_output,
        realTimeMs,
        0,
        &movie->ptsOfLastAudio);
    // int bytes_queued_up = GetNextAudioFrame(
    //     loaded_video.av_movie.afc,
    //     loaded_video.av_movie.audio.codecContext,
    //     loaded_video.av_movie.audio.index,
    //     ffmpeg_to_sdl_buffer,
    //     wanted_bytes,
    //     loaded_video.audio_stopwatch.MsElapsed());

    movie->audio_stopwatch.SetMsElapsedFromSeconds(ts.seconds());

    GetNextVideoFrame(
        movie->av_movie.vfc,
        movie->av_movie.video.codecContext,
        movie->sws_context,
        movie->av_movie.video.index,
        movie->frame_output,
        movie->audio_stopwatch.MsElapsed(),// - msAudioLatencyEstimate,
        0,
        &movie->ptsOfLastVideo);


    i64 streamIndex = movie->av_movie.video.index;
    i64 base_num = movie->av_movie.vfc->streams[streamIndex]->time_base.num;
    i64 base_den = movie->av_movie.vfc->streams[streamIndex]->time_base.den;
    timestamp currentTS = {movie->ptsOfLastVideo * base_num, base_den, ts.framesPerSecond};

    double totalFrameCount = (movie->av_movie.vfc->duration / (double)AV_TIME_BASE) * (double)ts.framesPerSecond;
    double durationSeconds = movie->av_movie.vfc->duration / (double)AV_TIME_BASE;

        // char morebuf[123];
        // sprintf(morebuf, "dur (s): %f * fps: %f = %f frames\n", durationSeconds, ts.framesPerSecond, totalFrameCount);
        // OutputDebugString(morebuf);

        // char morebuf2[123];
        // sprintf(morebuf2, "dur: %lli / in base: %i\n", movie->av_movie.vfc->duration, AV_TIME_BASE);
        // OutputDebugString(morebuf2);

        // char ptsbuf[123];
        // sprintf(ptsbuf, "at: %lli / want: %lli of %lli\n",
        //         nearestI64(currentTS.frame())+1,
        //         nearestI64(ts.frame())+1,
        //         nearestI64(totalFrameCount));
        // OutputDebugString(ptsbuf);
}


// todo: what to do with this stuff??
// move into ghosterwindow?
POINT mDownPoint;
bool mDown;
bool ctrlDown;
bool mouseHasMovedSinceDownL = false;  // make into function comparing mdownpoint to current?


struct GhosterWindow
{

    AppState state;
    SoundBuffer ffmpeg_to_sdl_buffer;
    SDLStuff sdl_stuff;
    RunningMovie loaded_video;


    // now running this on a sep thread from our msg loop so it's independent of mouse events / captures
    void Update()
    {

        // if (dt < targetMsPerFrame)
        //     return;


        if (state.globalContextMenuOpen && state.menuCloseTimer.started && state.menuCloseTimer.MsSinceStart() > 150)
        {
            state.globalContextMenuOpen = false;
        }


        bool drawProgressBar;
        if (state.app_timer.MsSinceStart() - state.msOfLastMouseMove > PROGRESS_BAR_TIMEOUT
            && !state.clickingOnProgressBar)
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
        GetWindowRect(state.window, &winRect);
        if (!PtInRect(&winRect, mPos))
        {
            // OutputDebugString("mouse outside window\n");
            drawProgressBar = false;

            // if we mouse up while not on window all our mdown etc flags will be wrong
            // so we just force an "end of click" when we leave the window
            mDown = false;
            mouseHasMovedSinceDownL = false;
            state.clickingOnProgressBar = false;
        }





        if (state.setSeek)
        {
            SDL_ClearQueuedAudio(sdl_stuff.audio_device);

            state.setSeek = false;
            int seekPos = state.seekProportion * loaded_video.av_movie.vfc->duration;


            double videoFPS = 1000.0 / loaded_video.targetMsPerFrame;
                // char fpsbuf[123];
                // sprintf(fpsbuf, "fps: %f\n", videoFPS);
                // OutputDebugString(fpsbuf);

            timestamp ts = {nearestI64(state.seekProportion*loaded_video.av_movie.vfc->duration), AV_TIME_BASE, videoFPS};

            HardSeekToFrameForTimestamp(&loaded_video, ts, sdl_stuff.estimated_audio_latency_ms);

        }

        // best place for this?
        loaded_video.elapsed = loaded_video.audio_stopwatch.MsElapsed() / 1000.0;
        double percent = loaded_video.elapsed/loaded_video.duration;
            // char durbuf[123];
            // sprintf(durbuf, "elapsed: %.2f  /  %.2f  (%.f%%)\n", loaded_video.elapsed, loaded_video.duration, percent*100);
            // OutputDebugString(durbuf);


        if (loaded_video.vid_paused)
        {
            if (!loaded_video.vid_was_paused)
            {
                loaded_video.audio_stopwatch.Pause();
                SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)true);
            }
        }
        else
        {
            if (loaded_video.vid_was_paused || !loaded_video.audio_stopwatch.timer.started)
            {
                loaded_video.audio_stopwatch.Start();
                SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)false);
            }


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
                // todo: how to sync this right, pts dts?
                int bytes_queued_up = GetNextAudioFrame(
                    loaded_video.av_movie.afc,
                    loaded_video.av_movie.audio.codecContext,
                    loaded_video.av_movie.audio.index,
                    ffmpeg_to_sdl_buffer,
                    wanted_bytes,
                    loaded_video.audio_stopwatch.MsElapsed(),
                    &loaded_video.ptsOfLastAudio);


                if (bytes_queued_up > 0)
                {
                    if (SDL_QueueAudio(sdl_stuff.audio_device, ffmpeg_to_sdl_buffer.data, bytes_queued_up) < 0)
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




            // TIMINGS / SYNC



            int bytes_left = SDL_GetQueuedAudioSize(sdl_stuff.audio_device);
            double bytes_per_second = sdl_stuff.desired_bytes_in_sdl_queue / 1.0; // 1 second in queue at the moment
            double seconds_left_in_queue = (double)bytes_left / bytes_per_second;
                // char secquebuf[123];
                // sprintf(secquebuf, "seconds_left_in_queue: %.1f\n", seconds_left_in_queue);
                // OutputDebugString(secquebuf);

            timestamp ts_audio = timestamp::FromAudioPTS(loaded_video);
            // assuming we've filled the sdl buffer, we are 1 second ahead
            // but is that actually accurate? should we instead use SDL_GetQueuedAudioSize again to est??
            // and how consistently do we pull audio data? is it sometimes more than others?
            // update: i think we always put everything we get from decoding into sdl queue,
            // so sdl buffer should be a decent way to figure out how far our audio decoding is ahead of "now"
            double aud_seconds = ts_audio.seconds() - seconds_left_in_queue;
                // char audbuf[123];
                // sprintf(audbuf, "aud_seconds: %.1f  seconds_left_in_queue: %.1f\n",
                //         aud_seconds, seconds_left_in_queue);
                // OutputDebugString(audbuf);


            timestamp ts_video = timestamp::FromVideoPTS(loaded_video);
            double vid_seconds = ts_video.seconds();

            double estimatedVidPTS = vid_seconds + loaded_video.targetMsPerFrame/1000.0;


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
            if (aud_seconds > estimatedVidPTS - allowableAudioLag/1000.0) // time for a new frame if audio is this far behind
            {
                GetNextVideoFrame(
                    loaded_video.av_movie.vfc,
                    loaded_video.av_movie.video.codecContext,
                    loaded_video.sws_context,
                    loaded_video.av_movie.video.index,
                    loaded_video.frame_output,
                    aud_seconds * 1000.0,  // loaded_video.audio_stopwatch.MsElapsed(), // - sdl_stuff.estimated_audio_latency_ms,
                    allowableAudioLead,
                    &loaded_video.ptsOfLastVideo);

            }
            else
            {
                OutputDebugString("repeating a frame\n");
            }


            ts_video = timestamp::FromVideoPTS(loaded_video);
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
        loaded_video.vid_was_paused = loaded_video.vid_paused;


        RenderToScreenGL((void*)loaded_video.vid_buffer,
                        loaded_video.vidWID,
                        loaded_video.vidHEI,
                        state.winWID,
                        state.winHEI,
                        state.window,
                        percent, drawProgressBar);

        // DisplayAudioBuffer((u32*)vid_buffer, vidWID, vidHEI,
        //            (float*)sound_buffer, bytes_in_buffer);
        // static int increm = 0;
        // RenderWeird(secondary_buf, vidWID, vidHEI, increm++);
        // RenderToScreenGL((void*)vid_buffer, vidWID, vidHEI, winWID, winHEI, window, 0, false);
        // RenderToScreenGDI((void*)secondary_buf, vidWID, vidHEI, window);




        // HIT FPS

        // something seems off with this... ? i guess it's, it's basically ms since END of last frame
        double dt = state.app_timer.MsSinceLastFrame();

        // todo: we actually don't want to hit a certain fps like a game,
        // but accurately track our continuous audio timer
        // (eg if we're late one frame, go early the next?)

        if (dt < loaded_video.targetMsPerFrame)
        {
            double msToSleep = loaded_video.targetMsPerFrame - dt;
            Sleep(msToSleep);
            while (dt < loaded_video.targetMsPerFrame)  // is this weird?
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
                    loaded_video.targetMsPerFrame, dt);
            OutputDebugString(msg);
        }
        state.app_timer.EndFrame();  // make sure to call for MsSinceLastFrame() to work.. feels weird

    }


};


static GhosterWindow global_ghoster;







bool FindAudioAndVideoUrls(char *path, char **video, char **audio)
{
    // to get the output from running youtube-dl.exe,
    // we need to make a pipe to capture the stdout

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

    char args[MAX_PATH]; //todo: tempy
    sprintf(args, "youtube-dl.exe -g %s", path);

    if (!CreateProcess(
        "youtube-dl.exe",
        args,  // todo: UNSAFE
        0, 0, TRUE,
        CREATE_NEW_CONSOLE,
        0, 0,
        &si, &pi))
    {
        //OutputDebugString("Error creating youtube-dl process.");
        MsgBox("Error creating youtube-dl process.");
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    TerminateProcess(pi.hProcess, 0); // kill youtube-dl if still running

    // close write end before reading from read end
    if (!CloseHandle(outWrite))
    {
        OutputDebugString("Error with CloseHandle()");
        return false;
    }


    // get the string out of the pipe...
    // char *result = path; // huh??
    int bigEnoughToHoldMessyUrlsFromYoutubeDL = 1024 * 30;
    char *result = (char*)malloc(bigEnoughToHoldMessyUrlsFromYoutubeDL);

    DWORD bytesRead;
    if (!ReadFile(outRead, result, 1024*8, &bytesRead, NULL))
    {
        // too big?
        MsgBox("Error reading pipe, not enough buffer space?");
        return false;
    }

    if (bytesRead == 0)
    {
        // no output?
        MsgBox("No data from reading pipe.");
        return false;
    }

    // seem to get two urls, first video, second sound
    *video = result;
    *audio = result;
    for (char *p = result; p++; *p)
    {
        if (*p == '\n')
        {
            *p = 0;
            *audio = p+1;
            break;
        }
    }
    for (char *p = *audio; p++; *p)
    {
        if (*p == '\n')
        {
            *p = 0;
            break;
        }
    }


    OutputDebugString(*video); OutputDebugString("\n");
    OutputDebugString(*audio); OutputDebugString("\n");

    // path = video;


    CloseHandle(outRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}


static char global_file_to_load[1024];
static bool global_load_new_file = false;

static void GlobalLoadMovie(char *path)
{
    strcpy_s(global_file_to_load, 1024, path);
    global_load_new_file = true;
}

// todo: peruse this for memory leaks. also: better name?
bool CreateNewMovieFromPath(char *path, RunningMovie *newMovie)
{

    char loadingMsg[1234];
    sprintf(loadingMsg, "\nLoading %s\n", path);
    OutputDebugString(loadingMsg);

    if (StringBeginsWith(path, "http"))
    {
        char *video_url;
        char *audio_url;
        if(FindAudioAndVideoUrls(path, &video_url, &audio_url))
        {
            newMovie->av_movie = OpenMovieAV(video_url, audio_url);
        }
        else
        {
            MsgBox("Error loading file.");
            return false;
        }
    }
    else if (path[1] == ':')
    {
        newMovie->av_movie = OpenMovieAV(path, path);
    }
    else
    {
        MsgBox("not full filepath or url\n");
        return false;
    }


    MovieAV *loaded_video = &newMovie->av_movie;

    // set window size on video source resolution
    newMovie->vidWID = loaded_video->video.codecContext->width;
    newMovie->vidHEI = loaded_video->video.codecContext->height;
    global_ghoster.state.winWID = newMovie->vidWID;
    global_ghoster.state.winHEI = newMovie->vidHEI;
    RECT winRect;
    GetWindowRect(global_ghoster.state.window, &winRect);
    //keep top left of window in same pos for now, change to keep center in same position?
    MoveWindow(global_ghoster.state.window, winRect.left, winRect.top, global_ghoster.state.winWID, global_ghoster.state.winHEI, true);  // ever non-zero opening position? launch option?

    newMovie->vid_aspect = (double)global_ghoster.state.winWID / (double)global_ghoster.state.winHEI;


    // MAKE NOTE OF VIDEO LENGTH

    // todo: add handling for this
    assert(loaded_video->vfc->start_time==0);
        // char rewq[123];
        // sprintf(rewq, "start: %lli\n", start_time);
        // OutputDebugString(rewq);


    OutputDebugString("\nvideo format ctx:\n");
    logFormatContextDuration(loaded_video->vfc);
    OutputDebugString("\naudio format ctx:\n");
    logFormatContextDuration(loaded_video->afc);

    newMovie->duration = (double)loaded_video->vfc->duration / (double)AV_TIME_BASE;
    newMovie->elapsed = 0;

    newMovie->audio_stopwatch.ResetCompletely();


    // SET FPS BASED ON LOADED VIDEO

    double targetFPS = ((double)loaded_video->video.codecContext->time_base.den /
                        (double)loaded_video->video.codecContext->time_base.num) /
                        (double)loaded_video->video.codecContext->ticks_per_frame;

    char vidfps[123];
    sprintf(vidfps, "\nvideo frame rate: %i / %i  (%.2f FPS)\nticks_per_frame: %i\n",
        loaded_video->video.codecContext->time_base.num,
        loaded_video->video.codecContext->time_base.den,
        targetFPS,
        loaded_video->video.codecContext->ticks_per_frame
    );
    OutputDebugString(vidfps);


    newMovie->targetMsPerFrame = 1000.0 / targetFPS;






    // SDL, for sound atm

    SetupSDLSoundFor(loaded_video->audio.codecContext, &global_ghoster.sdl_stuff);

    SetupSoundBuffer(loaded_video->audio.codecContext, &global_ghoster.ffmpeg_to_sdl_buffer);




    // MORE FFMPEG

    // AVFrame *
    if (newMovie->frame_output) av_frame_free(&newMovie->frame_output);
    newMovie->frame_output = av_frame_alloc();  // just metadata

    if (!newMovie->frame_output)
        { MsgBox("ffmpeg: Couldn't alloc frame."); return false; }


    // actual mem for frame
    int numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, newMovie->vidWID, newMovie->vidHEI);
    if (newMovie->vid_buffer) av_free(newMovie->vid_buffer);
    newMovie->vid_buffer = (u8*)av_malloc(numBytes * sizeof(u8)); // is this right?

    // frame is now using buffer memory
    avpicture_fill((AVPicture *)newMovie->frame_output, newMovie->vid_buffer, AV_PIX_FMT_RGB32,
        newMovie->vidWID,
        newMovie->vidHEI);

    // for converting frame from file to a standard color format buffer (size doesn't matter so much)
    if (newMovie->sws_context) sws_freeContext(newMovie->sws_context);
    newMovie->sws_context = {0};
    newMovie->sws_context = sws_getContext(
        loaded_video->video.codecContext->width,
        loaded_video->video.codecContext->height,
        loaded_video->video.codecContext->pix_fmt,
        newMovie->vidWID,
        newMovie->vidHEI,
        AV_PIX_FMT_RGB32, //(AVPixelFormat)frame_output->format,
        SWS_BILINEAR,
        0, 0, 0);


    // get first frame in case we are paused
    HardSeekToFrameForTimestamp(newMovie, {0,1,targetFPS}, global_ghoster.sdl_stuff.estimated_audio_latency_ms);


    return true;

}




DWORD WINAPI RunMainLoop( LPVOID lpParam )
{

    // OPENGL (note context is thread specific)
    InitOpenGL(global_ghoster.state.window);


    // LOAD FILE
    GlobalLoadMovie(TEST_FILES[0]);



    global_ghoster.state.app_timer.Start();
    global_ghoster.state.app_timer.EndFrame();  // seed our first frame dt

    while (global_ghoster.state.appRunning)
    {
        if (global_load_new_file)
        {
            CreateNewMovieFromPath(global_file_to_load, &global_ghoster.loaded_video);
            global_load_new_file = false;
        }

        global_ghoster.Update();
    }

    return 0;
}





void SetWindowToAspectRatio(double vid_aspect)
{
    RECT winRect;
    GetWindowRect(global_ghoster.state.window, &winRect);
    int w = winRect.right - winRect.left;
    int h = winRect.bottom - winRect.top;
    // which to adjust tho?
    int nw = (int)((double)h * vid_aspect);
    int nh = (int)((double)w / vid_aspect);
    // i guess always make smaller for now
    if (nw < w)
        MoveWindow(global_ghoster.state.window, winRect.left, winRect.top, nw, h, true);
    else
        MoveWindow(global_ghoster.state.window, winRect.left, winRect.top, w, nh, true);
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












#define ID_EXIT 1001
#define ID_PAUSE 1002
#define ID_ASPECT 1003
#define ID_PASTE 1004


void OpenRClickMenuAt(HWND hwnd, POINT point)
{
    UINT aspectChecked = global_ghoster.state.lock_aspect ? MF_CHECKED : MF_UNCHECKED;
    HMENU hPopupMenu = CreatePopupMenu();
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_EXIT, L"Exit");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | aspectChecked, ID_ASPECT, L"Lock Aspect Ratio");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_PASTE, L"Paste Clipboard URL");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_PAUSE, L"Pause/Play");
    SetForegroundWindow(hwnd);

    global_ghoster.state.globalContextMenuOpen = true;
    global_ghoster.state.menuCloseTimer.Stop();
    TrackPopupMenu(hPopupMenu, 0, point.x, point.y, 0, hwnd, NULL);
    global_ghoster.state.menuCloseTimer.Start(); // we only get past TrackPopupMenu once the menu is closed
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
    GlobalLoadMovie(clipboardContents);
    return true; // todo: do we need a result from loadmovie?
}



// todo: what to do with this assortment of functions?


bool clientPointIsOnProgressBar(int x, int y)
{
    return y >= global_ghoster.state.winHEI-(PROGRESS_BAR_H+PROGRESS_BAR_B) &&
           y <= global_ghoster.state.winHEI-PROGRESS_BAR_B;
}
bool screenPointIsOnProgressBar(HWND hwnd, int x, int y)
{
    POINT newPoint = {x, y};
    ScreenToClient(hwnd, &newPoint);
    return clientPointIsOnProgressBar(newPoint.x, newPoint.y);
}



void appTogglePause()
{
    global_ghoster.loaded_video.vid_paused = !global_ghoster.loaded_video.vid_paused;
}

void appSetProgressBar(int clientX, int clientY)
{
    if (clientPointIsOnProgressBar(clientX, clientY)) // check here or outside?
    {
        double prop = (double)clientX / (double)global_ghoster.state.winWID;

        global_ghoster.state.setSeek = true;
        global_ghoster.state.seekProportion = prop;
    }
}

void appDragWindow(HWND hwnd, int x, int y)
{
    WINDOWPLACEMENT winpos;
    winpos.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &winpos))
    {
        if (winpos.showCmd == SW_MAXIMIZE)
        {
            ShowWindow(hwnd, SW_RESTORE);

            int mouseX = x;
            int mouseY = y;
            int winX = mouseX - global_ghoster.state.winWID/2;
            int winY = mouseY - global_ghoster.state.winHEI/2;
            MoveWindow(hwnd, winX, winY, global_ghoster.state.winWID, global_ghoster.state.winHEI, true);
        }
    }

    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}



void onMouseMove(HWND hwnd, int clientX, int clientY)
{
    // this is for progress bar timeout.. rename/move?
    global_ghoster.state.msOfLastMouseMove = global_ghoster.state.app_timer.MsSinceStart();

    if (mDown)
    {
        // need to determine if click or drag here, not in buttonup
        // because mousemove will trigger (i think) at the first pixel of movement
        POINT mPos = { clientX, clientY };
        double dx = (double)mPos.x - (double)mDownPoint.x;
        double dy = (double)mPos.y - (double)mDownPoint.y;
        double distance = sqrt(dx*dx + dy*dy);
        double MOVEMENT_ALLOWED_IN_CLICK = 2.5;
        if (distance <= MOVEMENT_ALLOWED_IN_CLICK)
        {
            // we haven't moved enough to be considered a drag
            // or to eliminate a double click possibility
        }
        else
        {
            mouseHasMovedSinceDownL = true;

            if (clientPointIsOnProgressBar(mDownPoint.x, mDownPoint.y))
            {
                appSetProgressBar(clientX, clientY);
            }
            else
            {
                appDragWindow(hwnd, clientX, clientY);
            }
        }
    }
}

void onMouseUpL()
{
    // OutputDebugString("LUP\n");

    mDown = false;

    if (mouseHasMovedSinceDownL)
    {
        // end of a drag
    }
    else
    {
        if (!clientPointIsOnProgressBar(mDownPoint.x, mDownPoint.y))
            appTogglePause();
    }
    mouseHasMovedSinceDownL = false;
    global_ghoster.state.clickingOnProgressBar = false;
}

void onDoubleClickDownL()
{
    // OutputDebugString("LDOUBLECLICK\n");

    if (clientPointIsOnProgressBar(mDownPoint.x, mDownPoint.y))
    {
        // OutputDebugString("on bar dbl\n");
        global_ghoster.state.clickingOnProgressBar = true;
        return;
    }

    WINDOWPLACEMENT winpos;
    winpos.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(global_ghoster.state.window, &winpos))
    {
        if (winpos.showCmd == SW_MAXIMIZE)
        {
            ShowWindow(global_ghoster.state.window, SW_RESTORE);

            // make this an option... (we might want to keep it in the corner eg)
            // int mouseX = LOWORD(lParam); // todo: GET_X_PARAM
            // int mouseY = HIWORD(lParam);
            // int winX = mouseX - winWID/2;
            // int winY = mouseY - winHEI/2;
            // MoveWindow(hwnd, winX, winY, winWID, winHEI, true);
        }
        else
        {
            ShowWindow(global_ghoster.state.window, SW_MAXIMIZE);
        }
    }

}

void onMouseDownL(int clientX, int clientY)
{
    // OutputDebugString("LDOWN\n");

    // i think we can just ignore if context menu is open
    if (global_ghoster.state.globalContextMenuOpen)
        return;

    // mouse state / info about click...
    mDown = true;
    mouseHasMovedSinceDownL = false;
    mDownPoint = {clientX, clientY};

    if (clientPointIsOnProgressBar(clientX, clientY))
    {
        // OutputDebugString("on bar\n");
        global_ghoster.state.clickingOnProgressBar = true;
        appSetProgressBar(clientX, clientY);
    }
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_CLOSE: {
            global_ghoster.state.appRunning = false;
        } break;

        case WM_SIZE: {
            global_ghoster.state.winWID = LOWORD(lParam);
            global_ghoster.state.winHEI = HIWORD(lParam);
            return 0;
        }

        case WM_SIZING: {  // when dragging border
            if (global_ghoster.state.lock_aspect)
            {
                RECT rc = *(RECT*)lParam;
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;

                double vid_aspect = global_ghoster.loaded_video.vid_aspect;

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

            // little hack to allow us to use progress bar all the way out to the edge
            if (screenPointIsOnProgressBar(hwnd, pos.x, pos.y) && !bottom) { left = false; right = false; }

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


        case WM_LBUTTONDOWN: {
            onMouseDownL(LOWORD(lParam), HIWORD(lParam));
        } break;
        // case WM_NCLBUTTONDOWN: {
        // } break;
        case WM_LBUTTONDBLCLK: {
            onDoubleClickDownL();
        } break;

        case WM_MOUSEMOVE: {
            onMouseMove(hwnd, LOWORD(lParam), HIWORD(lParam));  // todo: GET_X_PARAM
        } break;

        case WM_LBUTTONUP:
        case WM_NCLBUTTONUP: {
            onMouseUpL();
        } break;

        case WM_RBUTTONDOWN: {    // rclicks in client area (HTCLIENT)
            POINT openPoint = { LOWORD(lParam), HIWORD(lParam) };
            ClientToScreen(hwnd, &openPoint);
            OpenRClickMenuAt(hwnd, openPoint);
        } break;
        case WM_NCRBUTTONDOWN: {  // non-client area, apparently lParam is treated diff?
            POINT openPoint = { LOWORD(lParam), HIWORD(lParam) };
            OpenRClickMenuAt(hwnd, openPoint);
        } break;


        case WM_KEYDOWN: {
            if (wParam == 0x56) // V
            {
                if (ctrlDown)
                {
                    PasteClipboard();
                }
            }
            if (wParam == 0x11) // ctrl
            {
                ctrlDown = true;
            }
        } break;

        case WM_KEYUP: {
            if (wParam == 0x11) // ctrl
            {
                ctrlDown = false;
            }
            if (wParam >= 0x30 && wParam <= 0x39) // 0-9
            {
                GlobalLoadMovie(TEST_FILES[wParam - 0x30]);
                // debugLoadTestVideo(wParam - 0x30);
            }
        } break;

        case WM_COMMAND: {
            switch (wParam)
            {
                case ID_EXIT:
                    global_ghoster.state.appRunning = false;
                    break;
                case ID_PAUSE:
                    global_ghoster.loaded_video.vid_paused = !global_ghoster.loaded_video.vid_paused;
                    break;
                case ID_ASPECT:
                    SetWindowToAspectRatio(global_ghoster.loaded_video.vid_aspect);
                    global_ghoster.state.lock_aspect = !global_ghoster.state.lock_aspect;
                    break;
                case ID_PASTE:
                    PasteClipboard();
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
                GlobalLoadMovie(filePath);
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

    global_ghoster.state.winWID = 960;
    global_ghoster.state.winHEI = 720;
    RECT neededRect = {};
    neededRect.right = global_ghoster.state.winWID;
    neededRect.bottom = global_ghoster.state.winHEI;
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
    global_ghoster.state.window = CreateWindowEx(
        exStyles,
        wc.lpszClassName, "vid player",
        WS_POPUP | WS_VISIBLE,  // ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX
        CW_USEDEFAULT, CW_USEDEFAULT,
        neededRect.right - neededRect.left, neededRect.bottom - neededRect.top,
        0, 0, hInstance, 0);

    if (!global_ghoster.state.window)
    {
        MsgBox("Couldn't open window.");
    }

    // set window transparent (if styles above are right)
    if (SEE_THROUGH || CLICK_THROUGH)
        SetLayeredWindowAttributes(global_ghoster.state.window, 0, 122, LWA_ALPHA);


    // ENABLE DRAG DROP
    DragAcceptFiles(global_ghoster.state.window, true);


    // MAIN APP LOOP
    CreateThread(0, 0, RunMainLoop, 0, 0, 0);


    // MSG LOOP
    while (global_ghoster.state.appRunning)
    {
        MSG Message;
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            // char ttt[123];
            // sprintf(ttt, "msg: %i\n", Message.message);
            // OutputDebugString(ttt);

            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }

    return 0;
}