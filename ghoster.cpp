#include <windows.h>
#include <stdio.h>
#include <stdint.h> // types
#include <assert.h>
#include <math.h>


// for drag drop, sys tray icon
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")



#include "resource.h"



#define uint unsigned int

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t



UINT singleClickTimerID;

static HBITMAP global_bitmap_w;
static HBITMAP global_bitmap_b;

static HBITMAP global_bitmap_c1;
static HBITMAP global_bitmap_c2;
static HBITMAP global_bitmap_c3;
static HBITMAP global_bitmap_c4;

static HBITMAP global_bitmap_p1;
static HBITMAP global_bitmap_p2;
static HBITMAP global_bitmap_p3;
static HBITMAP global_bitmap_p4;

static HBITMAP global_bitmap_r1;
static HBITMAP global_bitmap_r2;
static HBITMAP global_bitmap_r3;
static HBITMAP global_bitmap_r4;

static HBITMAP global_bitmap_y1;
static HBITMAP global_bitmap_y2;
static HBITMAP global_bitmap_y3;
static HBITMAP global_bitmap_y4;


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

// disallow opacity greater than this when in ghost ode
const double GHOST_MODE_MAX_OPACITY = 0.95;

// how long to wait before toggling pause when single click (which could be start of double click)
// higher makes double click feel better (no audio stutter on fullscreen for slow double clicks)
// lower makes single click feel better (less delay when clicking to pause/unpause)
const double MS_PAUSE_DELAY_FOR_DOUBLECLICK = 150;  // slowest double click is 500ms by default

// snap to screen edge if this close
const int SNAP_IF_PIXELS_THIS_CLOSE = 25;



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
    Stopwatch audio_stopwatch; // todo: audit this: is audio a misnomer now? what is it actually used for?
    double aspect_ratio; // feels like it'd be better to store this as a rational

    i64 ptsOfLastVideo;
    i64 ptsOfLastAudio;

    // lines between runnningmovie and appstate are blurring a bit to me right now
    bool is_paused = false;
    bool was_paused = false;

    double targetMsPerFrame;

    u8 *vid_buffer;
    int vidWID;
    int vidHEI;
};


// todo: split into appstate and moviestate? rename runningmovie to moviestate? rename state to app_state? or win_state?

struct AppState {
    bool appRunning = true;


    Timer menuCloseTimer;
    bool globalContextMenuOpen;

    bool lock_aspect = true;
    bool repeat = true;
    bool transparent = false;
    bool clickThrough = false;
    bool topMost = true;
    bool enableSnapping = true;

    double opacity = 1.0;
    double last_opacity;
    bool had_to_cache_opacity = false;

    bool fullscreen = false;
    WINDOWPLACEMENT last_win_pos;

    bool savestate_is_saved = false;
    bool toggledPauseOnLastSingleClick = false;
    bool next_mup_was_double_click; // a message of sorts passed from double click (a mdown event) to mouse up


    // mouse state
    POINT mDownPoint;

    bool mDown;
    bool ctrlDown;
    bool clickingOnProgressBar = false;

    bool mouseHasMovedSinceDownL = false;  // make into function comparing mdownpoint to current?
    double msOfLastMouseMove = -1000;


    HWND window;
    int winWID;
    int winHEI;

    Timer app_timer;


    // a bit awkward, we set these when we want to seek somewhere
    // better way? a function method maybe?
    // maybe a sort of command interface struct?
    bool setSeek = false;
    double seekProportion = 0;

    bool messageLoadNewMovie = false;
    MovieAV newMovieToRun;

    bool bufferingOrLoading = false;



};



