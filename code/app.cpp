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

// kinda hacky... todo: way to drop this? or maybe just allow this override of glass
// what about making progress bar into a child window?
bool screenPointIsOnProgressBar(HWND hwnd, int x, int y);
void SetSplash(char *msg, u32 col = 0xffffffff);

#include "movie/movie.cpp"
#include "glass/glass.cpp"
#include "urls.h"
#include "icons.h"
#include "dragdrop.h"


// progress bar position
const int PROGRESS_BAR_H = 22;

// hide progress bar after this many seconds
const double PROGRESS_BAR_TIMEOUT = 1.0;

// how long our splash messages last (fade included)
const double SEC_OF_SPLASH_MSG = 2;

const bool D3D_DEBUG_MSG = false;


static MovieProjector projector;


bool is_letterbox = false;

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
    // create d3d quads
    u32 gray = 0xaaaaaaaa;
    u32 red = 0xffff0000;
    progress_bar_gray.create((u8*)&gray,1,1, -1,-1,1,1,0);
    progress_bar_red.create((u8*)&red,1,1, -1,-1,1,1,0);

    // create screen quads
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
    int r = getUnplayedIndex();
    projector.QueueLoadFromPath(RANDOM_ON_LAUNCH[r]);
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

    // these should all be placed as indicated
    // progress_bar_quad.set_to_pixel_coords_BL(50, 30, sw-50, sh-30);
    // progress_bar_quad.set_to_pixel_coords_TL(50, 30, sw-50, sh-30);
    // screen.move_to_pixel_coords_BL(50,30);  // note quad is source size here
    // screen.move_to_pixel_coords_TL(50,30);
    // screen.move_to_pixel_coords_center(50,30);
    // screen.move_to_pixel_coords_center(sw/2,sh/2);
    // screen.move_to_pixel_coords_center(sw/2,0);

    // text = ttf_create(msg, 64, 255);
    // text.move_to_pixel_coords_center(sw/2, sh/2, sw, sh);
    // hud.update_with_pixel_coords(10, sh-10-200, 200, 200, sw, sh);

    // todo: seems we have still not completely eliminated flicker on resize.. hrmm
    d3d_clear(0, 0, 255);
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

void on_click(int x, int y) {
    // PRINT("%i, %i\n", x, y);
    if (clientPointIsOnProgressBar(x, y)) {
        // doing this on mdown and mdrag now
    } else {
        if (projector.state.is_paused) { SetSplash("Play", 0xaaaaaaaa/*0x7cec7aff*/); projector.PlayMovie(); }
        else { SetSplash("Pause", 0xaaaaaaaa/*0xfa8686ff*/); projector.PauseMovie(); }
        // projector.TogglePause();
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

    // may need to recreate wallpaper window here?

    // something like this? (na, would be too long on urls)
    if (first_video)
    {
        first_video = false;
        // glass.ShowWindow();
    }
}



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
    char *url = projector.rolling_movie.reel.path;

    char output[FFMEPG_PATH_SIZE]; // todo: stack alloc ok here?
    if (StringIsUrl(url) && withTimestamp) {
        int secondsElapsed = projector.rolling_movie.seconds_elapsed_at_last_decode;
        sprintf(output, "%s&t=%i", url, secondsElapsed);
    } else { sprintf(output, "%s", url); }

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
            if (wParam == 0x56 &&  // V
                GetKeyState(VK_CONTROL) & 0x8000)  // ctrl
            {
                paste_clipboard();
            }
        } break;

        case WM_KEYUP: {
            if (wParam >= 0x30 && wParam <= 0x39) // 0-9
            {
                projector.QueueLoadFromPath(TEST_FILES[wParam - 0x30]);
            }
            if (wParam == VK_OEM_3) // ~
            {
                queue_random_url();
            }
            if (wParam == VK_TAB)
            {
                show_debug = !show_debug;
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


    glass_create_window(hInstance, 0,0,400,400);
    glass.render = render;
    // should be able to comment these out to get default glass behavior
    glass.on_click = on_click;
    glass.on_mdown = on_mdown;
    glass.on_mup = on_mup;
    glass.on_oops_that_was_a_double_click = restore_vid_position;
    glass.on_mouse_move = on_mouse_move;
    glass.on_mouse_drag = on_mouse_drag;
    glass.on_mouse_exit_window = on_mouse_exit_window;
    glass.on_dragdrop_file = on_dragdrop_file;
    glass.set_icon(RandomIcon());
    glass.ghost_icon = global_icon_w;
    glass_custom_open_menu_at = OpenRClickMenuAt;

    origWndProc = SubclassWindow(glass.hwnd, appWndProc);


    // could bundle dragdrop lib with glass i guess
    dd_init(glass.hwnd, on_dragdrop);



    assert(d3d_load());
    assert(d3d_init(glass.hwnd, 400, 400));

    ttf_init();

    debug_string = (char*)malloc(0x8000);


    // should we have each module parse this themselves?

    wchar_t **argList;
    int argCount;
    argList = CommandLineToArgvW(GetCommandLineW(), &argCount);
    if (argList == 0)
    {
        // MsgBox("CommandLineToArgvW failed.");
        MessageBox(0,"CommandLineToArgvW fail!",0,0);  // here even? or not
    }

    // pass exe directory to ghoster so it knows where to find youtube-dl
    // can it find its directory itself?
    char *exe_directory = (char*)malloc(MAX_PATH); // todo: what to use?
    wcstombs(exe_directory, argList[0], MAX_PATH);
    DirectoryFromPath(exe_directory);

    projector.Init(exe_directory);
    projector.on_load_callback = on_video_load;

    projector.StartBackgroundChurn();
    queue_random_url();


    create_quads();


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
