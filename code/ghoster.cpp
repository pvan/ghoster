#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM
#include <stdio.h>
#include <math.h>

// for drag drop, sys tray icon, command line args
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")


#include "types.h"
#include "directx.cpp"
#include "text.cpp"


// these are the things movie needs that it shouldn't
void SetSplash(char *msg, u32 col = 0xffffffff);
#include "movie/movie.cpp"

// these are the things glass needs that it shouldn't
// todo: what about making progress bar into a child window?
bool screenPointIsOnProgressBar(HWND hwnd, int x, int y);
void hacky_extra_toggle_pause_function_for_glass();
#include "glass/glass.cpp"

#include "urls.h"
#include "icons.h"
#include "dragdrop.h"



const int PROGRESS_BAR_H = 22;  // progress bar position from bottom

const double PROGRESS_BAR_TIMEOUT = 1.0;  // hide progress bar after this many seconds

const double SEC_OF_SPLASH_MSG = 2; // how long our splash messages last (fade included)

const bool D3D_DEBUG_MSG = false;



// these are the things menu needs
static MovieProjector projector;
bool is_letterbox = false;
bool is_720p = true;
void set_max_quality(int qual);
bool paste_clipboard();
bool copy_url_to_clipboard(bool withTimestamp = false);
void queue_random_url();
void resize_win_to_native_res();
#include "menu.cpp"



char *debug_string;
bool show_debug = false;

bool show_bar = false;
double secOfLastMouseMove = -1;

char splash_msg[1024];
u32 splash_color;
double splash_sec_left = 0;

d3d_textured_quad screen;
d3d_textured_quad splash_quad;
d3d_textured_quad overlay_quad;
d3d_textured_quad debug_quad;
d3d_textured_quad progress_bar_gray;
d3d_textured_quad progress_bar_red;

void create_quads()
{
    u32 gray = 0xaaaaaaaa;
    u32 red = 0xffff0000;
    progress_bar_gray.create((u8*)&gray,1,1, -1,-1,1,1,0);
    progress_bar_red.create((u8*)&red,1,1, -1,-1,1,1,0);

    u32 green = 0xff00ff00;
    screen.create((u8*)&green,1,1, -1,-1,1,1,0);
}

void SetSplash(char *msg, u32 col)
{
    TransmogrifyTextInto(splash_msg, msg); // todo: check length somehow hmm...
    splash_color = col;
    splash_sec_left = SEC_OF_SPLASH_MSG;
}
void ClearSplash() { splash_msg[0] = '\0'; splash_sec_left = 0; }

void queue_random_url()
{
    projector.QueueLoadFromPath(get_random_url());
    // int r = getUnplayedIndex();
    // projector.QueueLoadFromPath(RANDOM_ON_LAUNCH[r]);
}


// assuming a canvas of 0,0,dw,dh, returns subrect filling that space but with aspect_ratio
RECT calc_pixel_letterbox_subrect(int dw, int dh, double aspect_ratio)
{
    int calcW = (int)((double)dh * aspect_ratio);
    int calcH = (int)((double)dw / aspect_ratio);
    if (calcW > dw) calcW = dw;  // letterbox
    else calcH = dh;  // pillarbox
    int posX = (int)(((double)dw - (double)calcW) / 2.0);
    int posY = (int)(((double)dh - (double)calcH) / 2.0);
    return {posX, posY, posX+calcW, posY+calcH};
}
// assuming a canvas of 0,0,dw,dh, returns a larger rect (negative position) that fills the canvas with aspect_ratio
RECT calc_pixel_stretch_rect(int dw, int dh, double aspect_ratio)
{
    int calcW = (int)((double)dh * aspect_ratio);
    int calcH = (int)((double)dw / aspect_ratio);
    if (calcW > dw) calcH = dh;  // sides will be trimmed
    else calcW = dw;  // top/bottom will be trimmed
    int posX = (int)(((double)dw - (double)calcW) / 2.0);
    int posY = (int)(((double)dh - (double)calcH) / 2.0);
    return {posX, posY, posX+calcW, posY+calcH};
}