#include "opengl.cpp"


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

    static timestamp FromPTS(i64 pts, AVFormatContext *fc, int streamIndex, RunningMovie movie)
    {
        i64 base_num = fc->streams[streamIndex]->time_base.num;
        i64 base_den = fc->streams[streamIndex]->time_base.den;
        return {pts * base_num, base_den, movie.targetMsPerFrame};
    }
    static timestamp FromVideoPTS(RunningMovie movie)
    {
        return timestamp::FromPTS(movie.ptsOfLastVideo, movie.av_movie.vfc, movie.av_movie.video.index, movie);
    }
    static timestamp FromAudioPTS(RunningMovie movie)
    {
        return timestamp::FromPTS(movie.ptsOfLastAudio, movie.av_movie.afc, movie.av_movie.audio.index, movie);
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


    // todo: special if at start of file?

    // todo: what if we seek right to an I-frame? i think that would still work,
    // we'd have to pull at least 1 frame to have something to display anyway

    double realTimeMs = ts.seconds() * 1000.0; //(double)seekPos / (double)AV_TIME_BASE;
    // double msSinceAudioStart = movie->audio_stopwatch.MsElapsed();
        // char msbuf[123];
        // sprintf(msbuf, "msSinceAudioStart: %f\n", msSinceAudioStart);
        // OutputDebugString(msbuf);


    movie->audio_stopwatch.SetMsElapsedFromSeconds(ts.seconds());


    // step through frames for both contexts until we reach our desired timestamp

    GetNextVideoFrame(
        movie->av_movie.vfc,
        movie->av_movie.video.codecContext,
        movie->sws_context,
        movie->av_movie.video.index,
        movie->frame_output,
        movie->audio_stopwatch.MsElapsed(),// - msAudioLatencyEstimate,
        0,
        &movie->ptsOfLastVideo);


    // kinda awkward
    SoundBuffer dummyBuffyJunkData;
    dummyBuffyJunkData.data = (u8*)malloc(1024 * 10);
    dummyBuffyJunkData.size_in_bytes = 1024 * 10;
    int bytes_queued_up = GetNextAudioFrame(
        movie->av_movie.afc,
        movie->av_movie.audio.codecContext,
        movie->av_movie.audio.index,
        dummyBuffyJunkData,
        1024,
        realTimeMs,
        &movie->ptsOfLastAudio);
    free(dummyBuffyJunkData.data);


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


bool SetupForNewMovie(MovieAV movie, RunningMovie *outMovie);

void appPlay();
void appPause();

struct GhosterWindow
{

    AppState state;
    SoundBuffer ffmpeg_to_sdl_buffer;
    SDLStuff sdl_stuff;
    RunningMovie loaded_video;
    HICON icon; // randomly assigned on launch, or set in menu todo: should be in app state probably


    // now running this on a sep thread from our msg loop so it's independent of mouse events / captures
    void Update()
    {

        if (state.globalContextMenuOpen && state.menuCloseTimer.started && state.menuCloseTimer.MsSinceStart() > 150)
        {
            state.globalContextMenuOpen = false;
        }



        if (state.messageLoadNewMovie)
        {
            OutputDebugString("Ready to load new movie...\n");
            state.messageLoadNewMovie = false;
            if (!SetupForNewMovie(state.newMovieToRun, &loaded_video))
            {
                MsgBox("Error in setup for new movie.\n");
            }
            state.bufferingOrLoading = false;
            appPlay();
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

        POINT mPos;   GetCursorPos(&mPos);
        RECT winRect; GetWindowRect(state.window, &winRect);
        if (!PtInRect(&winRect, mPos))
        {
            // OutputDebugString("mouse outside window\n");
            drawProgressBar = false;

            // if we mouse up while not on window all our mdown etc flags will be wrong
            // so we just force an "end of click" when we leave the window
            state.mDown = false;
            state.mouseHasMovedSinceDownL = false;
            state.clickingOnProgressBar = false;
        }


        double percent;


        // feels a bit clunky.. maybe have is_playing flag somewhere?
        if (state.bufferingOrLoading)
        {
            appPause();
        }
        else
        {

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
            percent = loaded_video.elapsed/loaded_video.duration;
                // char durbuf[123];
                // sprintf(durbuf, "elapsed: %.2f  /  %.2f  (%.f%%)\n", loaded_video.elapsed, loaded_video.duration, percent*100);
                // OutputDebugString(durbuf);


            if (!loaded_video.is_paused)
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
                        loaded_video.av_movie.afc,
                        loaded_video.av_movie.audio.codecContext,
                        loaded_video.av_movie.audio.index,
                        ffmpeg_to_sdl_buffer,
                        wanted_bytes,
                        -1,
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
                           // sprintf(msg2, "bytes_queued_up: %i  seconds: %.1f\n", bytes_queued_up,
                           //         (double)bytes_queued_up / (double)sdl_stuff.desired_bytes_in_sdl_queue);
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
                    // sprintf(audbuf, "raw: %.1f  aud_seconds: %.1f  seconds_left_in_queue: %.1f\n",
                    //         ts_audio.seconds(), aud_seconds, seconds_left_in_queue);
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
        }



        RenderToScreenGL((void*)loaded_video.vid_buffer,
                        960,
                        720, //todo: extra bar bug qwer
                        // loaded_video.vidWID,
                        // loaded_video.vidHEI,
                        state.winWID,
                        state.winHEI,
                        state.window,
                        percent, drawProgressBar, state.bufferingOrLoading);

        // DisplayAudioBuffer((u32*)vid_buffer, vidWID, vidHEI,
        //            (float*)sound_buffer, bytes_in_buffer);
        // static int increm = 0;
        // RenderWeird(secondary_buf, vidWID, vidHEI, increm++);
        // RenderToScreenGL((void*)vid_buffer, vidWID, vidHEI, winWID, winHEI, window, 0, false);
        // RenderToScreenGDI((void*)secondary_buf, vidWID, vidHEI, window);




        // REPEAT

        if (state.repeat && percent > 1.0)  // note percent will keep ticking up even after vid is done
        {
            double targetFPS = 1000.0 / loaded_video.targetMsPerFrame;
            HardSeekToFrameForTimestamp(&loaded_video, {0,1,targetFPS}, sdl_stuff.estimated_audio_latency_ms);
        }



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


void SetWindowToNativeRes(HWND hwnd, RunningMovie movie)
{
    RECT winRect;
    GetWindowRect(hwnd, &winRect);

        char hwbuf[123];
        sprintf(hwbuf, "wid: %i  hei: %i\n",
            global_ghoster.loaded_video.av_movie.video.codecContext->width, global_ghoster.loaded_video.av_movie.video.codecContext->height);
        OutputDebugString(hwbuf);

        global_ghoster.state.winWID = movie.vidWID;
        global_ghoster.state.winHEI = movie.vidHEI;

    MoveWindow(hwnd, winRect.left, winRect.top, movie.vidWID, movie.vidHEI, true);
}

void SetWindowToAspectRatio(HWND hwnd, double aspect_ratio)
{
    RECT winRect;
    GetWindowRect(hwnd, &winRect);
    int w = winRect.right - winRect.left;
    int h = winRect.bottom - winRect.top;
    // which to adjust tho?
    int nw = (int)((double)h * aspect_ratio);
    int nh = (int)((double)w / aspect_ratio);
    // // i guess always make smaller for now
    // if (nw < w)
    //     MoveWindow(hwnd, winRect.left, winRect.top, nw, h, true);
    // else
    //     MoveWindow(hwnd, winRect.left, winRect.top, w, nh, true);
    // now always adjusting width
    MoveWindow(hwnd, winRect.left, winRect.top, nw, h, true);
}


// fill movieAV with data from movie at path
// calls youtube-dl if needed so could take a sec
bool SetupMovieAVFromPath(char *path, MovieAV *newMovie)
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
            *newMovie = OpenMovieAV(video_url, audio_url);
        }
        else
        {
            MsgBox("Error loading file.");
            return false;
        }
    }
    else if (path[1] == ':')
    {
        *newMovie = OpenMovieAV(path, path);
    }
    else
    {
        MsgBox("not full filepath or url\n");
        return false;
    }
    return true;
}


// todo: peruse this for memory leaks. also: better name!

bool SetupForNewMovie(MovieAV inMovie, RunningMovie *outMovie)
{

    outMovie->av_movie = DeepCopyMovieAV(inMovie);
    MovieAV *movie = &outMovie->av_movie;
    // MovieAV *loaded_video = &newMovie->av_movie;

    // set window size on video source resolution
    // global_ghoster.state.winWID = movie->video.codecContext->width;
    // global_ghoster.state.winHEI = movie->video.codecContext->height;
    outMovie->vidWID = movie->video.codecContext->width;
    outMovie->vidHEI = movie->video.codecContext->height;
        char hwbuf[123];
        sprintf(hwbuf, "wid: %i  hei: %i\n", movie->video.codecContext->width, movie->video.codecContext->height);
        OutputDebugString(hwbuf);
    // global_ghoster.state.winWID = outMovie->vidWID;
    // global_ghoster.state.winHEI = outMovie->vidHEI;

    // RECT winRect;
    // GetWindowRect(global_ghoster.state.window, &winRect);
    // //keep top left of window in same pos for now, change to keep center in same position?
    // MoveWindow(global_ghoster.state.window, winRect.left, winRect.top, global_ghoster.state.winWID, global_ghoster.state.winHEI, true);  // ever non-zero opening position? launch option?

    outMovie->aspect_ratio = (double)outMovie->vidWID / (double)outMovie->vidHEI;

    SetWindowToAspectRatio(global_ghoster.state.window, outMovie->aspect_ratio);


    // MAKE NOTE OF VIDEO LENGTH

    // todo: add handling for this
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

    outMovie->audio_stopwatch.ResetCompletely();


    // SET FPS BASED ON LOADED VIDEO

    double targetFPS = ((double)movie->video.codecContext->time_base.den /
                        (double)movie->video.codecContext->time_base.num) /
                        (double)movie->video.codecContext->ticks_per_frame;

    char vidfps[123];
    sprintf(vidfps, "\nvideo frame rate: %i / %i  (%.2f FPS)\nticks_per_frame: %i\n",
        movie->video.codecContext->time_base.num,
        movie->video.codecContext->time_base.den,
        targetFPS,
        movie->video.codecContext->ticks_per_frame
    );
    OutputDebugString(vidfps);


    outMovie->targetMsPerFrame = 1000.0 / targetFPS;






    // SDL, for sound atm

    SetupSDLSoundFor(movie->audio.codecContext, &global_ghoster.sdl_stuff);

    SetupSoundBuffer(movie->audio.codecContext, &global_ghoster.ffmpeg_to_sdl_buffer);




    // MORE FFMPEG

    // AVFrame *
    if (outMovie->frame_output) av_frame_free(&outMovie->frame_output);
    outMovie->frame_output = av_frame_alloc();  // just metadata

    if (!outMovie->frame_output)
        { MsgBox("ffmpeg: Couldn't alloc frame."); return false; }


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


    char linbuf[123];
    sprintf(linbuf, "linesize: %i\n", *outMovie->frame_output->linesize);
    OutputDebugString(linbuf);



    // get first frame in case we are paused
    HardSeekToFrameForTimestamp(outMovie, {0,1,targetFPS}, global_ghoster.sdl_stuff.estimated_audio_latency_ms);


    return true;

}



DWORD WINAPI CreateMovieSourceFromPath( LPVOID lpParam )
{
    char *path = (char*)lpParam;

    MovieAV newMovie;
    if (!SetupMovieAVFromPath(path, &newMovie))
    {
        char errbuf[123];
        sprintf(errbuf, "Error creating movie source from path:\n%s\n", path);
        MsgBox(errbuf);
        return false;
    }
    global_ghoster.state.newMovieToRun = DeepCopyMovieAV(newMovie);
    global_ghoster.state.messageLoadNewMovie = true;

    return 0;
}


bool CreateNewMovieFromPath(char *path, RunningMovie *newMovie)
{
    // if (!SetupForNewMovie(newMovie->av_movie, newMovie)) return false;
    global_ghoster.state.bufferingOrLoading = true;
    appPause(); // stop playing movie as well, we'll auto start the next one

    CreateThread(0, 0, CreateMovieSourceFromPath, (void*)path, 0, 0);

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
            // global_ghoster.state.buffering = true;
            CreateNewMovieFromPath(global_file_to_load, &global_ghoster.loaded_video);
            global_load_new_file = false;
        }

        global_ghoster.Update();
    }

    return 0;
}







