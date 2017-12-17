

#include "resource.h"



// HICON MakeIconFromBitmap(HINSTANCE hInstance, HBITMAP hbm)
// {
//     ICONINFO info = {true, 0, 0, hbm, hbm};
//     return CreateIconIndirect(&info);
// }


HICON icon_load(HINSTANCE hInstance, UINT icon_id)
{
    return (HICON)LoadImage(hInstance, MAKEINTRESOURCE(icon_id), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
}



#define ID_SYSTRAY 999
#define ID_SYSTRAY_MSG WM_USER + 1


// todo: global variable instead?
NOTIFYICONDATA icon_build_info(HWND hwnd, HICON icon, char *text)
{
    NOTIFYICONDATA info =
    {
        sizeof(NOTIFYICONDATA),
        hwnd,
        ID_SYSTRAY,               //UINT  uID
        NIF_ICON | NIF_MESSAGE | NIF_TIP,
        ID_SYSTRAY_MSG,       //UINT  uCallbackMessage
        icon,                    //HICON hIcon
        0,                    //TCHAR szTip[64]
        0,                    //DWORD dwState
        0,                    //DWORD dwStateMask
        0,                    //TCHAR szInfo[256]
        0,                    //UINT uVersion
        0,                    //TCHAR szInfoTitle[64]
        0,                    //DWORD dwInfoFlags
        0,                    //GUID  guidItem
        0                     //HICON hBalloonIcon
    };
    if (text)
    {
        assert(strlen(text) < 256);
        strcpy_s(info.szTip, 256, text); // todo: check length
    }
    return info;
}

// void icon_set_title(HWND hwnd, char *title)
// {
//     // system tray hover
//     NOTIFYICONDATA info = icon_default_info(hwnd);
//     assert(strlen(title) < 256);
//     strcpy_s(info.szTip, 256, title); // todo: check length
//     Shell_NotifyIcon(NIM_MODIFY, &info);

//     // window titlebar (taskbar)
//     SetWindowText(hwnd, title);
// }

bool icon_systray_added = false;

void icon_update_systray(HWND hwnd, HICON icon, TCHAR *text)
{
    NOTIFYICONDATA info = icon_build_info(hwnd, icon, text);
    Shell_NotifyIcon(NIM_MODIFY, &info);
}

void icon_add_systray(HWND hwnd, HICON icon, TCHAR *text)
{
    NOTIFYICONDATA info = icon_build_info(hwnd, icon, text);
    Shell_NotifyIcon(NIM_ADD, &info);
    icon_systray_added = true;
}

void icon_remove_systray(HWND hwnd)
{
    NOTIFYICONDATA info = icon_build_info(hwnd,0,0);
    Shell_NotifyIcon(NIM_DELETE, &info);
    icon_systray_added = false;
}

void icon_set(HWND hwnd, HICON icon)
{
    if (icon_systray_added)
        icon_update_systray(hwnd, icon, 0);

    // window icon (taskbar if no title)
    SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);

    // // only needed if we're using the new wallpaper window icon in the taskbar
    // // set wallpaper window same for now
    // if (hwnd != global_wallpaper_window) SetIcon(global_wallpaper_window, icon);
}