void render()  // os msg pump thread
{
    if (!glass.loop_running) return;  // kinda smells

    double temp_dt = glass.sleep_ms / 1000.0; // todo: get proper dt and lock it to framerate

    RECT winRect; GetWindowRect(glass.hwnd, &winRect);
    int sw = winRect.right-winRect.left;
    int sh = winRect.bottom-winRect.top;


    if (!d3d_device_is_ok()) // probably device lost from ctrl alt delete or something
    {
        if (D3D_DEBUG_MSG) OutputDebugString("D3D NOT OK\n");
        if (d3d_device_can_be_reset())
        {
            OutputDebugString("Resetting d3d entirely...\n");

            screen.destroy();
            splash_quad.destroy();
            overlay_quad.destroy();
            debug_quad.destroy();
            progress_bar_gray.destroy();
            progress_bar_red.destroy();

            // d3d_reset(sw, sh, glass.hwnd);

            d3d_cleanup();
            d3d_init(glass.hwnd, glass.get_win_width(), glass.get_win_height());
            create_quads();

        }
        else
        {
            if (D3D_DEBUG_MSG) OutputDebugString("D3D CANNOT BE RESET\n");
        }
    }



    if (GetWallClockSeconds() - secOfLastMouseMove > PROGRESS_BAR_TIMEOUT)
        show_bar = false;
    if (!glass.is_mouse_in_window())
        show_bar = false;
    if (!projector.IsMovieLoaded())
        show_bar = false;


    if (!d3d_device_is_ok()) return;

    d3d_resize_if_change(sw, sh, glass.target_window());

    if (projector.front_buffer && projector.front_buffer->mem)
    {
        u8 *src = projector.front_buffer->mem;
        int w = projector.front_buffer->wid;
        int h = projector.front_buffer->hei;

        // added for autocrop
        double buffer_ratio = (double)w / (double)h;
        if (glass.aspect_ratio != buffer_ratio)
        {
            // PRINT("RESIZE\n");
            glass.aspect_ratio = (double)w / (double)h;
            glass.resize_to_aspect_ratio();
        }

        screen.fill_tex_with_mem(src, w, h);  //this resizes tex if needed, todo: better name

        if (glass.is_fullscreen && glass.is_ratiolocked) {
            RECT rect;
            if (is_letterbox)
                rect = calc_pixel_letterbox_subrect(sw, sh, projector.rolling_movie.reel.aspect_ratio);
            else
                rect = calc_pixel_stretch_rect(sw, sh, projector.rolling_movie.reel.aspect_ratio);
            screen.set_to_pixel_coords_BL(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top);
        } else {
            screen.fill_vb_with_rect(-1,-1,1,1,0);  // fill screen
        }
    } else {
        // buffering
        static float buffering_t = 0;
        buffering_t += temp_dt;
        // e^sin(x) very interesting shape, via http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
        float col = pow(M_E, sin(buffering_t*M_PI*2 / 3));  // cycle every 3sec
        float min = 1/M_E;
        float max = M_E;
        col = (col-min) / (max-min); // 0-1
        col = 0.75*col + 0.2*(1-col); //lerp

        u8 colbyte = col*255.99;
        u32 colhex = 0xff<<(8*3) | colbyte<<(8*2) | colbyte<<(8*1) | colbyte<<(8*0);
        screen.fill_tex_with_mem((u8*)&colhex, 1, 1);
    }



    if (show_debug)
    {
        projector.rolling_movie.reel.MetadataToString(debug_string);
        sprintf(debug_string+strlen(debug_string), "\n");

        glass.ToString(debug_string+strlen(debug_string));
        sprintf(debug_string+strlen(debug_string), "\n");

        sprintf(debug_string+strlen(debug_string), "secOfLastMouseMove: %f\n", secOfLastMouseMove);
        sprintf(debug_string+strlen(debug_string), "sec now: %f\n", GetWallClockSeconds());

        if (debug_string && *debug_string)
        {
            debug_quad.destroy();
            debug_quad = ttf_create(debug_string, 26, 125);
        }
        debug_quad.move_to_pixel_coords_TL(0, 0);
    }

    if (show_bar)
    {
        int progress_pixel = projector.rolling_movie.percent_elapsed() * (double)sw;
        progress_bar_gray.set_to_pixel_coords_BL(progress_pixel, 0, sw-progress_pixel, PROGRESS_BAR_H);
        progress_bar_red.set_to_pixel_coords_BL(0, 0, progress_pixel, PROGRESS_BAR_H);
    }


    double splash_alpha = 0;
    if (splash_sec_left > 0)
    {
        splash_sec_left -= temp_dt;

        double maxA = 0.65; // implicit min of 0
        splash_alpha = ((-cos(splash_sec_left*M_PI / SEC_OF_SPLASH_MSG) + 1.0) / 2.0) * maxA;

        if (splash_msg && *splash_msg) // todo check if msg changed
        {
            splash_quad.destroy();
            splash_quad = ttf_create(splash_msg, 42, 0);
        }
        splash_quad.move_to_pixel_coords_center(sw/2, sh/2);

        overlay_quad.update((u8*)&splash_color,1,1, -1,-1,1,1,0);
    }


    // todo: seems we have still not completely eliminated flicker on resize.. hrmm
    d3d_clear(0, 0, 0);
    screen.render(); // todo: strange bar on right
    if (show_bar) progress_bar_gray.render(0.4);
    if (show_bar) progress_bar_red.render(0.6);
    if (splash_sec_left > 0) { overlay_quad.render(splash_alpha); splash_quad.render(splash_alpha); }
    if (show_debug) debug_quad.render();
    d3d_swap();
}