#define ID_EXIT 1001
#define ID_PAUSE 1002
#define ID_ASPECT 1003
#define ID_PASTE 1004
#define ID_RESET_RES 1005
#define ID_REPEAT 1006
#define ID_TRANSPARENCY 1007
#define ID_CLICKTHRU 1008
#define ID_RANDICON 1009
#define ID_TOPMOST 1010
#define ID_FULLSCREEN 1011
#define ID_SNAPPING 1012

#define ID_SET_R 2001
#define ID_SET_P 2002
#define ID_SET_C 2003
#define ID_SET_Y 2004


void OpenRClickMenuAt(HWND hwnd, POINT point)
{
    // TODO: create this once at start then just show it here?

    HMENU hSubMenu = CreatePopupMenu();
    InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, ID_SET_Y, L"Clyde");
    InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, ID_SET_C, L"Inky");
    InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, ID_SET_P, L"Pinky");
    InsertMenuW(hSubMenu, 0, MF_BYPOSITION | MF_STRING, ID_SET_R, L"Blinky");
    MENUITEMINFO info = {0};
    info.cbSize = sizeof(MENUITEMINFO);
    info.fMask = MIIM_BITMAP;
    info.hbmpItem = global_bitmap_y1; SetMenuItemInfo(hSubMenu, ID_SET_Y, 0, &info);
    info.hbmpItem = global_bitmap_c1; SetMenuItemInfo(hSubMenu, ID_SET_C, 0, &info);
    info.hbmpItem = global_bitmap_p1; SetMenuItemInfo(hSubMenu, ID_SET_P, 0, &info);
    info.hbmpItem = global_bitmap_r1; SetMenuItemInfo(hSubMenu, ID_SET_R, 0, &info);

    UINT aspectChecked = global_ghoster.state.lock_aspect ? MF_CHECKED : MF_UNCHECKED;
    UINT repeatChecked = global_ghoster.state.repeat ? MF_CHECKED : MF_UNCHECKED;
    UINT transparentChecked = global_ghoster.state.transparent ? MF_CHECKED : MF_UNCHECKED;
    UINT clickThroughChecked = global_ghoster.state.clickThrough ? MF_CHECKED : MF_UNCHECKED;
    UINT topMostChecked = global_ghoster.state.topMost ? MF_CHECKED : MF_UNCHECKED;
    UINT fullscreenChecked = global_ghoster.state.fullscreen ? MF_CHECKED : MF_UNCHECKED;
    UINT snappingChecked = global_ghoster.state.enableSnapping ? MF_CHECKED : MF_UNCHECKED;

    wchar_t *playPauseText = global_ghoster.loaded_video.is_paused ? L"Play" : L"Pause";

    HMENU hPopupMenu = CreatePopupMenu();
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_EXIT, L"Exit");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | transparentChecked, ID_TRANSPARENCY, L"Toggle Transparency");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | clickThroughChecked, ID_CLICKTHRU, L"Ghost Mode (Click-Through)");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | topMostChecked, ID_TOPMOST, L"Always On Top");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, L"Choose Icon");
    // InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_RANDICON, L"New Random Icon");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | snappingChecked, ID_SNAPPING, L"Snap To Edges");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | aspectChecked, ID_ASPECT, L"Lock Aspect Ratio");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_RESET_RES, L"Resize To Native Resolution");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | repeatChecked, ID_REPEAT, L"Repeat");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | fullscreenChecked, ID_FULLSCREEN, L"Fullscreen");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_PASTE, L"Paste Clipboard URL");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, ID_PAUSE, playPauseText);
    SetForegroundWindow(hwnd);

    global_ghoster.state.globalContextMenuOpen = true;
    global_ghoster.state.menuCloseTimer.Stop();
    TrackPopupMenu(hPopupMenu, 0, point.x, point.y, 0, hwnd, NULL);
    global_ghoster.state.menuCloseTimer.Start(); // we only get here (past TrackPopupMenu) once the menu is closed
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
// split into "ghoster" and "system"? and put ghoster ones in ghoster class? prep for splitting into two files?


