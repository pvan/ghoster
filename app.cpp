#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM
#include <stdio.h>
#include <math.h>

// for drag drop, sys tray icon, command line args
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")


#include "types.h"
// #include "directx.cpp"
// #include "text.cpp"

#include "glass/glass.cpp"


void LogError(char *str) { MessageBox(0,str,0,0); }
void LogMessage(char *str) { OutputDebugString(str); }

#include "movie/movie.h"


#include "urls.h"



HWND g_hwnd;
static MovieProjector projector;



void render()
{
    // source setup
    HDC hdc = GetDC(g_hwnd);
    RECT dispRect; GetClientRect(g_hwnd, &dispRect);
    int sw = dispRect.right - dispRect.left;
    int sh = dispRect.bottom - dispRect.top;

    // dest setup
    void *dst = projector.rolling_movie.reel.vid_buffer;
    int dw = 960;
    int dh = 720;

    // bmi setup
    BITMAPINFO bmi;
    bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
    bmi.bmiHeader.biWidth = dw;
    bmi.bmiHeader.biHeight = -dh;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    u32 red = 0xffff0000;
    if (dst == 0)
    {
        dst = (u8*)&red;
        dw = 1;
        dh = 1;
        bmi.bmiHeader.biWidth = dw;
        bmi.bmiHeader.biHeight = -dh;
    }

    StretchDIBits(hdc,0,0,sw,sh, 0,0,dw,dh,dst, &bmi,DIB_RGB_COLORS,SRCCOPY);
}


// char msg[1024];

// d3d_textured_quad screen;
// // d3d_textured_quad hud;
// // d3d_textured_quad text;


// void main_render_call()
// {
//     if (!glass.loop_running) return;  // kinda smells

//     RECT winRect; GetWindowRect(glass.hwnd, &winRect);
//     int sw = winRect.right-winRect.left;
//     int sh = winRect.bottom-winRect.top;


//     // d3d_set_hwnd_target(glass.target_window());
//     d3d_resize_if_change(sw, sh, glass.target_window());



//     if (msg && *msg)
//     {
//         text.destroy();
//         text = ttf_create(msg, 64, 255, true, true);
//     }

//     text.move_to_pixel_coords_center(sw/2, sh/2, sw, sh);

//     // hud.update_with_pixel_coords(10, sh-10-200, 200, 200, sw, sh);

//     screen.render();
//     // hud.render();
//     text.render();
//     d3d_swap();
// }

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    timeBeginPeriod(1); // set resolution of sleep


    glass_create_window(hInstance, 0,0,400,400);
    glass.render = render;
    g_hwnd = glass.hwnd;


    // assert(d3d_load());
    // assert(d3d_init(glass.hwnd, 400, 400));

    // ttf_init();




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

    // todo: have projector be running in background, so load is really "queue load" etc
    // // projector.start();
    // projector.load_from_url("D:\\Users\\phil\\Desktop\\test4.mp4");




    glass_run_msg_render_loop();


    // d3d_cleanup();

    projector.KillBackgroundChurn();

    OutputDebugString("Ending app process...\n");
    return 0;
}
