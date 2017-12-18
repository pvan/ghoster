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

#include "glass/glass.cpp"
#include "movie/movie.cpp"
#include "urls.h"



static MovieProjector projector;




// char msg[1024*4];
char *debug_string;

d3d_textured_quad screen;
// d3d_textured_quad hud;
d3d_textured_quad debug_quad;


bool keyD1;
bool keyD2;
void render()
{
    if (!glass.loop_running) return;  // kinda smells

    if (GetKeyState(0x31) & 0x8000 && !keyD1) { keyD1=true; projector.QueueLoadFromPath(TEST_FILES[2]); }
    if (GetKeyState(0x32) & 0x8000 && !keyD2) { keyD2=true; projector.QueueLoadFromPath(TEST_FILES[7]); }
    if (!(GetKeyState(0x31) & 0x8000)) keyD1 = false;
    if (!(GetKeyState(0x32) & 0x8000)) keyD2 = false;
    // if (GetKeyState(0x31) & 0x8000) { projector.QueueLoadFromPath(TEST_FILES[2]); }
    // if (GetKeyState(0x32) & 0x8000) { projector.QueueLoadFromPath(TEST_FILES[7]); }

    RECT winRect; GetWindowRect(glass.hwnd, &winRect);
    int sw = winRect.right-winRect.left;
    int sh = winRect.bottom-winRect.top;


    d3d_resize_if_change(sw, sh, glass.target_window());


    if (projector.front_buffer && projector.front_buffer->mem)
    {
        u8 *src = projector.front_buffer->mem;
        int w = projector.front_buffer->wid;
        int h = projector.front_buffer->hei;
        screen.fill_tex_with_mem(src, w, h);
    }



    projector.rolling_movie.reel.MetadataToString(debug_string);
    sprintf(debug_string+strlen(debug_string), "\n");
    glass.ToString(debug_string+strlen(debug_string));
    if (debug_string && *debug_string)
    {
        debug_quad.destroy();
        debug_quad = ttf_create(debug_string, 26, 255);
    }
    debug_quad.move_to_pixel_coords_TL(0, 0, sw, sh);


    // text = ttf_create(msg, 64, 255);
    // text.move_to_pixel_coords_center(sw/2, sh/2, sw, sh);
    // hud.update_with_pixel_coords(10, sh-10-200, 200, 200, sw, sh);

    // todo: seems we have still not completely eliminated flicker on resize.. hrmm
    screen.render();
    // hud.render();
    debug_quad.render();
    d3d_swap();
}

void on_click()
{
    projector.TogglePause();
}

// other ways?
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
    glass.on_single_lclick = on_click;


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

    projector.StartBackgroundChurn();
    projector.on_load_callback = on_video_load;


    // window msg pump...
    screen.create(projector.rolling_movie.reel.vid_buffer,960,720, -1,-1,1,1,0.5); //todo:update to new api
    glass_run_msg_render_loop();


    // cleanup....
    projector.KillBackgroundChurn();
    d3d_cleanup();
    OutputDebugString("Ending app process...\n");
    return 0;
}