#define ID_SYSTRAY 999
#define ID_SYSTRAY_MSG WM_USER + 1

static HICON global_icon;
static HICON global_icon_w;
static HICON global_icon_b;

static HICON global_icon_c1;
static HICON global_icon_c2;
static HICON global_icon_c3;
static HICON global_icon_c4;

static HICON global_icon_p1;
static HICON global_icon_p2;
static HICON global_icon_p3;
static HICON global_icon_p4;

static HICON global_icon_r1;
static HICON global_icon_r2;
static HICON global_icon_r3;
static HICON global_icon_r4;

static HICON global_icon_y1;
static HICON global_icon_y2;
static HICON global_icon_y3;
static HICON global_icon_y4;



// HICON MakeIconFromBitmapID(HINSTANCE hInstance, int id)
// {
//     HBITMAP hbm = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(id), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
//     ICONINFO info = {true, 0, 0, hbm, hbm };
//     return CreateIconIndirect(&info);
// }

HICON MakeIconFromBitmap(HINSTANCE hInstance, HBITMAP hbm)
{
    ICONINFO info = {true, 0, 0, hbm, hbm};
    return CreateIconIndirect(&info);
}

void MakeIcons(HINSTANCE hInstance)
{
    global_icon = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(ID_ICON),
        IMAGE_ICON,
        0, 0, LR_DEFAULTSIZE);

    global_bitmap_w  = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_W ), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_b  = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_B ), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_c4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_C4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_p4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_P4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_r4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_R4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y1 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y1), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y2 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y2), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y3 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y3), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    global_bitmap_y4 = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(ID_ICON_Y4), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);

    global_icon_w =  MakeIconFromBitmap(hInstance, global_bitmap_w );
    global_icon_b =  MakeIconFromBitmap(hInstance, global_bitmap_b );
    global_icon_c1 = MakeIconFromBitmap(hInstance, global_bitmap_c1);
    global_icon_c2 = MakeIconFromBitmap(hInstance, global_bitmap_c2);
    global_icon_c3 = MakeIconFromBitmap(hInstance, global_bitmap_c3);
    global_icon_c4 = MakeIconFromBitmap(hInstance, global_bitmap_c4);
    global_icon_p1 = MakeIconFromBitmap(hInstance, global_bitmap_p1);
    global_icon_p2 = MakeIconFromBitmap(hInstance, global_bitmap_p2);
    global_icon_p3 = MakeIconFromBitmap(hInstance, global_bitmap_p3);
    global_icon_p4 = MakeIconFromBitmap(hInstance, global_bitmap_p4);
    global_icon_r1 = MakeIconFromBitmap(hInstance, global_bitmap_r1);
    global_icon_r2 = MakeIconFromBitmap(hInstance, global_bitmap_r2);
    global_icon_r3 = MakeIconFromBitmap(hInstance, global_bitmap_r3);
    global_icon_r4 = MakeIconFromBitmap(hInstance, global_bitmap_r4);
    global_icon_y1 = MakeIconFromBitmap(hInstance, global_bitmap_y1);
    global_icon_y2 = MakeIconFromBitmap(hInstance, global_bitmap_y2);
    global_icon_y3 = MakeIconFromBitmap(hInstance, global_bitmap_y3);
    global_icon_y4 = MakeIconFromBitmap(hInstance, global_bitmap_y4);

}