bool clientPointIsOnProgressBar(int x, int y)
{
    int sh = glass.get_win_height();
    return y >= sh-PROGRESS_BAR_H && y <= sh;
}
bool screenPointIsOnProgressBar(HWND hwnd, int x, int y)
{
    POINT newPoint = {x, y};
    ScreenToClient(hwnd, &newPoint);
    return clientPointIsOnProgressBar(newPoint.x, newPoint.y);
}
void restore_vid_position() {
    // only do this if local file, otherwise the seeking will be too slow
    if (!StringIsUrl(projector.rolling_movie.reel.path))
        projector.RestoreVideoPosition();
    ClearSplash();  // either way clear this
}
void set_progress_bar(double percent) {
    double seconds = percent * projector.rolling_movie.reel.durationSeconds;
    projector.rolling_movie.seconds_elapsed_at_last_decode = seconds; // just set this here so it's immediate
    projector.QueueSeekToPercent(percent);
}
void set_max_quality(int qual)
{
    projector.maxQuality = qual;

    if (qual == LIMIT_QUAL)  // todo: hack to determine if we're downgrading or upgrading
        if (projector.rolling_movie.reel.height <= qual)
            return; // don't bother to reload in this case, we won't get anything different

    projector.QueueReload(true);
}

void play() { SetSplash("Play", 0xaaaaaaaa/*0x7cec7aff*/); projector.PlayMovie(); }
void pause() { SetSplash("Pause", 0xaaaaaaaa/*0xfa8686ff*/); projector.PauseMovie(); }
void toggle_pause() { if (projector.state.is_paused) play(); else pause(); }
void on_click(int x, int y) {
    // PRINT("%i, %i\n", x, y);
    if (clientPointIsOnProgressBar(x, y)) {
        // doing this on mdown and mdrag now
    } else {
        toggle_pause();
        // if (projector.state.is_paused) { play(); }
        // else { pause(); }
        // // projector.TogglePause();
    }
}
bool clickingOnProgressBar = false;
void on_mdown(int cx, int cy) {
    if (clientPointIsOnProgressBar(cx, cy)) {
        clickingOnProgressBar = true;
        set_progress_bar((double)cx/(double)glass.get_win_width());
    }
    else
    {
        projector.SaveVideoPosition(); // for undoing if this is going to be a double click
    }
}
void on_mup() { clickingOnProgressBar = false; }
void on_mouse_move(int cx, int cy) {   // only triggers in window
    show_bar = true;
    secOfLastMouseMove = GetWallClockSeconds();
}
void on_mouse_drag(int cx, int cy) {
    if (clickingOnProgressBar) {
        set_progress_bar((double)cx/(double)glass.get_win_width());
    }
    else
    {
        glass.DragWindow(cx, cy);
    }
}
void on_mouse_exit_window() { clickingOnProgressBar = false; }
void on_dragdrop_file(char *path) { projector.QueueLoadFromPath(path); }

