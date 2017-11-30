#include <windows.h>
#include <stdio.h>
#include <stdint.h> // types
#include <assert.h>
#include <math.h>


// for drag drop, sys tray icon
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")


// for themes
#include <Uxtheme.h>
#include <Vsstyle.h> // parts and
#include <Vssym32.h> // states defns
#pragma comment(lib, "UxTheme.lib")


#include <Windowsx.h> // SelectFont, GET_X_LPARAM


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


const bool DEBUG_MCLICK_MSGS = false;

const bool FORCE_NON_ZERO_OPACITY = false;  // not sure if we want this or not


HWND global_workerw;
HWND global_wallpaper_window;
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
const char *WALLPAPER_CLASS_NAME = "ghoster wallpaper window class";

HWND global_popup_window;
HWND global_icon_menu_window;
static int selectedItem = -1;
static int subMenuSelectedItem = -1;
static bool global_is_submenu_shown = false;

HINSTANCE global_hInstance;

bool global_awkward_next_mup_was_closing_menu = false;


static char *global_exe_directory;


static HANDLE global_asyn_load_thread;


// todo: move into a system obj
static char *global_title_buffer;
const int TITLE_BUFFER_SIZE = 256;

const int URL_BUFFER_SIZE = 1024;  // todo: what to use for this?



UINT singleClickTimerID;

// static HBITMAP gobal_bitmap_checkmark;

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




char *TEST_FILES[] = {
    "D:/~phil/projects/ghoster/test-vids/tall.mp4",
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

// disallow opacity greater than this when in ghost mode
const double GHOST_MODE_MAX_OPACITY = 0.95;

// feels like i want this less often
const bool GHOST_MODE_SETS_TOPMOST = false;

// how long to wait before toggling pause when single click (which could be start of double click)
// higher makes double click feel better (no audio stutter on fullscreen for slow double clicks)
// lower makes single click feel better (less delay when clicking to pause/unpause)
const double MS_PAUSE_DELAY_FOR_DOUBLECLICK = 150;  // slowest double click is 500ms by default

// snap to screen edge if this close
const int SNAP_IF_PIXELS_THIS_CLOSE = 25;

// how long to leave messages on screen (including any fade time ATM)
double MS_TO_DISPLAY_MSG = 3000;



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

// a simple guess at least
bool StringIsUrl(const char *path)
{
    // feels pretty rudimentary / incomplete
    if (StringBeginsWith(path, "http")) return true;
    if (StringBeginsWith(path, "www")) return true;
    return false;
}

int nearestInt(double in)
{
    return floor(in + 0.5);
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

    char *cached_url;

    u8 *vid_buffer;
    int vidWID;
    int vidHEI;
};


// todo: split into appstate and moviestate? rename runningmovie to moviestate? rename state to app_state? or win_state?

struct AppState {
    bool appRunning = true;


    bool lock_aspect = true;
    bool repeat = true;

    // these could almost all be in app system state... hmm..
    bool clickThrough = false;
    bool topMost = false;
    bool enableSnapping = true;
    bool wallpaperMode = false;

    double opacity = 1.0;
    double last_opacity;
    bool had_to_cache_opacity = false;

    double volume = 1.0;


    Timer app_timer;

    bool bufferingOrLoading = true;

};


// basically anything related to the OS in some way
struct AppSystemState
{

    HWND window;
    int winWID;
    int winHEI;


    HICON icon; // randomly assigned on launch, or set in menu todo: should be in app state probably

    bool contextMenuOpen;


    bool fullscreen = false;  // could be in app state maybe
    WINDOWPLACEMENT last_win_pos;



    // mouse state
    POINT mDownPoint;

    bool mDown;
    bool ctrlDown;
    bool clickingOnProgressBar = false;

    bool mouseHasMovedSinceDownL = false;
    double msOfLastMouseMove = -1000;
};


struct AppColorBuffer
{
    u8 *memory = 0;
    int width;
    int height;

    void Allocate(int w, int h)
    {
        width = w;
        height = h;
        if (memory) free(memory);
        memory = (u8*)malloc(width * height * sizeof(u32));
        assert(memory); //todo for now
    }
};

struct AppTextBuffer
{
    char *memory = 0;
    int space;

    void Allocate(int len)
    {
        space = len;
        if (memory) free(memory);
        memory = (char*)malloc(space * sizeof(char));
        assert(memory); //todo for now
    }

    void Set(char *msg)
    {
        int newLen = strlen(msg);
        assert(newLen < space);
        memcpy(memory, msg, newLen);
    }
};

struct AppBuffers
{
    AppColorBuffer overlay;
    AppTextBuffer msg;
    AppTextBuffer rawMsg;
};

struct Color
{
    union
    {
        u32 hex;
        struct
        {
            u8 a;
            u8 r;
            u8 g;
            u8 b;
        };
    };
};

struct AppMessages
{
    // not sure why we're splitting these off from state
    // maybe just to keep things somewhat organized

    bool resizeWindowBuffers = false;

    bool next_mup_was_double_click; // a message of sorts passed from double click (a mdown event) to mouse up

    bool savestate_is_saved = false;
    bool toggledPauseOnLastSingleClick = false;

    bool setSeek = false;
    double seekProportion = 0;

    bool loadNewMovie = false;
    MovieAV newMovieToRun;

    int startAtSeconds = 0;

    bool displayNewMsg = false;
    double msLeftOfMsg = 0;
    Color msgBackgroundCol;

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
    // todo: measure the time this function takes and debug output it

    // not entirely sure if this flush usage is right
    if (movie->av_movie.video.codecContext) avcodec_flush_buffers(movie->av_movie.video.codecContext);
    if (movie->av_movie.audio.codecContext) avcodec_flush_buffers(movie->av_movie.audio.codecContext);

    i64 seekPos = ts.i64InUnits(AV_TIME_BASE);

    // AVSEEK_FLAG_BACKWARD = find first I-frame before our seek position
    if (movie->av_movie.video.codecContext) av_seek_frame(movie->av_movie.vfc, -1, seekPos, AVSEEK_FLAG_BACKWARD);
    if (movie->av_movie.audio.codecContext) av_seek_frame(movie->av_movie.afc, -1, seekPos, AVSEEK_FLAG_BACKWARD);


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

    int frames_skipped;
    GetNextVideoFrame(
        movie->av_movie.vfc,
        movie->av_movie.video.codecContext,
        movie->sws_context,
        movie->av_movie.video.index,
        movie->frame_output,
        movie->audio_stopwatch.MsElapsed(),// - msAudioLatencyEstimate,
        0,
        true,
        &movie->ptsOfLastVideo,
        &frames_skipped);


    // kinda awkward
    SoundBuffer dummyBufferJunkData;
    dummyBufferJunkData.data = (u8*)malloc(1024 * 10);
    dummyBufferJunkData.size_in_bytes = 1024 * 10;
    int bytes_queued_up = GetNextAudioFrame(
        movie->av_movie.afc,
        movie->av_movie.audio.codecContext,
        movie->av_movie.audio.index,
        dummyBufferJunkData,
        1024,
        realTimeMs,
        &movie->ptsOfLastAudio);
    free(dummyBufferJunkData.data);


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



HBITMAP CreateSolidColorBitmap(HDC hdc, int width, int height, COLORREF cref)
{
    HDC memDC  = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);

    HBRUSH brushFill = CreateSolidBrush(cref);
    HGDIOBJ oldBitmap = SelectObject(memDC, bitmap);
    HBRUSH  oldBrush = (HBRUSH)SelectObject(memDC, brushFill);
    SelectObject(memDC, GetStockObject(NULL_PEN));

    Rectangle(memDC, 0, 0, width+1, height+1);  // apparently +1s needed

    SelectObject(memDC, oldBrush); // do we really need this if we're destroying the memDC?
    SelectObject(memDC, oldBitmap);
    DeleteObject(brushFill);
    DeleteDC(memDC);

    return bitmap;
}

void PutTextOnBitmap(HDC hdc, HBITMAP bitmap, char *text, int x, int y, int fontSize, COLORREF cref)
{
    // create a device context for the skin
    HDC memDC = CreateCompatibleDC(hdc);

        // select the skin bitmap
        HGDIOBJ oldBitmap = SelectObject(memDC, bitmap);

            SetTextColor(memDC, cref);
            SetBkMode(memDC, TRANSPARENT);

            int nHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);

            // HFONT font = (HFONT)GetStockObject(ANSI_VAR_FONT);
            HFONT font = CreateFont(nHeight, 0,0,0,0,0,0,0,0,0,0,0,0, "Segoe UI");

                HFONT oldFont = (HFONT)SelectObject(memDC, font);

                    RECT rect = {x, y, x+1, y+1};
                    DrawText(memDC, text, -1, &rect, DT_CALCRECT);
                    int wid = rect.right-rect.left;
                    int hei = rect.bottom-rect.top;
                    rect = {rect.left-wid/2, rect.top-hei/2, rect.right-wid/2, rect.bottom-hei/2};
                    DrawText(memDC, text, -1, &rect, DT_CENTER|DT_TOP);

                    // view rect
                    // Rectangle(memDC, rect.left, rect.top, rect.right, rect.bottom);

                SelectObject(memDC, oldFont);
            DeleteObject(font);

        SelectObject(memDC, oldBitmap);

    DeleteDC(memDC);
}