HICON GetIconByInt(int i)
{
    if (i-- < 1) return global_icon_c1;
    if (i-- < 1) return global_icon_c2;
    if (i-- < 1) return global_icon_c3;
    if (i-- < 1) return global_icon_c4;

    if (i-- < 1) return global_icon_p1;
    if (i-- < 1) return global_icon_p2;
    if (i-- < 1) return global_icon_p3;
    if (i-- < 1) return global_icon_p4;

    if (i-- < 1) return global_icon_r1;
    if (i-- < 1) return global_icon_r2;
    if (i-- < 1) return global_icon_r3;
    if (i-- < 1) return global_icon_r4;

    if (i-- < 1) return global_icon_y1;
    if (i-- < 1) return global_icon_y2;
    if (i-- < 1) return global_icon_y3;
    if (i-- < 1) return global_icon_y4;

    return global_icon_b;
}

static bool global_already_seeded_rand = false;
int randomInt(int upToAndNotIncluding)
{
    if (!global_already_seeded_rand)
    {
        global_already_seeded_rand = true;
        srand(time(0));
    }
    return rand() % upToAndNotIncluding;
}

HICON RandomIcon()
{
    return GetIconByInt(randomInt(16));
}



NOTIFYICONDATA SysTrayDefaultInfo(HWND hwnd)
{
    NOTIFYICONDATA info =
    {
        sizeof(NOTIFYICONDATA),
        hwnd,
        ID_SYSTRAY,               //UINT  uID
        NIF_ICON | NIF_MESSAGE | NIF_TIP,
        ID_SYSTRAY_MSG,           //UINT  uCallbackMessage
        global_icon,              //HICON hIcon
        "replace with movie title",               //TCHAR szTip[64]
        0,                    //DWORD dwState
        0,                    //DWORD dwStateMask
        0,                    //TCHAR szInfo[256]
        0,                    //UINT uVersion
        0,                    //TCHAR szInfoTitle[64]
        0,                    //DWORD dwInfoFlags
        0,                    //GUID  guidItem
        0                     //HICON hBalloonIcon
    };
    return info;
}

void AddSysTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA info = SysTrayDefaultInfo(hwnd);
    Shell_NotifyIcon(NIM_ADD, &info);
}

void RemoveSysTrayIcon(HWND hwnd)
{
    NOTIFYICONDATA info = SysTrayDefaultInfo(hwnd);
    Shell_NotifyIcon(NIM_DELETE, &info);
}

void SetIcon(HWND hwnd, HICON icon)
{
    // system tray icon
    NOTIFYICONDATA info = SysTrayDefaultInfo(hwnd);
    info.hIcon = icon;
    Shell_NotifyIcon(NIM_MODIFY, &info);

    // window icon (taskbar)
    SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
}



void toggleFullscreen()
{
    WINDOWPLACEMENT winpos;
    winpos.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(global_ghoster.state.window, &winpos))
    {
        if (winpos.showCmd == SW_MAXIMIZE || global_ghoster.state.fullscreen)
        {
            // ShowWindow(global_ghoster.state.window, SW_RESTORE);

            // restore our old position todo: replace if we get SW_MAXIMIZE / SW_RESTORE working
            SetWindowPos(
                global_ghoster.state.window,
                0,
                global_ghoster.state.last_win_pos.rcNormalPosition.left,
                global_ghoster.state.last_win_pos.rcNormalPosition.top,
                global_ghoster.state.last_win_pos.rcNormalPosition.right -
                global_ghoster.state.last_win_pos.rcNormalPosition.left,
                global_ghoster.state.last_win_pos.rcNormalPosition.bottom -
                global_ghoster.state.last_win_pos.rcNormalPosition.top,
                0);
            global_ghoster.state.fullscreen = false;


            // make this an option... (we might want to keep it in the corner eg)
            // int mouseX = LOWORD(lParam); // todo: GET_X_PARAM
            // int mouseY = HIWORD(lParam);
            // int winX = mouseX - winWID/2;
            // int winY = mouseY - winHEI/2;
            // MoveWindow(hwnd, winX, winY, winWID, winHEI, true);
        }
        else
        {
            // todo: BUG: transparency is lost when we full screen
            // ShowWindow(global_ghoster.state.window, SW_MAXIMIZE); // or SW_SHOWMAXIMIZED?

            // for now just change our window size to the monitor
            // but leave 1 pixel along the bottom because this method causes the same bug as SW_MAXIMIZE
            global_ghoster.state.last_win_pos.length = sizeof(WINDOWPLACEMENT);
            if (GetWindowPlacement(global_ghoster.state.window, &global_ghoster.state.last_win_pos)); // cache last position
            //
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(MonitorFromWindow(global_ghoster.state.window, MONITOR_DEFAULTTOPRIMARY), &mi))
            {
                SetWindowPos(
                    global_ghoster.state.window,
                    HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top -1,   //
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED
                    );
                global_ghoster.state.fullscreen = true;
            }



        }
    }
}

void setTopMost(HWND hwnd, bool enable)
{
    global_ghoster.state.topMost = enable;
    if (enable)
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    else
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}
void setClickThrough(HWND hwnd, bool enable)
{
    global_ghoster.state.clickThrough = enable;
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable)
    {
        style = style | WS_EX_TRANSPARENT;
        SetIcon(hwnd, global_icon_w);
    }
    else
    {
        style = style & ~WS_EX_TRANSPARENT;
        SetIcon(hwnd, global_ghoster.icon);
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, style);
}
void setWindowOpacity(HWND hwnd, double opacity)
{
    if (global_ghoster.state.clickThrough && opacity > GHOST_MODE_MAX_OPACITY)
        opacity = GHOST_MODE_MAX_OPACITY;
    global_ghoster.state.opacity = opacity;
    SetLayeredWindowAttributes(global_ghoster.state.window, 0, 255.0*opacity, LWA_ALPHA);
}

void setGhostMode(HWND hwnd, bool enable)
{
    setClickThrough(hwnd, enable);
    if (enable)
    {
        if (global_ghoster.state.opacity > GHOST_MODE_MAX_OPACITY)
        {
            global_ghoster.state.last_opacity = global_ghoster.state.opacity;
            global_ghoster.state.had_to_cache_opacity = true;
            global_ghoster.state.opacity = GHOST_MODE_MAX_OPACITY;
            setWindowOpacity(hwnd, global_ghoster.state.opacity);
        }
        else
        {
            global_ghoster.state.had_to_cache_opacity = false;
        }
    }
    else
    {
        if (global_ghoster.state.had_to_cache_opacity)
        {
            global_ghoster.state.opacity = global_ghoster.state.last_opacity;
            setWindowOpacity(hwnd, global_ghoster.state.opacity);
        }
    }
}


