#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM
#include <stdio.h>
#include <math.h>
#include <assert.h>


// for drag drop, sys tray icon, command line args
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")


#include <Wingdi.h> // StretchDIBits
#pragma comment(lib, "Gdi32.lib")


#include "types.h"

void LogError(char *str) { MessageBox(0,str,0,0); }
void LogMessage(char *str) { OutputDebugString(str); }


bool running = true;
HWND g_hwnd;


#include "ghoster.cpp"


void render()
{
    // source setup
    HDC hdc = GetDC(g_hwnd);
    RECT dispRect; GetClientRect(g_hwnd, &dispRect);
    int sw = dispRect.right - dispRect.left;
    int sh = dispRect.bottom - dispRect.top;

    // // dest setup
    // void *dst = global_ghoster.rolling_movie.vid_buffer;

    // // bmi setup
    // BITMAPINFO bmi;
    // bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
    // bmi.bmiHeader.biWidth = 960;
    // bmi.bmiHeader.biHeight = -720;
    // bmi.bmiHeader.biPlanes = 1;
    // bmi.bmiHeader.biBitCount = 32;
    // bmi.bmiHeader.biCompression = BI_RGB;

    // dest setup
    u32 red = 0xffff0000;
    void *dst = (void*)&red;

    // bmi setup
    BITMAPINFO bmi;
    bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
    bmi.bmiHeader.biWidth = 1;
    bmi.bmiHeader.biHeight = 1;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;



    StretchDIBits(hdc,0,0,sw,sh, 0,0,1,1,dst, &bmi,DIB_RGB_COLORS,SRCCOPY);

}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_CLOSE) running = false;
    if (uMsg ==  WM_NCHITTEST) {
        RECT win; if (!GetWindowRect(hwnd, &win)) return HTNOWHERE;
        POINT pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT pad = { GetSystemMetrics(SM_CXFRAME), GetSystemMetrics(SM_CYFRAME) };
        bool left   = pos.x < win.left   + pad.x;
        bool right  = pos.x > win.right  - pad.x -1;  // win.right 1 pixel beyond window, right?
        bool top    = pos.y < win.top    + pad.y;
        bool bottom = pos.y > win.bottom - pad.y -1;
        if (top && left)     return HTTOPLEFT;
        if (top && right)    return HTTOPRIGHT;
        if (bottom && left)  return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left)            return HTLEFT;
        if (right)           return HTRIGHT;
        if (top)             return HTTOP;
        if (bottom)          return HTBOTTOM;
        return HTCAPTION;
    }
    if (uMsg ==  WM_SIZE) {
        // if (device) device->Present(0, 0, 0, 0);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    timeBeginPeriod(1); // set resolution of sleep

    WNDCLASS wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "app";
    if (!RegisterClass(&wc)) { MessageBox(0, "RegisterClass failed", 0, 0); return 1; }

    HWND hwnd = CreateWindowEx(
        0, "app", "title",
        // WS_POPUP | WS_VISIBLE,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 400, 0, 0, hInstance, 0);
    if (!hwnd) { MessageBox(0, "CreateWindowEx failed", 0, 0); return 1; }
    g_hwnd = hwnd;


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

    global_ghoster.Init(exe_directory);

    // MAIN APP LOOP
    CreateThread(0, 0, RunMainLoop, 0, 0, 0);

    // todo: have projector be running in background, so load is really "queue load" etc
    // // projector.start();
    // projector.load_from_url("D:\\Users\\phil\\Desktop\\test4.mp4");



    while(running)
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        render();
        Sleep(16);
    }




    return 0;
}








