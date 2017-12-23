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
// maybe make progress bar into a child window?
bool screenPointIsOnProgressBar(HWND hwnd, int x, int y);

#include "movie/movie.cpp"
#include "glass/glass.cpp"
#include "urls.h"


// progress bar position
const int PROGRESS_BAR_H = 22;
// const int PROGRESS_BAR_B = 0;  // just hardcode bottom for now

// hide progress bar after this many seconds
const double PROGRESS_BAR_TIMEOUT = 1.0;


static MovieProjector projector;




// char msg[1024*4];
char *debug_string;
bool show_debug = false;

bool show_bar = false;
double secOfLastMouseMove = -1;

d3d_textured_quad screen;
// d3d_textured_quad hud;
d3d_textured_quad debug_quad;
d3d_textured_quad progress_bar_gray;
d3d_textured_quad progress_bar_red;

void queue_random_url()
{
    int r = getUnplayedIndex();
    projector.QueueLoadFromPath(RANDOM_ON_LAUNCH[r]);
}


bool keyD1;
bool keyD2;
bool keyD3;
bool keyD4;
void render()  // os msg pump thread
{
    if (!glass.loop_running) return;  // kinda smells

    if (GetKeyState(0x31) & 0x8000 && !keyD1) { keyD1=true; projector.QueueLoadFromPath(TEST_FILES[2]); }
    if (GetKeyState(0x32) & 0x8000 && !keyD2) { keyD2=true; projector.QueueLoadFromPath(TEST_FILES[7]); }
    if (GetKeyState(VK_TAB) & 0x8000 && !keyD3) { keyD3=true; show_debug=!show_debug; }
    if (GetKeyState(VK_OEM_3) & 0x8000 && !keyD4) { keyD4=true; queue_random_url(); }
    if (!(GetKeyState(0x31) & 0x8000)) keyD1 = false;
    if (!(GetKeyState(0x32) & 0x8000)) keyD2 = false;
    if (!(GetKeyState(VK_TAB) & 0x8000)) keyD3 = false;
    if (!(GetKeyState(VK_OEM_3) & 0x8000)) keyD4 = false;

    if (GetWallClockSeconds() - secOfLastMouseMove > PROGRESS_BAR_TIMEOUT)
        show_bar = false;
    if (!glass.is_mouse_in_window())
        show_bar = false;
    if (!projector.IsMovieLoaded())
        show_bar = false;

    RECT winRect; GetWindowRect(glass.hwnd, &winRect);
    int sw = winRect.right-winRect.left;
    int sh = winRect.bottom-winRect.top;


    d3d_resize_if_change(sw, sh, glass.target_window());


    if (projector.front_buffer && projector.front_buffer->mem)
    {
        u8 *src = projector.front_buffer->mem;
        int w = projector.front_buffer->wid;
        int h = projector.front_buffer->hei;
        screen.fill_tex_with_mem(src, w, h);  //this resizes if needed, todo: better name

        if (glass.is_fullscreen && glass.is_ratiolocked) {
            // todo: need to stretch to screen actually
            screen.move_to_pixel_coords_center(sw/2,sh/2);  // keep texture size
        } else {
            screen.fill_vb_with_rect(-1,-1,1,1,0);  // fill screen
        }
    }



    if (show_debug)
    {
        projector.rolling_movie.reel.MetadataToString(debug_string);
        sprintf(debug_string+strlen(debug_string), "\n");
        glass.ToString(debug_string+strlen(debug_string));
        if (debug_string && *debug_string)
        {
            debug_quad.destroy();
            debug_quad = ttf_create(debug_string, 26, 255, 125);
        }
        debug_quad.move_to_pixel_coords_TL(0, 0);
    }

    if (show_bar)
    {
        int progress_pixel = projector.rolling_movie.percent_elapsed() * (double)sw;
        progress_bar_gray.set_to_pixel_coords_BL(progress_pixel, 0, sw-progress_pixel, PROGRESS_BAR_H);
        progress_bar_red.set_to_pixel_coords_BL(0, 0, progress_pixel, PROGRESS_BAR_H);
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
void restore_vid_position() { projector.RestoreVideoPosition(); }
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
        projector.TogglePause();
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



// other ways? could poll IsMovieLoaded each loop?
bool first_video = true;
void on_video_load(int w, int h)
{
    glass.aspect_ratio = (double)w / (double)h;

    if (!glass.is_fullscreen) // not in this case though
        glass.set_ratiolocked(glass.is_ratiolocked); // awkward way to set window size to aspect ratio if enabled

    // something like this? (na, would be too long on urls)
    if (first_video)
    {
        first_video = false;
        // glass.ShowWindow();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    timeBeginPeriod(1); // set resolution of sleep


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


    // create d3d quads
    u32 gray = 0xaaaaaaaa;
    u32 red = 0xffff0000;
    progress_bar_gray.create((u8*)&gray,1,1, -1,-1,1,1,0);
    progress_bar_red.create((u8*)&red,1,1, -1,-1,1,1,0);

    // window msg pump...
    u32 green = 0xff00ff00;
    screen.create((u8*)&green,1,1, -1,-1,1,1,0); //todo:update to new api
    // screen.create(projector.rolling_movie.reel.vid_buffer,960,720, -1,-1,1,1,0); //todo:update to new api
    glass_run_msg_render_loop();


    // cleanup....
    projector.KillBackgroundChurn();
    d3d_cleanup();
    OutputDebugString("Ending app process...\n");
    return 0;
}