// todo: if we have a "videostate" struct we could just copy/restore it for these functions? hmm

// note we're saving this every time we click down (since it could be the start of a double click)
// so don't make this too crazy
void saveVideoPositionForAfterDoubleClick()
{
    global_ghoster.state.savestate_is_saved = true;

    global_ghoster.loaded_video.elapsed = global_ghoster.loaded_video.audio_stopwatch.MsElapsed() / 1000.0;
    double percent = global_ghoster.loaded_video.elapsed / global_ghoster.loaded_video.duration;
    global_ghoster.state.seekProportion = percent; // todo: make new variable rather than co-opt this one?
}

void restoreVideoPositionAfterDoubleClick()
{
    if (global_ghoster.state.savestate_is_saved)
    {
        global_ghoster.state.savestate_is_saved = false;

        global_ghoster.state.setSeek = true;
    }
}



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


void appPlay()
{
    global_ghoster.loaded_video.audio_stopwatch.Start();
    SDL_PauseAudioDevice(global_ghoster.sdl_stuff.audio_device, (int)false);
    global_ghoster.loaded_video.is_paused = false;
}

void appPause()
{
    global_ghoster.loaded_video.audio_stopwatch.Pause();
    SDL_PauseAudioDevice(global_ghoster.sdl_stuff.audio_device, (int)true);
    global_ghoster.loaded_video.is_paused = true;
}

void appTogglePause()
{
    global_ghoster.loaded_video.is_paused = !global_ghoster.loaded_video.is_paused;

    if (global_ghoster.loaded_video.is_paused && !global_ghoster.loaded_video.was_paused)
    {
        appPause();
    }
    if (!global_ghoster.loaded_video.is_paused)
    {
        if (global_ghoster.loaded_video.was_paused || !global_ghoster.loaded_video.audio_stopwatch.timer.started)
        {
            appPlay();
        }
    }

    global_ghoster.loaded_video.was_paused = global_ghoster.loaded_video.is_paused;
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

bool EdgeIsClose(int a, int b)
{
    return abs(a-b) < SNAP_IF_PIXELS_THIS_CLOSE;
}

void SnapRectEdgesToRect(RECT in, RECT limit, RECT *out)
{
    *out = in;

    int width = out->right - out->left;
    int height =  out->bottom - out->top;

    if (EdgeIsClose(in.left  , limit.left  )) out->left = limit.left;
    if (EdgeIsClose(in.top   , limit.top   )) out->top  = limit.top;
    if (EdgeIsClose(in.right , limit.right )) out->left = limit.right - width;
    if (EdgeIsClose(in.bottom, limit.bottom)) out->top  = limit.bottom - height;

    out->right = out->left + width;
    out->bottom = out->top + height;
}

void SnapRectToMonitor(RECT in, RECT *out)
{
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(global_ghoster.state.window, MONITOR_DEFAULTTOPRIMARY), &mi))
    {
        // snap to whatever is closer
        int distToBottom = abs(in.bottom - mi.rcMonitor.bottom);
        int distToTaskbar = abs(in.bottom - mi.rcWork.bottom);
        if (distToBottom < distToTaskbar)
            SnapRectEdgesToRect(in, mi.rcMonitor, out);
        else
            SnapRectEdgesToRect(in, mi.rcWork, out);
    }
}

void appDragWindow(HWND hwnd, int x, int y)
{
    WINDOWPLACEMENT winpos;
    winpos.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &winpos))
    {
        if (winpos.showCmd == SW_MAXIMIZE || global_ghoster.state.fullscreen)
        {
            // ShowWindow(hwnd, SW_RESTORE);

            SetWindowPos(
                global_ghoster.state.window,
                0,
                global_ghoster.state.last_win_pos.rcNormalPosition.left,
                global_ghoster.state.last_win_pos.rcNormalPosition.top,
                global_ghoster.state.last_win_pos.rcNormalPosition.right -
                global_ghoster.state.last_win_pos.rcNormalPosition.left,
                global_ghoster.state.last_win_pos.rcNormalPosition.bottom -
                global_ghoster.state.last_win_pos.rcNormalPosition.top,
                0);
            global_ghoster.state.fullscreen = false;

            // move window to mouse..
            int mouseX = x;
            int mouseY = y;
            int winX = mouseX - global_ghoster.state.winWID/2;
            int winY = mouseY - global_ghoster.state.winHEI/2;
            MoveWindow(hwnd, winX, winY, global_ghoster.state.winWID, global_ghoster.state.winHEI, true);
        }
    }

    global_ghoster.state.mDown = false; // kind of out-of-place but mouseup() is not getting called after drags
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}