void TransmogrifyText(char *src, char *dest)
{
    for (; *src; src++)
    {
        *dest = toupper(*src);
        dest++;
        *dest = ' ';
        dest++;
    }
    dest = '\0';
}


bool SetupForNewMovie(MovieAV movie, RunningMovie *outMovie);


void setWallpaperMode(HWND, bool);
void setFullscreen(bool);

struct GhosterWindow
{

    AppState state;
    AppSystemState system;

    SoundBuffer ffmpeg_to_sdl_buffer;
    SoundBuffer volume_adjusted_buffer;

    SDLStuff sdl_stuff;

    RunningMovie loaded_video;

    double msLastFrame; // todo: replace this with app timer, make timer usage more obvious

    // buffers mostly, anything on the heap
    AppBuffers buffer;

    // mostly flags, basic way to communicate between threads etc
    AppMessages message;


    void appPlay(bool userRequest = true)
    {
        if (userRequest)
        {
            QueueNewMsg("Play", 0x5aec5aff);
        }
        loaded_video.audio_stopwatch.Start();
        if (loaded_video.av_movie.IsAudioAvailable())
            SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)false);
        loaded_video.is_paused = false;
    }

    void appPause(bool userRequest = true)
    {
        if (userRequest)
        {
            QueueNewMsg("Pause", 0x7676eeff);
        }
        loaded_video.audio_stopwatch.Pause();
        SDL_PauseAudioDevice(sdl_stuff.audio_device, (int)true);
        loaded_video.is_paused = true;
    }

    void appTogglePause(bool userRequest = true)
    {
        loaded_video.is_paused = !loaded_video.is_paused;

        if (loaded_video.is_paused && !loaded_video.was_paused)
        {
            appPause(userRequest);
        }
        if (!loaded_video.is_paused)
        {
            if (loaded_video.was_paused || !loaded_video.audio_stopwatch.timer.started)
            {
                appPlay(userRequest);
            }
        }

        loaded_video.was_paused = loaded_video.is_paused;
    }



    void LoadNewMovie()
    {
        OutputDebugString("Ready to load new movie...\n");
        message.loadNewMovie = false;
        if (!SetupForNewMovie(message.newMovieToRun, &loaded_video))
        {
            MsgBox("Error in setup for new movie.\n");
        }

        if (message.startAtSeconds != 0)
        {
            double videoFPS = 1000.0 / loaded_video.targetMsPerFrame;
            timestamp ts = {message.startAtSeconds, 1, videoFPS};
            HardSeekToFrameForTimestamp(&loaded_video, ts, sdl_stuff.estimated_audio_latency_ms);
        }

        state.bufferingOrLoading = false;
        appPlay(false);

        // need to recreate wallpaper window basically
        if (state.wallpaperMode)
            setWallpaperMode(system.window, state.wallpaperMode);

        if (system.fullscreen)
            setFullscreen(system.fullscreen); // mostly for launching in fullscreen mode
    }


    void ResizeWindow(int wid, int hei)
    {
        system.winWID = wid;
        system.winHEI = hei;
        message.resizeWindowBuffers = true;
    }

    void QueueNewMsg(char *msg, u32 col = 0xff888888)
    {
        // todo: transmopgrify here, skip the second buffer
        buffer.rawMsg.Set(msg);
        // message.displayNewMsg = true;
        message.msLeftOfMsg = MS_TO_DISPLAY_MSG;
        message.msgBackgroundCol.hex = col;
    }

    void Init()
    {
        buffer.msg.Allocate(1024); // todo: some big length // todo: add length checks during usage
        buffer.rawMsg.Allocate(1024); // todo: some big length // todo: add length checks during usage


        // todo: move this to ghoster app
        // space we can re-use for title strings
        global_title_buffer = (char*)malloc(TITLE_BUFFER_SIZE); //remember this includes space for \0

        // maybe alloc ghoster app all at once? is this really the only mem we need for it?
        loaded_video.cached_url = (char*)malloc(URL_BUFFER_SIZE);
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


        // replace this with the-one-dt-to-rule-them-all, maybe from app_timer
        double temp_dt = state.app_timer.MsSinceStart() - msLastFrame;
        msLastFrame = state.app_timer.MsSinceStart();


        if (message.resizeWindowBuffers ||
            system.winWID != buffer.overlay.width ||
            system.winHEI != buffer.overlay.height
            )
        {
            message.resizeWindowBuffers = false;
            buffer.overlay.Allocate(system.winWID, system.winHEI);
        }


        if (message.loadNewMovie)
        {
            LoadNewMovie();
        }


        bool drawProgressBar;
        if (state.app_timer.MsSinceStart() - system.msOfLastMouseMove > PROGRESS_BAR_TIMEOUT
            && !system.clickingOnProgressBar)
        {
            drawProgressBar = false;
        }
        else
        {
            drawProgressBar = true;
        }

        POINT mPos;   GetCursorPos(&mPos);
        RECT winRect; GetWindowRect(system.window, &winRect);
        if (!PtInRect(&winRect, mPos))
        {
            // OutputDebugString("mouse outside window\n");
            drawProgressBar = false;

            // if we mouse up while not on window all our mdown etc flags will be wrong
            // so we just force an "end of click" when we leave the window
            system.mDown = false;
            system.mouseHasMovedSinceDownL = false;
            system.clickingOnProgressBar = false;
        }

        // awkward way to detect if mouse leaves the menu (and hide highlighting)
        GetWindowRect(global_icon_menu_window, &winRect);
        if (!PtInRect(&winRect, mPos)) {
            subMenuSelectedItem = -1;
            RedrawWindow(global_icon_menu_window, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
            global_is_submenu_shown = false;//
        }
        GetWindowRect(global_popup_window, &winRect);
        if (!PtInRect(&winRect, mPos)) {
            selectedItem = -1;
            RedrawWindow(global_popup_window, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
        }


        double percent;


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
                int seekPos = message.seekProportion * loaded_video.av_movie.vfc->duration;


                double videoFPS = 1000.0 / loaded_video.targetMsPerFrame;
                    // char fpsbuf[123];
                    // sprintf(fpsbuf, "fps: %f\n", videoFPS);
                    // OutputDebugString(fpsbuf);

                timestamp ts = {nearestI64(message.seekProportion*loaded_video.av_movie.vfc->duration), AV_TIME_BASE, videoFPS};

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

                timestamp ts_audio = timestamp::FromAudioPTS(loaded_video);

                // if no audio, use video pts (we should basically never skip or repeat in this case)
                if (!loaded_video.av_movie.IsAudioAvailable())
                {
                    ts_audio = timestamp::FromVideoPTS(loaded_video);
                }

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

                // todo: want to do any special handling here if no video?

                // time for a new frame if audio is this far behind
                if (aud_seconds > estimatedVidPTS - allowableAudioLag/1000.0
                    || !loaded_video.av_movie.IsAudioAvailable()
                )
                {
                    int frames_skipped = 0;
                    GetNextVideoFrame(
                        loaded_video.av_movie.vfc,
                        loaded_video.av_movie.video.codecContext,
                        loaded_video.sws_context,
                        loaded_video.av_movie.video.index,
                        loaded_video.frame_output,
                        aud_seconds * 1000.0,  // loaded_video.audio_stopwatch.MsElapsed(), // - sdl_stuff.estimated_audio_latency_ms,
                        allowableAudioLead,
                        loaded_video.av_movie.IsAudioAvailable(),
                        &loaded_video.ptsOfLastVideo,
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



        int wid = buffer.overlay.width;
        int hei = buffer.overlay.height;
        // int wid = system.winWID;
        // int hei = system.winHEI;
        // winRect;
        // GetWindowRect(system.window, &winRect);
        // int wid = winRect.right - winRect.left;
        // int hei = winRect.bottom - winRect.top;

        HDC hdc = GetDC(system.window);
            Color col = message.msgBackgroundCol;
            HBITMAP hBitmap = CreateSolidColorBitmap(hdc, wid, hei, RGB(col.r, col.g, col.b));

                if (message.msLeftOfMsg > 0)
                {
                    message.msLeftOfMsg -= temp_dt;
                    TransmogrifyText(buffer.rawMsg.memory, buffer.msg.memory);
                    PutTextOnBitmap(hdc, hBitmap, buffer.msg.memory, wid/2.0, hei/2.0, 36, RGB(255, 255, 255));
                }

                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);

                // Get the BITMAPINFO structure from the bitmap
                if(0 == GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmi, DIB_RGB_COLORS)) {
                    OutputDebugString("GetDIBits error1\n");
                }

                // create the bitmap buffer
                // BYTE* textMem = new BYTE[bmi.bmiHeader.biSizeImage];
                u8 *textMem = buffer.overlay.memory;

                // Better do this here - the original bitmap might have BI_BITFILEDS, which makes it
                // necessary to read the color table - you might not want this.
                bmi.bmiHeader.biCompression = BI_RGB;
                bmi.bmiHeader.biHeight *= -1;

                // get the actual bitmap buffer
                if(0 == GetDIBits(hdc, hBitmap, 0, -bmi.bmiHeader.biHeight, (LPVOID)textMem, &bmi, DIB_RGB_COLORS)) {
                    OutputDebugString("GetDIBits error2\n");
                }

            DeleteObject(hBitmap);
        ReleaseDC(system.window, hdc);



        // one idea
        // but we need to solve the stretching issue as well,
        // maybe a different solution will catch both
        for (int x = 0; x < wid; x++)
        {
            for (int y = 0; y < hei; y++)
            {
                u8 *b = textMem + ((wid*y)+x)*4 + 0;
                u8 *g = textMem + ((wid*y)+x)*4 + 1;
                u8 *r = textMem + ((wid*y)+x)*4 + 2;
                u8 *a = textMem + ((wid*y)+x)*4 + 3;

                // if we start with all white,
                // the amount off black we are should be our alpha right?
                // edit: unfortunately it doesn't work since we
                // get uneven color for subpixel accuracy or some other text rendering reason
                // *a = 255-*r;

                // ok so try alpha from most white value
                // edit: better but still have up some weird edges
                // *a = min(min(255-*r, 255-*g), 255-*b);

                *a = 255;
                // *a = 125;

                // now trying alpha from black
                // these are pretty much all terrible but min is the least bad
                // *a = min(min(*r, *g), *b);
                // *a = max(max(*r, *g), *b);
                // *a = (*r + *g + *b)/3.0;

                // u8 average = (*r + *g + *b)/3.0;
                // *r = average;
                // *g = average;
                // *b = average;
                // *a = average;
            }
        }




        // static double t = 0;
        // t += temp_dt;
        // double textAlpha = (sin(t*M_PI*2 / 3000) + 1.0)/2.0;
        double textAlpha = 0;
        double maxA = 0.65; // implicit min of 0
        // textAlpha = lerp(maxA, minA, message.msLeftOfMsg / MS_TO_DISPLAY_MSG);
        if (message.msLeftOfMsg > 0)
        {
            textAlpha = ((-cos(message.msLeftOfMsg*M_PI / MS_TO_DISPLAY_MSG) + 1) / 2) * maxA;
        }



        HWND destWin = system.window;
        if (state.wallpaperMode)
        {
            destWin = global_wallpaper_window;
        }
        RenderToScreenGL((void*)loaded_video.vid_buffer,
                        960,
                        720, //todo: extra bar bug qwer
                        // loaded_video.vidWID,
                        // loaded_video.vidHEI,
                        system.winWID,
                        system.winHEI,
                        // system.winWID,
                        // system.winHEI,
                        buffer.overlay.width,
                        buffer.overlay.height,
                        destWin,
                        temp_dt,
                        state.lock_aspect && system.fullscreen,  // temp: aspect + fullscreen = letterbox
                        loaded_video.aspect_ratio,
                        percent, drawProgressBar, state.bufferingOrLoading,
                        textMem, textAlpha
                        );
        // RenderToScreen_FF((void*)loaded_video.vid_buffer, 960, 720, destWin);
        // Render_GDI((void*)loaded_video.vid_buffer, 960, 720, destWin);

        // delete[] textMem;



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
    sprintf(youtube_dl_path, "%syoutube-dl.exe", global_exe_directory);

    char args[MAX_PATH]; //todo: tempy
    sprintf(args, "%syoutube-dl.exe %s %s", global_exe_directory, options, url);
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
        //OutputDebugString("Error creating youtube-dl process.");
        char errmsg[123];
        sprintf(errmsg, "Error creating youtube-dl process.\nCode: %i", GetLastError());
        MsgBox(errmsg);
        // MsgBox("Error creating youtube-dl process.");
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


    // // get the string out of the pipe...
    // // char *result = path; // huh??
    // int bigEnoughToHoldMessyUrlsFromYoutubeDL = 1024 * 30;
    // char *result = (char*)malloc(bigEnoughToHoldMessyUrlsFromYoutubeDL); // todo: leak

    DWORD bytesRead;
    if (!ReadFile(outRead, outString, 1024*8, &bytesRead, NULL))
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



// todo: move these to gg.messages?
static char global_file_to_load[1024];
static bool global_load_new_file = false;

static void GlobalLoadMovie(char *path)
{
    strcpy_s(global_file_to_load, 1024, path);
    global_load_new_file = true;
}


void SetWindowSize(HWND hwnd, int wid, int hei)
{
    global_ghoster.ResizeWindow(wid, hei);

    RECT winRect;
    GetWindowRect(hwnd, &winRect);
    MoveWindow(hwnd, winRect.left, winRect.top, wid, hei, true);
}

void SetWindowToNativeRes(HWND hwnd, RunningMovie movie)
{

    char hwbuf[123];
    sprintf(hwbuf, "wid: %i  hei: %i\n",
        global_ghoster.loaded_video.av_movie.video.codecContext->width, global_ghoster.loaded_video.av_movie.video.codecContext->height);
    OutputDebugString(hwbuf);

    SetWindowSize(hwnd, movie.vidWID, movie.vidHEI);
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
    // MoveWindow(hwnd, winRect.left, winRect.top, nw, h, true);
    SetWindowSize(hwnd, nw, h);
}


// todo: global variable instead?
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

void SetTitle(HWND hwnd, char *title)
{
    // system tray hover
    NOTIFYICONDATA info = SysTrayDefaultInfo(hwnd);
    assert(strlen(title) < 64);
    strcpy_s(info.szTip, 64, title); // todo: check length
    info.hIcon = global_ghoster.system.icon;
    Shell_NotifyIcon(NIM_MODIFY, &info);

    // window titlebar (taskbar)
    SetWindowText(hwnd, title);
}


// fill movieAV with data from movie at path
// calls youtube-dl if needed so could take a sec
bool SetupMovieAVFromPath(char *path, MovieAV *newMovie, char *outTitle)
{
    char loadingMsg[1234];
    sprintf(loadingMsg, "\nLoading %s\n", path);
    OutputDebugString(loadingMsg);

    // strcpy_s(outTitle, 64, "[no title]"); //todo: length

    if (StringIsUrl(path))
    {
        char *video_url = (char*)malloc(1024*10);  // big enough for some big url from youtube-dl
        char *audio_url = (char*)malloc(1024*10);  // todo: mem leak if we kill this thread before free()
        if(FindAudioAndVideoUrls(path, video_url, audio_url, outTitle))
        {
            if (!OpenMovieAV(video_url, audio_url, newMovie))
            {
                free(video_url);
                free(audio_url);

                return false;
            }
            free(video_url);
            free(audio_url); // all these frees are a bit messy, better way?
        }
        else
        {
            free(video_url);
            free(audio_url);

            MsgBox("Error loading file.");
            return false;
        }
    }
    else if (path[1] == ':')
    {
        // *newMovie = OpenMovieAV(path, path);
        if (!OpenMovieAV(path, path, newMovie))
            return false;

        char *fileNameOnly = path;
        while (*fileNameOnly)
            fileNameOnly++; // find end
        while (*fileNameOnly != '\\' && *fileNameOnly != '/')
            fileNameOnly--; // backup till we hit a directory
        fileNameOnly++; // drop the / tho
        strcpy_s(outTitle, 64, fileNameOnly); //todo: length
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

    SetWindowToAspectRatio(global_ghoster.system.window, outMovie->aspect_ratio);


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



DWORD WINAPI CreateMovieSourceFromPath( LPVOID lpParam )
{
    char *path = (char*)lpParam;

    MovieAV newMovie;
    char *title = global_title_buffer;
    strcpy_s(title, TITLE_BUFFER_SIZE, "[no title]");
    if (!SetupMovieAVFromPath(path, &newMovie, title))  // todo: move title into movieAV? rename to movieFile or something?
    {
        char errbuf[123];
        sprintf(errbuf, "Error creating movie source from path:\n%s\n", path);
        MsgBox(errbuf);
        return false;
    }

    // todo: better place for this? i guess it might be fine
    SetTitle(global_ghoster.system.window, title);

    global_ghoster.message.newMovieToRun = DeepCopyMovieAV(newMovie);
    global_ghoster.message.loadNewMovie = true;

    return 0;
}


// for timestamps in the form &t=X / &t=Xs / &t=XmYs / &t=XhYmZs
int SecondsFromStringTimestamp(char *timestamp)
{
    int secondsSoFar = 0;

    if (StringBeginsWith(timestamp, "&t=") || StringBeginsWith(timestamp, "#t="))
    {
        timestamp+=3;
    }

    char *p = timestamp;

    char *nextDigits = timestamp;
    int digitCount = 0;

    char *nextUnits;

    while (*p)
    {

        nextDigits = p;
        while (isdigit((int)*p))
        {
            p++;
        }

        int nextNum = atoi(nextDigits);

        // nextUnit = *p;
        // if (nextUnit == '\0') nextUnit = 's';

        int secondsPerUnit = 1;
        if (*p == 'm') secondsPerUnit = 60;
        if (*p == 'h') secondsPerUnit = 60*60;

        secondsSoFar += nextNum*secondsPerUnit;

        p++;

    }

    return secondsSoFar;
}

bool Test_SecondsFromStringTimestamp()
{
    if (SecondsFromStringTimestamp("#t=21s1m") != 21+60) return false;
    if (SecondsFromStringTimestamp("&t=216") != 216) return false;
    if (SecondsFromStringTimestamp("12") != 12) return false;
    if (SecondsFromStringTimestamp("12s") != 12) return false;
    if (SecondsFromStringTimestamp("854s") != 854) return false;
    if (SecondsFromStringTimestamp("2m14s") != 2*60+14) return false;
    if (SecondsFromStringTimestamp("3h65m0s") != 3*60*60+65*60+0) return false;
    return true;
}


bool CreateNewMovieFromPath(char *path, RunningMovie *newMovie)
{
    // if (!SetupForNewMovie(newMovie->av_movie, newMovie)) return false;
    global_ghoster.state.bufferingOrLoading = true;
    global_ghoster.appPause(false); // stop playing movie as well, we'll auto start the next one

    char *timestamp = strstr(path, "&t=");
    if (timestamp == 0) timestamp = strstr(path, "#t=");
    if (timestamp != 0) {
        int startSeconds = SecondsFromStringTimestamp(timestamp);
        global_ghoster.message.startAtSeconds = startSeconds;
            // char buf[123];
            // sprintf(buf, "\n\n\nstart seconds: %i\n\n\n", startSeconds);
            // OutputDebugString(buf);
    }

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

    global_asyn_load_thread = CreateThread(0, 0, CreateMovieSourceFromPath, (void*)path, 0, 0);


    // strip off timestamp before caching path
    if (timestamp != 0)
        timestamp[0] = '\0';

    // save url for later (is loaded_video the best place for cached_url?)
    // is this the best place to set cached_url?
    strcpy_s(global_ghoster.loaded_video.cached_url, URL_BUFFER_SIZE, path);


    return true;
}



DWORD WINAPI RunMainLoop( LPVOID lpParam )
{

    InitOpenGL(global_ghoster.system.window);


    // LOAD FILE
    if (!global_load_new_file)
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
    free(clipboardContents);
    return true; // todo: do we need a result from loadmovie?
}


bool CopyUrlToClipboard()
{
    char *url = global_ghoster.loaded_video.cached_url;

    char output[URL_BUFFER_SIZE]; // todo: stack alloc ok here?
    if (StringIsUrl(url)) {
        int secondsElapsed = global_ghoster.loaded_video.elapsed;
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



// todo: what to do with this assortment of functions?
// split into "ghoster" and "system"? and put ghoster ones in ghoster class? prep for splitting into two files?




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

    // // also this bitmap here
    // gobal_bitmap_checkmark = LoadBitmap((HINSTANCE) NULL, (LPTSTR) OBM_CHECK);


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

    // only needed if we're using the new wallpaper window icon in the taskbar
    // set wallpaper window same for now
    if (hwnd != global_wallpaper_window) SetIcon(global_wallpaper_window, icon);
}



void setFullscreen(bool enable)
{
    if (enable)
    {
        // todo: BUG: transparency is lost when we full screen
        // ShowWindow(global_ghoster.system.window, SW_MAXIMIZE); // or SW_SHOWMAXIMIZED?

        // for now just change our window size to the monitor
        // but leave 1 pixel along the bottom because this method causes the same bug as SW_MAXIMIZE
        global_ghoster.system.last_win_pos.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(global_ghoster.system.window, &global_ghoster.system.last_win_pos)); // cache last position
        //
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(MonitorFromWindow(global_ghoster.system.window, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            SetWindowPos(
                global_ghoster.system.window,
                HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top -1,   // todo: note this workaround for bug explained above
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED
                );
            global_ghoster.system.fullscreen = true;
        }

        // move to top so we're above taskbar
        // todo: only works if we set as topmost.. setting it temporarily for now
        SetWindowPos(global_ghoster.system.window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    else
    {
        // ShowWindow(global_ghoster.system.window, SW_RESTORE);

        if (global_ghoster.system.last_win_pos.length)
        {
            // restore our old position todo: replace if we get SW_MAXIMIZE / SW_RESTORE working
            SetWindowPos(
                global_ghoster.system.window,
                0,
                global_ghoster.system.last_win_pos.rcNormalPosition.left,
                global_ghoster.system.last_win_pos.rcNormalPosition.top,
                global_ghoster.system.last_win_pos.rcNormalPosition.right -
                global_ghoster.system.last_win_pos.rcNormalPosition.left,
                global_ghoster.system.last_win_pos.rcNormalPosition.bottom -
                global_ghoster.system.last_win_pos.rcNormalPosition.top,
                0);
        }
        global_ghoster.system.fullscreen = false;


        // unset our temp topmost from fullscreening if we aren't actually set that way
        if (!global_ghoster.state.topMost)
        {
            SetWindowPos(global_ghoster.system.window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }

        // make this an option... (we might want to keep it in the corner eg)
        // int mouseX = GET_X_LPARAM(lParam);
        // int mouseY = GET_Y_LPARAM(lParam);
        // int winX = mouseX - winWID/2;
        // int winY = mouseY - winHEI/2;
        // MoveWindow(hwnd, winX, winY, winWID, winHEI, true);
    }
}

void toggleFullscreen()
{
    WINDOWPLACEMENT winpos;
    winpos.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(global_ghoster.system.window, &winpos))
    {
        if (winpos.showCmd == SW_MAXIMIZE || global_ghoster.system.fullscreen)
        {
            setFullscreen(false);
        }
        else
        {
            setFullscreen(true);
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
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable)
    {
        style = style | WS_EX_TRANSPARENT;
    }
    else
    {
        style = style & ~WS_EX_TRANSPARENT;
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, style);
}
void setOpacity(HWND hwnd, double opacity, bool userRequest = true)
{
    if (opacity < 0) opacity = 0;
    if (opacity > 1) opacity = 1;
    if (global_ghoster.state.clickThrough && opacity > GHOST_MODE_MAX_OPACITY)
        opacity = GHOST_MODE_MAX_OPACITY;
    if (FORCE_NON_ZERO_OPACITY)
    {
        if (opacity < 0.01) // if we become completely invisible, we'll lose clicks
            opacity = 0.01;
    }

    // if we manually change opacity, don't restore it when leaving ghost mode
    if (userRequest)
    {
        if (global_ghoster.state.had_to_cache_opacity && opacity != global_ghoster.state.last_opacity)
            global_ghoster.state.had_to_cache_opacity = false;
    }

    global_ghoster.state.opacity = opacity;
    SetLayeredWindowAttributes(global_ghoster.system.window, 0, 255.0*opacity, LWA_ALPHA);
}
void setVolume(double volume)
{
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    global_ghoster.state.volume = volume;
}

void setGhostMode(HWND hwnd, bool enable)
{
    global_ghoster.state.clickThrough = enable;
    setClickThrough(hwnd, enable);
    if (enable)
    {
        SetIcon(hwnd, global_icon_w);
        if (GHOST_MODE_SETS_TOPMOST) setTopMost(hwnd, true);
        if (global_ghoster.state.opacity > GHOST_MODE_MAX_OPACITY)
        {
            global_ghoster.state.last_opacity = global_ghoster.state.opacity;
            global_ghoster.state.had_to_cache_opacity = true;
            global_ghoster.state.opacity = GHOST_MODE_MAX_OPACITY;
            setOpacity(hwnd, global_ghoster.state.opacity, false);
        }
        else
        {
            global_ghoster.state.had_to_cache_opacity = false;
        }
    }
    else
    {
        SetIcon(hwnd, global_ghoster.system.icon);
        if (global_ghoster.state.had_to_cache_opacity)
        {
            global_ghoster.state.opacity = global_ghoster.state.last_opacity;
            setOpacity(hwnd, global_ghoster.state.opacity, false);
        }
    }
}



// We enumerate all Windows, until we find one, that has the SHELLDLL_DefView as a child.
// If we found that window, we take its next sibling and assign it to workerw.
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND p = FindWindowEx(hwnd, 0, "SHELLDLL_DefView", 0);

    if (p != 0)
    {
        // Gets the WorkerW Window after the current one.
        global_workerw = FindWindowEx(0, hwnd, "WorkerW", 0);
    }

    return true;
}

void setWallpaperMode(HWND hwnd, bool enable)
{

    // cleanup first if we are already in wallpaper mode and trying to enable
    if (global_ghoster.state.wallpaperMode && enable)
    {
        if (global_wallpaper_window)
            DestroyWindow(global_wallpaper_window);
    }

    global_ghoster.state.wallpaperMode = enable;
    if (enable)
    {
        setTopMost(hwnd, false); // todo: test if needed

        // wallpaper mode method via
        // https://www.codeproject.com/Articles/856020/Draw-Behind-Desktop-Icons-in-Windows

        HWND progman = FindWindow("Progman", "Program Manager");

        // Send 0x052C to Progman. This message directs Progman to spawn a
        // WorkerW behind the desktop icons. If it is already there, nothing happens.
        u64 *result = 0;
        SendMessageTimeout(
            progman,
            0x052C,
            0,
            0,
            SMTO_NORMAL,
            1000, //timeout in ms
            result);

        EnumWindows(EnumWindowsProc, 0);

        if (!global_workerw)
        {
            MsgBox("Unable to find WorkerW!");
        }

        // which of these work seems a bit intermittent
        // maybe something we're doing is effecting which one works
        // right now the known working method is...
        // -SW_HIDE workerw
        // -create window
        // -setparent of window to progman

        // HWND newParent = global_workerw;  // may have to use this in 8+
        HWND newParent = progman;


        // trick is to hide this or it could intermittently hide our own window
        // todo: bug: once we hide this, if we minimize our window it appears on the desktop again!
        ShowWindow(global_workerw, SW_HIDE);


        // // alt idea.. might be able to get something like this to work instead of
        // // making a whole new window and parenting it to progman...
        // // though, I couldn't get this work right
        // // it seems the "show desktop" function in the bottom right is
        // // different than just a SW_MINIMIZE
        // // source: when we do a "show desktop" after SW_HIDEing workerW, we get a kind of wallpaper mode
        // // (if you test again, don't forget to continue to render to our main window)
        // setTopMost(hwnd, false); // todo: test if needed
        // ShowWindow(global_ghoster.system.window, SW_MINIMIZE);



        // create our new window
        // we need a new window so we can discard it when we're done with wallpaper mode
        // trying to salvage a window after setparent seems to cause a number of issues

        RECT win;
        GetWindowRect(hwnd, &win);

        // // not sure if these are needed, has worked with and without
        // SetWindowPos(newParent, HWND_BOTTOM, win.left, win.top, win.right-win.left, win.bottom-win.top, 0);
        // ShowWindow(newParent, SW_MAXIMIZE);

        global_wallpaper_window = CreateWindowEx(
            0,
            // WS_EX_TOOLWINDOW, // don't show taskbar
            WALLPAPER_CLASS_NAME, "ghoster video player wallpaper",
            WS_POPUP | WS_VISIBLE,
            win.left, win.top,
            win.right - win.left,
            win.bottom - win.top,
            0,
            0, global_hInstance, 0);

        // set dc format to be same as our main dc
        HDC hdc = GetDC(global_wallpaper_window);
        PIXELFORMATDESCRIPTOR pixel_format = {};
        pixel_format.nSize = sizeof(pixel_format);
        pixel_format.nVersion = 1;
        pixel_format.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
        pixel_format.iPixelType = PFD_TYPE_RGBA;
        pixel_format.cColorBits = 32;
        pixel_format.cAlphaBits = 8;
        int format_index = ChoosePixelFormat(hdc, &pixel_format);
        SetPixelFormat(hdc, format_index, &pixel_format);
        if (!global_ghoster.system.window) { MsgBox("Couldn't open wallpaper window."); }

        // only need this if we're using the new wallpaper window icon in the taskbar
        if (global_ghoster.state.clickThrough)
            SetIcon(global_wallpaper_window, global_icon_w);
        else
            SetIcon(global_wallpaper_window, global_ghoster.system.icon);


        // only seems to work when setting after window has been created
        SetParent(global_wallpaper_window, newParent);


        // actually this method seems to work pretty well...
        ShowWindow(hwnd, SW_HIDE);
        // with this method, not sure how to prevent main window from showing when clicking on sys tray icon
        // SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);  // make invisible
        // setClickThrough(hwnd, true);

    }
    else
    {
        // try this to undo our unwanted wallpaper mode bug when using "show desktop"
        // seems to work, but we also have to show on program exit
        // the only problem is if we have two ghoster apps open, one could hide this
        // and the other one could be bugged out when using "show desktop" when not topmost
        // not to mention any other apps that could be bugged out by hiding this
        // UPDATE: some more info on this,
        // when "show desktop" is done, it first minimizes all "minimizable" windows
        // then brings the Z order of the desktop to the front...
        // so maybe is there a way to make our main window minimizable?
        // EDIT: WS_MINIMIZEBOX seemed to do the trick, now everything works as expected
        // (though there could theoretically be other windows that aren't minimizable
        // (that are effected while a ghoster window is in wallpaper mode)
        if (global_workerw)
            ShowWindow(global_workerw, SW_SHOW);

        if (global_wallpaper_window)
            DestroyWindow(global_wallpaper_window);

        ShowWindow(hwnd, SW_SHOW);
        // setOpacity(hwnd, global_ghoster.state.opacity);
        // setClickThrough(hwnd, global_ghoster.state.clickThrough);
    }
}


// todo: if we have a "videostate" struct we could just copy/restore it for these functions? hmm

// note we're saving this every time we click down (since it could be the start of a double click)
// so don't make this too crazy
void saveVideoPositionForAfterDoubleClick()
{
    global_ghoster.message.savestate_is_saved = true;

    global_ghoster.loaded_video.elapsed = global_ghoster.loaded_video.audio_stopwatch.MsElapsed() / 1000.0;
    double percent = global_ghoster.loaded_video.elapsed / global_ghoster.loaded_video.duration;
    global_ghoster.message.seekProportion = percent; // todo: make new variable rather than co-opt this one?
}

void restoreVideoPositionAfterDoubleClick()
{
    if (global_ghoster.message.savestate_is_saved)
    {
        global_ghoster.message.savestate_is_saved = false;

        global_ghoster.message.setSeek = true;
    }
}



bool clientPointIsOnProgressBar(int x, int y)
{
    return y >= global_ghoster.system.winHEI-(PROGRESS_BAR_H+PROGRESS_BAR_B) &&
           y <= global_ghoster.system.winHEI-PROGRESS_BAR_B;
}
bool screenPointIsOnProgressBar(HWND hwnd, int x, int y)
{
    POINT newPoint = {x, y};
    ScreenToClient(hwnd, &newPoint);
    return clientPointIsOnProgressBar(newPoint.x, newPoint.y);
}


void appSetProgressBar(int clientX, int clientY)
{
    if (clientPointIsOnProgressBar(clientX, clientY)) // check here or outside?
    {
        double prop = (double)clientX / (double)global_ghoster.system.winWID;

        global_ghoster.message.setSeek = true;
        global_ghoster.message.seekProportion = prop;
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
    if (GetMonitorInfo(MonitorFromWindow(global_ghoster.system.window, MONITOR_DEFAULTTOPRIMARY), &mi))
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
        if (winpos.showCmd == SW_MAXIMIZE || global_ghoster.system.fullscreen)
        {
            // ShowWindow(hwnd, SW_RESTORE);

            SetWindowPos(
                global_ghoster.system.window,
                0,
                global_ghoster.system.last_win_pos.rcNormalPosition.left,
                global_ghoster.system.last_win_pos.rcNormalPosition.top,
                global_ghoster.system.last_win_pos.rcNormalPosition.right -
                global_ghoster.system.last_win_pos.rcNormalPosition.left,
                global_ghoster.system.last_win_pos.rcNormalPosition.bottom -
                global_ghoster.system.last_win_pos.rcNormalPosition.top,
                0);
            global_ghoster.system.fullscreen = false;

            // move window to mouse..
            int mouseX = x;
            int mouseY = y;
            int winX = mouseX - global_ghoster.system.winWID/2;
            int winY = mouseY - global_ghoster.system.winHEI/2;
            MoveWindow(hwnd, winX, winY, global_ghoster.system.winWID, global_ghoster.system.winHEI, true);
        }
    }

    global_ghoster.system.mDown = false; // kind of out-of-place but mouseup() is not getting called after drags
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}



void onMouseMove(HWND hwnd, int clientX, int clientY)
{
    // this is for progress bar timeout.. rename/move?
    global_ghoster.system.msOfLastMouseMove = global_ghoster.state.app_timer.MsSinceStart();

    if (global_ghoster.system.mDown)
    {
        // need to determine if click or drag here, not in buttonup
        // because mousemove will trigger (i think) at the first pixel of movement
        POINT mPos = { clientX, clientY };
        double dx = (double)mPos.x - (double)global_ghoster.system.mDownPoint.x;
        double dy = (double)mPos.y - (double)global_ghoster.system.mDownPoint.y;
        double distance = sqrt(dx*dx + dy*dy);
        double MOVEMENT_ALLOWED_IN_CLICK = 2.5;
        if (distance <= MOVEMENT_ALLOWED_IN_CLICK)
        {
            // we haven't moved enough to be considered a drag
            // or to eliminate a double click possibility (edit: although system handles that now)
        }
        else
        {
            global_ghoster.system.mouseHasMovedSinceDownL = true;

            if (clientPointIsOnProgressBar(global_ghoster.system.mDownPoint.x,
                                           global_ghoster.system.mDownPoint.y))
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
    if (DEBUG_MCLICK_MSGS) OutputDebugString("DELAYED M LUP\n");

    KillTimer(0, singleClickTimerID);
    {
        global_ghoster.appTogglePause();

        // we have to track whether get here or not
        // so we know if we've toggled pause between our double click or not
        // (it could be either case now that we have a little delay in our pause)
        global_ghoster.message.toggledPauseOnLastSingleClick = true;
    }
}

void onMouseUpL()
{
    if (DEBUG_MCLICK_MSGS) OutputDebugString("LUP\n");

    global_ghoster.system.mDown = false;

    if (global_awkward_next_mup_was_closing_menu)
        return;

    if (global_ghoster.system.mouseHasMovedSinceDownL)
    {
        // end of a drag
        // todo: i don't think we ever actually get here on the end of a drag
    }
    else
    {
        if (!clientPointIsOnProgressBar(global_ghoster.system.mDownPoint.x,
                                        global_ghoster.system.mDownPoint.y))
        {
            // since this could be the mouse up in between the two clicks of a double click,
            // wait a little bit before we actually pause (should work with 0 delay as well)

            // don't queue up another pause if this is the end of a double click
            // we'll be restoring our pause state in restoreVideoPositionAfterDoubleClick() below
            if (!global_ghoster.message.next_mup_was_double_click)
            {
                global_ghoster.message.toggledPauseOnLastSingleClick = false; // we haven't until the timer runs out
                singleClickTimerID = SetTimer(NULL, 0, MS_PAUSE_DELAY_FOR_DOUBLECLICK, &onSingleClickL);
            }
            else
            {
                // if we are ending the click and we already registered the first click as a pause,
                // toggle pause again to undo that
                if (global_ghoster.message.toggledPauseOnLastSingleClick)
                {
                    // OutputDebugString("undo that click\n");
                    global_ghoster.appTogglePause();
                }
            }
        }
    }

    global_ghoster.system.mouseHasMovedSinceDownL = false;
    global_ghoster.system.clickingOnProgressBar = false;

    if (global_ghoster.message.next_mup_was_double_click)
    {
        global_ghoster.message.next_mup_was_double_click = false;

        // cancel any pending single click effects lingering from the first mup of this dclick
        KillTimer(0, singleClickTimerID);

        // only restore if we actually paused/unpaused, otherwise we can just keep everything rolling as is
        if (global_ghoster.message.toggledPauseOnLastSingleClick)
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
    if (DEBUG_MCLICK_MSGS) OutputDebugString("LDOUBLECLICK\n");

    if (clientPointIsOnProgressBar(global_ghoster.system.mDownPoint.x, global_ghoster.system.mDownPoint.y))
    {
        // OutputDebugString("on bar dbl\n");
        global_ghoster.system.clickingOnProgressBar = true;
        return;
    }

    toggleFullscreen();

    // note we actually have to do this in mouse up because that's where the vid gets paused
    // restoreVideoPositionAfterDoubleClick();

    // instead make a note to restore in mouseUp
    global_ghoster.message.next_mup_was_double_click = true;

}

void onMouseDownL(int clientX, int clientY)
{
    // OutputDebugString("LDOWN\n");

    // i think we can just ignore if context menu is open
    if (global_ghoster.system.contextMenuOpen)
        return;

    // mouse state / info about click...
    global_ghoster.system.mDown = true;
    global_ghoster.system.mouseHasMovedSinceDownL = false;
    global_ghoster.system.mDownPoint = {clientX, clientY};

    if (clientPointIsOnProgressBar(clientX, clientY))
    {
        // OutputDebugString("on bar\n");
        global_ghoster.system.clickingOnProgressBar = true;
        appSetProgressBar(clientX, clientY);
    }
    else
    {
        // note this works because onMouseDownL doesn't trigger on the second click of a double click
        saveVideoPositionForAfterDoubleClick();
    }
}



// needs global ghoster and a number of its functions
#include "menu.cpp"



static int sys_moving_anchor_x;
static int sys_moving_anchor_y;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {

        // doesn't help with flicker
        // case WM_ERASEBKGND: {
        //     return 1; // don't erase
        // } break;

        case WM_MEASUREITEM: {
            LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT) lParam;
            lpmis->itemWidth = 200;
            lpmis->itemHeight = 20;
        } break;
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT) lParam;


            RECT container = lpdis->rcItem;
            container.left += 20;
            container.right -= 40;
            container.top += 1;
            container.bottom -= 1;

            float percent = global_ghoster.state.opacity;
            int width = container.right - container.left;
            int sliderPos = container.left + nearestInt(width*percent);

            RECT blue;
            blue.left   = container.left   + 1;  // +1 to leave pixel border on top left
            blue.top    = container.top    + 1;
            blue.right  = sliderPos        + 1;  // +1 to  cover up bottom right border
            blue.bottom = container.bottom + 1;

            RECT empty;
            empty.left   = sliderPos        + 1;  // +1 to  leave pixel border on top left
            empty.top    = container.top    + 1;
            empty.right  = container.right  + 1;  // +1 to  cover up bottom right border
            empty.bottom = container.bottom + 1;


            // HBRUSH bgbrush = (HBRUSH) ;
            // // if (lpdis->itemState == ODS_SELECTED)
            // bgbrush = CreateSolidBrush(0xE6D8AD);
            // SetDCPenColor(lpdis->hDC, 0xE6D8AD);
            // SetBkMode(lpdis->hDC, TRANSPARENT);

            SetBkMode(lpdis->hDC, OPAQUE);

            SelectObject(lpdis->hDC, CreatePen(PS_SOLID, 1, 0x888888));
            SelectObject(lpdis->hDC, GetStockObject(HOLLOW_BRUSH));
            Rectangle(lpdis->hDC, container.left, container.top, container.right, container.bottom);

            SetBkMode(lpdis->hDC, TRANSPARENT);

            SelectObject(lpdis->hDC, CreateSolidBrush(0xE6D8AD));
            SelectObject(lpdis->hDC, GetStockObject(NULL_PEN));
            Rectangle(lpdis->hDC, blue.left, blue.top, blue.right, blue.bottom);

            SelectObject(lpdis->hDC,  CreateSolidBrush(0xe0e0e0));
            SelectObject(lpdis->hDC, GetStockObject(NULL_PEN));
            Rectangle(lpdis->hDC, empty.left, empty.top, empty.right, empty.bottom);

            char volbuf[123];
            sprintf(volbuf, "Volume %i", nearestInt(global_ghoster.state.opacity*100.0));
            container.left += 2;
            container.top += 2;
            DrawText(lpdis->hDC, volbuf, -1, &container, DT_CENTER);


        } break;


        // is this needed?
        // case WM_DESTROY: {
        //     RemoveSysTrayIcon(hwnd);
        // } break;

        case WM_CLOSE: {
            global_ghoster.state.appRunning = false;
        } break;

        case WM_SIZE: {
            SetWindowSize(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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

            POINT pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
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
            onMouseDownL(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        } break;
        // case WM_NCLBUTTONDOWN: {
        // } break;
        case WM_LBUTTONDBLCLK: {
            onDoubleClickDownL();
        } break;

        case WM_MOUSEMOVE: {
            onMouseMove(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        } break;

        case WM_LBUTTONUP:
        case WM_NCLBUTTONUP: {
            onMouseUpL();
        } break;

        case WM_RBUTTONDOWN: {    // rclicks in client area (HTCLIENT)
            POINT openPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &openPoint);
            OpenRClickMenuAt(hwnd, openPoint);
        } break;
        case WM_NCRBUTTONDOWN: {  // non-client area, apparently lParam is treated diff?
            POINT openPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
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
                if (global_ghoster.system.ctrlDown)
                {
                    PasteClipboard();
                }
            }
            if (wParam == 0x11) // ctrl
            {
                global_ghoster.system.ctrlDown = true;
            }
        } break;

        case WM_KEYUP: {
            if (wParam == 0x11) // ctrl
            {
                global_ghoster.system.ctrlDown = false;
            }
            if (wParam >= 0x30 && wParam <= 0x39) // 0-9
            {
                GlobalLoadMovie(TEST_FILES[wParam - 0x30]);
            }
        } break;

        // note this is NOT called via show desktop unless we are already able to be minimized, i think
        // case WM_SYSCOMMAND: {
        //     switch (wParam) {
        //         case SC_MINIMIZE:
        //             OutputDebugString("MINIMIZE!");
        //             break;
        //     }
        // } break;

        // case WM_COMMAND: {
        //     OutputDebugString("COMMAD");
        //     switch (wParam)
        //     {
        //         case ID_VOLUME:
        //             return 0;
        //             break;
        //         case ID_EXIT:
        //             global_ghoster.state.appRunning = false;
        //             break;
        //         case ID_PAUSE:
        //             appTogglePause();
        //             break;
        //         case ID_ASPECT:
        //             SetWindowToAspectRatio(global_ghoster.system.window, global_ghoster.loaded_video.aspect_ratio);
        //             global_ghoster.state.lock_aspect = !global_ghoster.state.lock_aspect;
        //             break;
        //         case ID_PASTE:
        //             PasteClipboard();
        //             break;
        //         case ID_RESET_RES:
        //             SetWindowToNativeRes(global_ghoster.system.window, global_ghoster.loaded_video);
        //             break;
        //         case ID_REPEAT:
        //             global_ghoster.state.repeat = !global_ghoster.state.repeat;
        //             break;
        //         case ID_TRANSPARENCY:
        //             global_ghoster.state.transparent = !global_ghoster.state.transparent;
        //             if (global_ghoster.state.transparent) setOpacity(global_ghoster.system.window, 0.5);
        //             if (!global_ghoster.state.transparent) setOpacity(global_ghoster.system.window, 1.0);
        //             break;
        //         case ID_CLICKTHRU:
        //             setGhostMode(global_ghoster.system.window, !global_ghoster.state.clickThrough);
        //             break;
        //         case ID_RANDICON:
        //             global_ghoster.system.icon = RandomIcon();
        //             if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
        //             break;
        //         case ID_TOPMOST:
        //             setTopMost(global_ghoster.system.window, !global_ghoster.state.topMost);
        //             break;
        //         int color;
        //         case ID_SET_C: color = 0;
        //             global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
        //             if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
        //             break;
        //         case ID_SET_P: color = 1;
        //             global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
        //             if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
        //             break;
        //         case ID_SET_R: color = 2;
        //             global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
        //             if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
        //             break;
        //         case ID_SET_Y: color = 3;
        //             global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
        //             if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
        //             break;
        //         case ID_FULLSCREEN:
        //             toggleFullscreen();
        //             break;
        //         case ID_SNAPPING:
        //             global_ghoster.state.enableSnapping = !global_ghoster.state.enableSnapping;
        //             if (global_ghoster.state.enableSnapping)
        //             {
        //                 RECT winRect; GetWindowRect(global_ghoster.system.window, &winRect);

        //                 SnapRectToMonitor(winRect, &winRect);

        //                 SetWindowPos(global_ghoster.system.window, 0,
        //                     winRect.left, winRect.top,
        //                     winRect.right  - winRect.left,
        //                     winRect.bottom - winRect.top,
        //                     0);
        //             }
        //             break;
        //         case ID_WALLPAPER:
        //             setWallpaperMode(global_ghoster.system.window, !global_ghoster.state.wallpaperMode);
        //             break;

        //     }
        // } break;


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
                    // if (!global_ghoster.state.wallpaperMode)
                    // {
                        SetForegroundWindow(hwnd);
                        setGhostMode(hwnd, !global_ghoster.state.clickThrough);
                    // }
                break;
                case WM_RBUTTONUP:
                    POINT mousePos;
                    GetCursorPos(&mousePos);
                    // SetForegroundWindow(hwnd); // this is done in openRclick
                    OpenRClickMenuAt(hwnd, mousePos);
                break;
            }
        } break;


    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}



static HHOOK mouseHook;
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // PKBDLLHOOKSTRUCT k = (PKBDLLHOOKSTRUCT)(lParam);

    if (wParam == WM_RBUTTONDOWN || wParam == WM_LBUTTONDOWN)
    {
        // GetCursorPos(&p);
        POINT point = ((MSLLHOOKSTRUCT*)(lParam))->pt;

        RECT popupRect;
        RECT submenuRect;
        GetWindowRect(global_popup_window, &popupRect);
        GetWindowRect(global_icon_menu_window, &submenuRect);
        if (!PtInRect(&popupRect, point) && !PtInRect(&submenuRect, point))
        {
            if (global_ghoster.system.contextMenuOpen)
            {
                global_awkward_next_mup_was_closing_menu = true;
            }
            HideSubMenu();
            ClosePopup(global_popup_window);
        }
    }

    return CallNextHookEx(0, nCode, wParam, lParam);
}

void DirectoryFromPath(char *path)
{
    char *new_end = 0;
    for (char *c = path; *c; c++)
    {
        if (*c == '\\' || *c == '/')
            new_end = c+1;
    }
    if (new_end != 0)
        *new_end = '\0';
}

int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    global_hInstance = hInstance;


    assert(Test_SecondsFromStringTimestamp());


    // load icons/bitmaps
    MakeIcons(hInstance);


    global_ghoster.Init();



    // COMMAND LINE ARGS

    wchar_t **argList;
    int argCount;
    argList = CommandLineToArgvW(GetCommandLineW(), &argCount);
    if (argList == 0)
    {
        MsgBox("CommandLineToArgvW failed.");
    }

    // make note of exe directory
    global_exe_directory = (char*)malloc(MAX_PATH); // todo: what to use?
    wcstombs(global_exe_directory, argList[0], MAX_PATH);
    DirectoryFromPath(global_exe_directory);
        // OutputDebugString("\n\n\n\n");
        // OutputDebugString(global_exe_directory);
        // OutputDebugString("\n\n\n\n");

    bool startInGhostMode = false;
    for (int i = 1; i < argCount; i++)  // skip first one which is name of exe
    {
        char filePathOrUrl[256]; // todo what max to use
        wcstombs(filePathOrUrl, argList[i], 256);
        if (filePathOrUrl[0] != '-')
        {
            GlobalLoadMovie(filePathOrUrl);
        }

        if (strcmp(filePathOrUrl, "-top") == 0)
        {
            global_ghoster.state.topMost = true;
        }
        if (strcmp(filePathOrUrl, "-notop") == 0)
        {
            global_ghoster.state.topMost = false;
        }

        if (strcmp(filePathOrUrl, "-aspect") == 0)
        {
            global_ghoster.state.lock_aspect = true;
        }
        if (strcmp(filePathOrUrl, "-noaspect") == 0 || strcmp(filePathOrUrl, "-stretch") == 0)
        {
            global_ghoster.state.lock_aspect = false;
        }

        if (strcmp(filePathOrUrl, "-repeat") == 0)
        {
            global_ghoster.state.repeat = true;
        }
        if (strcmp(filePathOrUrl, "-norepeat") == 0)
        {
            global_ghoster.state.repeat = false;
        }

        if (strcmp(filePathOrUrl, "-ghost") == 0)
        {
            startInGhostMode = true;
        }
        if (strcmp(filePathOrUrl, "-noghost") == 0)
        {
            startInGhostMode = false;
        }

        if (strcmp(filePathOrUrl, "-snap") == 0)
        {
            global_ghoster.state.enableSnapping = true;
        }
        if (strcmp(filePathOrUrl, "-nosnap") == 0)
        {
            global_ghoster.state.enableSnapping = false;
        }

        if (strcmp(filePathOrUrl, "-wall") == 0)
        {
            global_ghoster.state.wallpaperMode = true;
        }
        if (strcmp(filePathOrUrl, "-nowall") == 0)
        {
            global_ghoster.state.wallpaperMode = false;
        }

        if (strcmp(filePathOrUrl, "-fullscreen") == 0)
        {
            global_ghoster.system.fullscreen = true;
        }
        if (strcmp(filePathOrUrl, "-nofullscreen") == 0)
        {
            global_ghoster.system.fullscreen = false;
        }

        if (strcmp(filePathOrUrl, "-blinky") == 0)
        {
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*2);
        }
        if (strcmp(filePathOrUrl, "-pinky") == 0)
        {
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*1);
        }
        if (strcmp(filePathOrUrl, "-inky") == 0)
        {
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*0);
        }
        if (strcmp(filePathOrUrl, "-clyde") == 0)
        {
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*3);
        }

        if (StringBeginsWith(filePathOrUrl, "-opac"))
        {
            char *opacNum = filePathOrUrl + 5; // 5 = length of "-opac"
            global_ghoster.state.opacity = (double)atoi(opacNum) / 100.0;
        }

        if (StringBeginsWith(filePathOrUrl, "-vol"))
        {
            char *volNum = filePathOrUrl + 4; // 4 = length of "-vol"
            global_ghoster.state.volume = (double)atoi(volNum) / 100.0;
        }

        // todo: many settings here are coupled with the setX() functions called below
        // maybe move this whole arg parsing below window creation so we don't have this two-step process?
    }


    // FFMPEG
    InitAV();  // basically just registers all codecs.. call when needed instead?


    // install mouse hook so we know when we click outside of a menu (to close it)
    // (could also use this to detect clicks on an owner-draw menu item)
    // (probably would have been easier than redrawing our own entire menu)
    // (but we'd have been stuck with an old non-themed menu that way)
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, hInstance, 0);


    // register class for wallpaper window if we ever use one
    WNDCLASS wc2 = {};
    wc2.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc2.lpfnWndProc =  DefWindowProc;//WndProc;
    wc2.hInstance = hInstance;
    wc2.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc2.lpszClassName = WALLPAPER_CLASS_NAME;
    if (!RegisterClass(&wc2)) { MsgBox("RegisterClass for wallpaper window failed."); return 1; }



    // create windows for popup menu
    global_popup_window = InitPopupMenu(hInstance, menuItems, sizeof(menuItems)/sizeof(menuItem));



    // WINDOW

    // register wndproc
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "ghoster window class";
    if (!RegisterClass(&wc)) { MsgBox("RegisterClass failed."); return 1; }

    RECT neededRect = {0};
    neededRect.right = 960;
    neededRect.bottom = 720;

    // HWND
    global_ghoster.system.window = CreateWindowEx(
        WS_EX_LAYERED,
        wc.lpszClassName, "ghoster video player",
        WS_MINIMIZEBOX |   // i swear sometimes we can't shrink (via show desktop) without WS_MINIMIZEBOX
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        neededRect.right - neededRect.left, neededRect.bottom - neededRect.top,
        0, 0, hInstance, 0);

    if (!global_ghoster.system.window) { MsgBox("Couldn't open window."); }


    global_ghoster.ResizeWindow(neededRect.right, neededRect.bottom);


    // /*
    // setup starting options based on command args / defaults (defaults are set in struct)....

    if (!global_ghoster.system.icon)
        global_ghoster.system.icon = RandomIcon();

    AddSysTrayIcon(global_ghoster.system.window); // sets as default icon
    SetIcon(global_ghoster.system.window, global_ghoster.system.icon);


    setOpacity(global_ghoster.system.window, global_ghoster.state.opacity);
    setTopMost(global_ghoster.system.window, global_ghoster.state.topMost);

    // do not call here, wait until movie as been loaded and window is correct size
    // setFullscreen(global_ghoster.system.fullscreen);

    setVolume(global_ghoster.state.volume);

    if (startInGhostMode)
        setGhostMode(global_ghoster.system.window, startInGhostMode);

    // this has to be called after loading video so size is correct (maybe other things too)
    // (it's now called after every new video which is better anyway)
    // if (global_ghoster.state.wallpaperMode)
    //     setWallpaperMode(global_ghoster.system.window, global_ghoster.state.wallpaperMode);

    // end options setup
    // */


    // ENABLE DRAG DROP
    DragAcceptFiles(global_ghoster.system.window, true);


    // MAIN APP LOOP
    CreateThread(0, 0, RunMainLoop, 0, 0, 0);


    // MSG LOOP
    while (global_ghoster.state.appRunning)
    {
        MSG Message;
        // while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        // {
        //     // char msgbuf[123];
        //     // sprintf(msgbuf, "msg: %i\n", Message.message);
        //     // OutputDebugString(msgbuf);

        //     TranslateMessage(&Message);
        //     DispatchMessage(&Message);
        // }
        // Sleep(1);

        BOOL ret;
        while (ret = GetMessage(&Message, 0, 0, 0) && global_ghoster.state.appRunning)
        {
            if (ret == -1)
            {
               global_ghoster.state.appRunning = false;
            }
            else
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }
        }
    }

    // if (global_workerw) CloseWindow(global_workerw);
    RemoveSysTrayIcon(global_ghoster.system.window);
    UnhookWindowsHookEx(mouseHook);

    // show this again if we happen to be in wallpaper mode on exit
    // if not, the next time user changes wallpaper it won't transition gradually
    // (but only on the first transition)
    // there could be other consequences too like windows being "put on the desktop"
    // rather than minimized when using "show desktop" (which is how we found this is needed)
    if (global_workerw)
        ShowWindow(global_workerw, SW_SHOW);

    return 0;
}