void on_dragdrop(char *path) { if (glass.on_dragdrop_file) glass.on_dragdrop_file(path); }


// other ways? could poll IsMovieLoaded each loop? this is prolly fine
bool first_video = true;
void on_video_load(int w, int h)
{
    ClearSplash();

    glass.set_title(projector.rolling_movie.reel.title);

    glass.aspect_ratio = (double)w / (double)h;

    if (!glass.is_fullscreen)
        glass.set_ratiolocked(glass.is_ratiolocked);

    if (glass.is_fullscreen)
    {
        // todo: awkward, you can see it flicker
        glass.setFullscreen(false);
        glass.set_ratiolocked(glass.is_ratiolocked);
        glass.setFullscreen(true);
    }

    if (glass.is_wallpaper)
    {
        // this seems to be enough
        // todo: (actually it's overkill, we could just set the size of the wallpaper hwnd)
        glass.set_wallpaper(false);
        glass.set_wallpaper(true);
    }

    // something like this? (na, would be too long on urls)
    if (first_video)
    {
        first_video = false;
        // glass.ShowWindow();
    }
}


void hacky_extra_toggle_pause_function_for_glass() { toggle_pause(); }

void resize_win_to_native_res()
{
    // char hwbuf[123];
    // sprintf(hwbuf, "wid: %i  hei: %i\n",
    //     global_ghoster.rolling_movie.reel.video.codecContext->width,
    //     global_ghoster.rolling_movie.reel.video.codecContext->height);
    // OutputDebugString(hwbuf);

    RECT winRect = glass.get_win_rect();
    int vw = projector.rolling_movie.reel.vid_width;
    int vh = projector.rolling_movie.reel.vid_height;
    MoveWindow(glass.hwnd, winRect.left, winRect.top, vw, vh, true);
}

bool paste_clipboard()
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
        sprintf(printit, "Clipboard: %s\n", (char*)clipboardContents);
        OutputDebugString(printit);
    projector.QueueLoadFromPath(clipboardContents);
    free(clipboardContents);
    return true;
}