void onMouseMove(HWND hwnd, int clientX, int clientY)
{
    // this is for progress bar timeout.. rename/move?
    global_ghoster.state.msOfLastMouseMove = global_ghoster.state.app_timer.MsSinceStart();

    if (global_ghoster.state.mDown)
    {
        // need to determine if click or drag here, not in buttonup
        // because mousemove will trigger (i think) at the first pixel of movement
        POINT mPos = { clientX, clientY };
        double dx = (double)mPos.x - (double)global_ghoster.state.mDownPoint.x;
        double dy = (double)mPos.y - (double)global_ghoster.state.mDownPoint.y;
        double distance = sqrt(dx*dx + dy*dy);
        double MOVEMENT_ALLOWED_IN_CLICK = 2.5;
        if (distance <= MOVEMENT_ALLOWED_IN_CLICK)
        {
            // we haven't moved enough to be considered a drag
            // or to eliminate a double click possibility (edit: although system handles that now)
        }
        else
        {
            global_ghoster.state.mouseHasMovedSinceDownL = true;

            if (clientPointIsOnProgressBar(global_ghoster.state.mDownPoint.x, global_ghoster.state.mDownPoint.y))
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


VOID CALLBACK onSingleClickL(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    // OutputDebugString("DELAYED M UP\n");

    KillTimer(0, singleClickTimerID);

    {
        appTogglePause();

        // we have to track whether get here or not
        // so we know if we've toggled pause between our double click or not
        // (it could be either case now that we have a little delay in our pause)
        global_ghoster.state.toggledPauseOnLastSingleClick = true;
    }
}

void onMouseUpL()
{
    // OutputDebugString("LUP\n");

    global_ghoster.state.mDown = false;

    if (global_ghoster.state.mouseHasMovedSinceDownL)
    {
        // end of a drag
        // todo: i don't think we ever actually get here on the end of a drag
    }
    else
    {
        if (!clientPointIsOnProgressBar(global_ghoster.state.mDownPoint.x, global_ghoster.state.mDownPoint.y))
        {
            // since this could be the mouse up in between the two clicks of a double click,
            // wait a little bit before we actually pause (should work with 0 delay as well)

            // don't queue up another pause if this is the end of a double click
            // we'll be restoring our pause state in restoreVideoPositionAfterDoubleClick() below
            if (!global_ghoster.state.next_mup_was_double_click)
            {
                global_ghoster.state.toggledPauseOnLastSingleClick = false; // we haven't until the timer runs out
                singleClickTimerID = SetTimer(NULL, 0, MS_PAUSE_DELAY_FOR_DOUBLECLICK, &onSingleClickL);
            }
            else
            {
                // if we are ending the click and we already registered the first click as a pause,
                // toggle pause again to undo that
                if (global_ghoster.state.toggledPauseOnLastSingleClick)
                {
                    // OutputDebugString("undo that click\n");
                    appTogglePause();
                }
            }
        }
    }

    global_ghoster.state.mouseHasMovedSinceDownL = false;
    global_ghoster.state.clickingOnProgressBar = false;

    if (global_ghoster.state.next_mup_was_double_click)
    {
        global_ghoster.state.next_mup_was_double_click = false;

        // cancel any pending single click effects lingering from the first mup of this dclick
        KillTimer(0, singleClickTimerID);

        // only restore if we actually paused/unpaused, otherwise we can just keep everything rolling as is
        if (global_ghoster.state.toggledPauseOnLastSingleClick)
        {
            // OutputDebugString("restore our vid position\n");
            restoreVideoPositionAfterDoubleClick();
        }
        else
        {
            // OutputDebugString("that was a fast doubleclick! no stutter\n");
        }
    }
}

void onDoubleClickDownL()
{
    // OutputDebugString("LDOUBLECLICK\n");

    if (clientPointIsOnProgressBar(global_ghoster.state.mDownPoint.x, global_ghoster.state.mDownPoint.y))
    {
        // OutputDebugString("on bar dbl\n");
        global_ghoster.state.clickingOnProgressBar = true;
        return;
    }

    toggleFullscreen();

    // note we actually have to do this in mouse up because that's where the vid gets paused
    // restoreVideoPositionAfterDoubleClick();

    // instead make a note to restore in mouseUp
    global_ghoster.state.next_mup_was_double_click = true;

}

void onMouseDownL(int clientX, int clientY)
{
    // OutputDebugString("LDOWN\n");

    // i think we can just ignore if context menu is open
    if (global_ghoster.state.globalContextMenuOpen)
        return;

    // mouse state / info about click...
    global_ghoster.state.mDown = true;
    global_ghoster.state.mouseHasMovedSinceDownL = false;
    global_ghoster.state.mDownPoint = {clientX, clientY};

    if (clientPointIsOnProgressBar(clientX, clientY))
    {
        // OutputDebugString("on bar\n");
        global_ghoster.state.clickingOnProgressBar = true;
        appSetProgressBar(clientX, clientY);
    }
    else
    {
        // note this works because onMouseDownL doesn't trigger on the second click of a double click
        saveVideoPositionForAfterDoubleClick();
    }
}


static int sys_moving_anchor_x;
static int sys_moving_anchor_y;


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        // is this needed?
        // case WM_DESTROY: {
        //     RemoveSysTrayIcon(hwnd);
        // } break;

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

                double aspect_ratio = global_ghoster.loaded_video.aspect_ratio;

                switch (wParam)
                {
                    case WMSZ_LEFT:
                    case WMSZ_RIGHT:
                        rc.bottom = rc.top + (int)((double)w / aspect_ratio);
                        break;

                    case WMSZ_TOP:
                    case WMSZ_BOTTOM:
                        rc.right = rc.left + (int)((double)h * aspect_ratio);
                        break;

                    case WMSZ_LEFT + WMSZ_TOP:
                    case WMSZ_LEFT + WMSZ_BOTTOM:
                        rc.left = rc.right - (int)((double)h * aspect_ratio);
                        break;

                    case WMSZ_RIGHT + WMSZ_TOP:
                        rc.top = rc.bottom - (int)((double)w / aspect_ratio);
                        break;

                    case WMSZ_RIGHT + WMSZ_BOTTOM:
                        rc.bottom = rc.top + (int)((double)w / aspect_ratio);
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


        case WM_ENTERSIZEMOVE: {
            POINT mPos;   GetCursorPos(&mPos);
            RECT winRect; GetWindowRect(hwnd, &winRect);
            sys_moving_anchor_x = mPos.x - winRect.left;
            sys_moving_anchor_y = mPos.y - winRect.top;
        } break;
        case WM_MOVING: {
            if (global_ghoster.state.enableSnapping)
            {
                POINT mPos;   GetCursorPos(&mPos);

                int width = ((RECT*)lParam)->right - ((RECT*)lParam)->left;
                int height = ((RECT*)lParam)->bottom - ((RECT*)lParam)->top;

                RECT positionIfNoSnap;
                positionIfNoSnap.left = mPos.x - sys_moving_anchor_x;
                positionIfNoSnap.top = mPos.y - sys_moving_anchor_y;
                positionIfNoSnap.right = positionIfNoSnap.left + width;
                positionIfNoSnap.bottom = positionIfNoSnap.top + height;

                SnapRectToMonitor(positionIfNoSnap, (RECT*)lParam);
            }
        } break;


        case WM_KEYDOWN: {
            if (wParam == 0x56) // V
            {
                if (global_ghoster.state.ctrlDown)
                {
                    PasteClipboard();
                }
            }
            if (wParam == 0x11) // ctrl
            {
                global_ghoster.state.ctrlDown = true;
            }
        } break;

        case WM_KEYUP: {
            if (wParam == 0x11) // ctrl
            {
                global_ghoster.state.ctrlDown = false;
            }
            if (wParam >= 0x30 && wParam <= 0x39) // 0-9
            {
                GlobalLoadMovie(TEST_FILES[wParam - 0x30]);
            }
        } break;

        case WM_COMMAND: {
            switch (wParam)
            {
                case ID_EXIT:
                    global_ghoster.state.appRunning = false;
                    break;
                case ID_PAUSE:
                    appTogglePause();
                    break;
                case ID_ASPECT:
                    SetWindowToAspectRatio(global_ghoster.state.window, global_ghoster.loaded_video.aspect_ratio);
                    global_ghoster.state.lock_aspect = !global_ghoster.state.lock_aspect;
                    break;
                case ID_PASTE:
                    PasteClipboard();
                    break;
                case ID_RESET_RES:
                    SetWindowToNativeRes(global_ghoster.state.window, global_ghoster.loaded_video);
                    break;
                case ID_REPEAT:
                    global_ghoster.state.repeat = !global_ghoster.state.repeat;
                    break;
                case ID_TRANSPARENCY:
                    global_ghoster.state.transparent = !global_ghoster.state.transparent;
                    if (global_ghoster.state.transparent) setWindowOpacity(hwnd, 0.5);
                    if (!global_ghoster.state.transparent) setWindowOpacity(hwnd, 1.0);
                    break;
                case ID_CLICKTHRU:
                    setGhostMode(hwnd, !global_ghoster.state.clickThrough);
                    break;
                case ID_RANDICON:
                    global_ghoster.icon = RandomIcon();
                    if (!global_ghoster.state.clickThrough) SetIcon(hwnd, global_ghoster.icon);
                    break;
                case ID_TOPMOST:
                    setTopMost(hwnd, !global_ghoster.state.topMost);
                    break;
                int color;
                case ID_SET_C: color = 0;
                    global_ghoster.icon = GetIconByInt(randomInt(4) + 4*color);
                    if (!global_ghoster.state.clickThrough) SetIcon(hwnd, global_ghoster.icon);
                    break;
                case ID_SET_P: color = 1;
                    global_ghoster.icon = GetIconByInt(randomInt(4) + 4*color);
                    if (!global_ghoster.state.clickThrough) SetIcon(hwnd, global_ghoster.icon);
                    break;
                case ID_SET_R: color = 2;
                    global_ghoster.icon = GetIconByInt(randomInt(4) + 4*color);
                    if (!global_ghoster.state.clickThrough) SetIcon(hwnd, global_ghoster.icon);
                    break;
                case ID_SET_Y: color = 3;
                    global_ghoster.icon = GetIconByInt(randomInt(4) + 4*color);
                    if (!global_ghoster.state.clickThrough) SetIcon(hwnd, global_ghoster.icon);
                    break;
                case ID_FULLSCREEN:
                    toggleFullscreen();
                    break;
                case ID_SNAPPING:
                    global_ghoster.state.enableSnapping = !global_ghoster.state.enableSnapping;
                    if (global_ghoster.state.enableSnapping)
                    {
                        RECT winRect; GetWindowRect(global_ghoster.state.window, &winRect);

                        SnapRectToMonitor(winRect, &winRect);

                        SetWindowPos(hwnd, 0,
                            winRect.left, winRect.top,
                            winRect.right  - winRect.left,
                            winRect.bottom - winRect.top,
                            0);
                    }
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


        case ID_SYSTRAY_MSG: {
            switch (lParam) {
                case WM_LBUTTONUP:
                    SetForegroundWindow(hwnd);
                    setGhostMode(hwnd, !global_ghoster.state.clickThrough);
                break;
                case WM_RBUTTONUP:
                    POINT mousePos;
                    GetCursorPos(&mousePos);
                    SetForegroundWindow(hwnd); // supposedly needed for menu to work as expected?
                    OpenRClickMenuAt(hwnd, mousePos);
                break;
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
    wc.lpszClassName = "ghoster window class";
    if (!RegisterClass(&wc)) { MsgBox("RegisterClass failed."); return 1; }

    global_ghoster.state.winWID = 960;
    global_ghoster.state.winHEI = 720;
    RECT neededRect = {};
    neededRect.right = global_ghoster.state.winWID;
    neededRect.bottom = global_ghoster.state.winHEI;

    // HWND
    global_ghoster.state.window = CreateWindowEx(
        WS_EX_LAYERED,
        wc.lpszClassName, "ghoster video player",
        WS_POPUP | WS_VISIBLE,  // ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX
        CW_USEDEFAULT, CW_USEDEFAULT,
        neededRect.right - neededRect.left, neededRect.bottom - neededRect.top,
        0, 0, hInstance, 0);

    if (!global_ghoster.state.window) { MsgBox("Couldn't open window."); }


    // setup some defaults....

    setWindowOpacity(global_ghoster.state.window, 1.0);
    setTopMost(global_ghoster.state.window, global_ghoster.state.topMost);

    MakeIcons(hInstance);

    global_ghoster.icon = RandomIcon();

    AddSysTrayIcon(global_ghoster.state.window); // sets as default icon
    SetIcon(global_ghoster.state.window, global_ghoster.icon);

    //


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
            // char msgbuf[123];
            // sprintf(msgbuf, "msg: %i\n", Message.message);
            // OutputDebugString(msgbuf);

            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }

    RemoveSysTrayIcon(global_ghoster.state.window);

    return 0;
}