bool copy_url_to_clipboard(bool withTimestamp)
{
    char output[FFMPEG_PATH_SIZE]; // todo: stack alloc ok here?
    projector.rolling_movie.get_url(output, FFMPEG_PATH_SIZE, withTimestamp);

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


WNDPROC origWndProc;
LRESULT CALLBACK appWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_KEYDOWN: {
            if (wParam == 0x56 && GetKeyState(VK_CONTROL) & 0x8000)  // ctrl v
            {
                paste_clipboard();
            }
        } break;

        case WM_KEYUP: {
            if (wParam >= 0x30 && wParam <= 0x39 && GetKeyState(VK_CONTROL) & 0x8000) // ctrl 0-9
            {
                projector.QueueLoadFromPath(TEST_FILES[wParam - 0x30]);
            }
            // if (wParam == VK_OEM_3) // ~
            // {
            //     queue_random_url();
            // }
            if (wParam == VK_TAB)
            {
                show_debug = !show_debug;
            }
            if (wParam == 0x52 && GetKeyState(VK_CONTROL) & 0x8000) // ctrl r
            {
                queue_random_url();
            }
        } break;
    }
    return origWndProc(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    timeBeginPeriod(1); // set resolution of sleep


    LoadIcons(hInstance);


    // would be great to make this into a proper lib
    global_popup_window = InitPopupMenu(hInstance, menuItems, sizeof(menuItems)/sizeof(menuItem));



    // should we have each module parse this themselves?
    wchar_t **argList;
    int argCount;
    argList = CommandLineToArgvW(GetCommandLineW(), &argCount);
    if (argList == 0)
    {
        MessageBox(0,"CommandLineToArgvW fail!",0,0);  // here even? or not
    }
    // for (int i = 0; i < argCount; i++)
    //     MessageBoxW(0,argList[i],0,0);


    // pass exe directory to ghoster so it knows where to find youtube-dl
    // can it find its directory itself?
    char *exe_directory = (char*)malloc(MAX_PATH); // todo: what to use?
    wcstombs(exe_directory, argList[0], MAX_PATH);

    // handle differently if full path (launched from debugger or folder)
    // or just name of exe (launched from cmd)
    // can't use this when debugging i think, will get debugger directory?
    if (exe_directory[1] != ':') GetModuleFileName(0,exe_directory,MAX_PATH);

    DirectoryFromPath(exe_directory);


    projector.Init(exe_directory);
    projector.on_load_callback = on_video_load;


    // defaults
    int startX = 0;
    int startY = 0;
    int startW = 600;
    int startH = 400;
    int nest = 0; // todo: enum 0=none, 1=tl, 2=tr, 3=bl, 4=br
    int randomVideo = true;
    int randomIcon = true;
    // parse args
    for (int i = 1; i < argCount; i++)  // skip first one which is name of exe
    {
        char nextArg[256]; // todo what max to use
        wcstombs(nextArg, argList[i], 256);
        if (nextArg[0] != '-')
        {
            randomVideo = false;
            projector.QueueLoadFromPath(nextArg);
        }

        if (strcmp(nextArg, "-qualmax") == 0) is_720p = false; set_max_quality(is_720p? LIMIT_QUAL : MAX_QUAL);
        if (strcmp(nextArg, "-qual720") == 0) is_720p = true; set_max_quality(is_720p? LIMIT_QUAL : MAX_QUAL);

        if (strcmp(nextArg, "-top") == 0) glass.is_topmost = true;
        if (strcmp(nextArg, "-notop") == 0) glass.is_topmost = false;

        if (strcmp(nextArg, "-aspect") == 0) glass.is_ratiolocked = true;
        if (strcmp(nextArg, "-noaspect") == 0) glass.is_ratiolocked = false;

        if (strcmp(nextArg, "-repeat") == 0) { projector.state.repeat = true; }
        if (strcmp(nextArg, "-norepeat") == 0) { projector.state.repeat = false; }

        if (strcmp(nextArg, "-ghost") == 0) glass.is_clickthrough = true;
        if (strcmp(nextArg, "-noghost") == 0) glass.is_clickthrough = false;

        if (strcmp(nextArg, "-snap") == 0) glass.is_snappy = true;
        if (strcmp(nextArg, "-nosnap") == 0) glass.is_snappy = false;

        // if (strcmp(nextArg, "-wall") == 0) wallpaperMode = true;
        // if (strcmp(nextArg, "-nowall") == 0) wallpaperMode = false;

        if (strcmp(nextArg, "-fullscreen") == 0) glass.is_fullscreen = true;
        if (strcmp(nextArg, "-nofullscreen") == 0) glass.is_fullscreen = false;

        if (strcmp(nextArg, "-blinky") == 0) glass.set_icon(GetIconByInt(randomInt(4) + 4*2));
        if (strcmp(nextArg, "-pinky") == 0)  glass.set_icon(GetIconByInt(randomInt(4) + 4*1));
        if (strcmp(nextArg, "-inky") == 0)   glass.set_icon(GetIconByInt(randomInt(4) + 4*0));
        if (strcmp(nextArg, "-clyde") == 0)  glass.set_icon(GetIconByInt(randomInt(4) + 4*3));

        if (glass_string_starts_with(nextArg, "-opac"))
        {
            char *opacNum = nextArg + 5; // 5 = length of "-opac"
            glass.opacity = (double)atoi(opacNum) / 100.0;
        }

        if (glass_string_starts_with(nextArg, "-vol"))
        {
            char *volNum = nextArg + 4; // 4 = length of "-vol"
            projector.state.volume = (double)atoi(volNum) / 100.0;
        }

        if (glass_string_starts_with(nextArg, "-x"))
        {
            char *xNum = nextArg + 2; // 2 = length of "-x"
            startX = (double)atoi(xNum);
        }
        if (glass_string_starts_with(nextArg, "-y"))
        {
            char *yNum = nextArg + 2; // 2 = length of "-y"
            startY = (double)atoi(yNum);
        }
        if (glass_string_starts_with(nextArg, "-w"))
        {
            char *wNum = nextArg + 2; // 2 = length of "-w"
            startW = (double)atoi(wNum);
        }
        if (glass_string_starts_with(nextArg, "-h"))
        {
            char *hNum = nextArg + 2; // 2 = length of "-h"
            startH = (double)atoi(hNum);
        }

        if (strcmp(nextArg, "-tl")==0) nest = 1;
        if (strcmp(nextArg, "-tr")==0) nest = 2;
        if (strcmp(nextArg, "-bl")==0) nest = 3;
        if (strcmp(nextArg, "-br")==0) nest = 4;
        if (strcmp(nextArg, "-Bl")==0) nest = 5;  // todo: better name for above/below taskbar
        if (strcmp(nextArg, "-Br")==0) nest = 6;
    }

    glass_create_window(hInstance, startX, startY, startW, startH, nest);
    // glass_create_window(hInstance, 0,0,400,400);
    // glass_create_window_from_args_string(hInstance, argList, argCount);
    glass.render = render;
    // should be able to comment these out to get default glass behavior (not any more since parsing args ourselves)
    glass.on_click = on_click;
    glass.on_mdown = on_mdown;
    glass.on_mup = on_mup;
    glass.on_oops_that_was_a_double_click = restore_vid_position;
    glass.on_mouse_move = on_mouse_move;
    glass.on_mouse_drag = on_mouse_drag;
    glass.on_mouse_exit_window = on_mouse_exit_window;
    glass.on_dragdrop_file = on_dragdrop_file;
    glass.ghost_icon = global_icon_w;
    glass_custom_open_menu_at = OpenRClickMenuAt;
    if (randomVideo)
        queue_random_url();
    if (randomIcon)
        glass.set_icon(RandomIcon());

    origWndProc = SubclassWindow(glass.hwnd, appWndProc);




    // init other libs and things...
    dd_init(glass.hwnd, on_dragdrop); // could bundle dragdrop lib with glass i guess
    assert(d3d_load());
    assert(d3d_init(glass.hwnd, 400, 400));
    ttf_init();


    // allocate app stuff...
    debug_string = (char*)malloc(0x8000);
    create_quads();


    // start decoder loop...
    projector.StartBackgroundChurn();



    // window msg pump...
    glass_run_msg_render_loop();


    // cleanup....
    projector.KillBackgroundChurn();
    d3d_cleanup();
    OutputDebugString("Ending app process...\n");
    return 0;
}


void LogError(char *str) { SetSplash(str, 0xffff0000); }
void LogMessage(char *str) { OutputDebugString(str); }
