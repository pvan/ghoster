// ideally a little stand-alone lib for creating a window that's
// borderless, draggable, resizable, snappable, transparent

#include <windows.h>
#include <Windowsx.h> // GET_X_LPARAM
#include <stdio.h>
#include <assert.h>

// for drag drop, sys tray icon
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

// for set pixel format on wallpaper window.. maybe only needed if using gdi to render??
// #include <Wingdi.h>
// #pragma comment(lib, "Gdi32.lib")


#include "icon/icon.h"

#include "hwnd.h"



// basically just for parsing command line args.. if we drop that we can drop this
bool glass_string_starts_with(const char *str, const char *front) // case sensitive
{
    while (*front && *str) { if (*front++ != *str++) return false; }
    return true;
}



const bool DEBUG_MCLICK_MSGS = false;


// disallow opacity greater than this when in ghost mode
const double GHOST_MODE_MAX_OPACITY = 0.95;

// feels like i want this less often
const bool GHOST_MODE_SETS_TOPMOST = false;

// how long to wait before toggling pause when single click (which could be start of double click)
// higher makes double click feel better (no audio stutter on fullscreen for slow double clicks)
// lower makes single click feel better (less delay when clicking to pause/unpause)
const double MS_PAUSE_DELAY_FOR_DOUBLECLICK = 200;  // slowest double click is 500ms by default

// snap to screen edge if this close
const int SNAP_IF_PIXELS_THIS_CLOSE = 25;





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
void SnapWinRectToMonitor(HWND hwnd, RECT in, RECT *out)
{
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
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



// find the workerW for wallpaper mode
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

VOID CALLBACK SingleLClickCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);


struct glass_window
{
    // int w;
    // int h;

    HWND hwnd;
    HINSTANCE hInstance;
    HICON icon = 0;
    HICON ghost_icon = 0;



    bool loop_running = true;
    int sleep_ms = 16; // todo: make more accurate loop timing

    UINT render_timer_id;

    void (*render)() = 0;  // application-defined render function
    void (*on_click)(int,int) = 0;
    void (*on_mdown)(int,int) = 0;
    void (*on_mup)() = 0;
    void (*on_oops_that_was_a_double_click)() = 0;
    void (*on_mouse_move)(int,int) = 0;
    void (*on_mouse_drag)(int,int) = 0;
    void (*on_mouse_exit_window)() = 0;
    void (*on_dragdrop_file)(char*) = 0;
    // not sure if i'm crazy about all these, but maybe they're a decent way to do it?


    // settings / state
    bool is_fullscreen = false;
    bool is_topmost = false;
    bool is_clickthrough = false;
    bool is_ratiolocked = true;
    bool is_snappy = true;
    bool is_wallpaper = false;
    double aspect_ratio = 1;
    double opacity = 1;


    // workaround for changing size while dragging window
    bool cached_size_up_to_date = false; // basically only false on startup
    int cached_w;
    int cached_h;


    // for ghost mode
    bool had_to_cache_opacity;
    double last_opacity;


    // for wallpaper mode
    HWND workerw;
    HWND wallpaper_window;


    bool menu_open = false;


    // mouse state
    POINT mDownPoint;
    bool mDown;
    // bool clickingOnProgressBar = false;
    bool mouseHasMovedSinceDownL = false;
    // double msOfLastMouseMove = -1000;
    bool next_mup_was_double_click = false;
    bool next_mup_was_closing_menu = false;
    UINT single_click_timer_id;
    POINT mup_timer_point;
    bool registeredLastSingleClick = false;


    RECT get_win_rect() { RECT winRect; GetWindowRect(hwnd,&winRect); return winRect; }
    int get_win_width() { RECT wr = get_win_rect(); return wr.right-wr.left; }
    int get_win_height() { RECT wr = get_win_rect(); return wr.bottom-wr.top; }

    bool screen_point_in_window(int sx, int sy) { POINT p = {sx, sy}; return PtInRect(&get_win_rect(), p); }
    bool mouse_in_window() { POINT p; GetCursorPos(&p); return screen_point_in_window(p.x,p.y); }

    bool get_mdown() { return (GetKeyState(VK_LBUTTON) & 0x100) != 0; }


    void ToString(char *out)
    {
        RECT wr = get_win_rect();
        sprintf(out,
        "loop_running: %s\n"
        "sleep_ms: %i\n"
        "win_rect: %i, %i, %i, %i\n"
        "mouse_in_window: %s\n"
        "is_fullscreen: %s\n"
        "is_topmost: %s\n"
        "is_clickthrough: %s\n"
        "is_ratiolocked: %s\n"
        "is_snappy: %s\n"
        "is_wallpaper: %s\n"
        "aspect_ratio: %f\n"
        "opacity: %f\n"
        "had_to_cache_opacity: %s\n"
        "last_opacity: %f\n"
        "menu_open: %s\n"
        "mDownPoint: %i, %i\n"
        "mDown: %s\n"
        "mouseHasMovedSinceDownL: %s\n"
        // "msOfLastMouseMove: %f\n"
        "next_mup_was_double_click: %s\n"
        "next_mup_was_closing_menu: %s\n"
        "registeredLastSingleClick: %s\n",
        loop_running ? "true" : "false",
        sleep_ms,
        wr.left, wr.top, get_win_width(), get_win_height(),
        mouse_in_window() ? "true" : "false",
        is_fullscreen ? "true" : "false",
        is_topmost ? "true" : "false",
        is_clickthrough ? "true" : "false",
        is_ratiolocked ? "true" : "false",
        is_snappy ? "true" : "false",
        is_wallpaper ? "true" : "false",
        aspect_ratio,
        opacity,
        had_to_cache_opacity ? "true" : "false",
        last_opacity,
        menu_open ? "true" : "false",
        mDownPoint.x, mDownPoint.y,
        mDown ? "true" : "false",
        mouseHasMovedSinceDownL ? "true" : "false",
        // msOfLastMouseMove,
        next_mup_was_double_click ? "true" : "false",
        next_mup_was_closing_menu ? "true" : "false",
        registeredLastSingleClick ? "true" : "false");
    }


    // todo: where do we need to replace hwnd usage with this? if anywhere
    HWND target_window() { return is_wallpaper ? wallpaper_window : hwnd; }


    void main_update()
    {
        // if we mouse up while not on window all our mdown etc flags will be wrong
        // so we just force an "end of click" when we leave the window
        bool mouse_in_win = mouse_in_window();
        static bool mouse_was_in_win = false;
        if (!mouse_in_win)
        {
            mDown = false;
            mouseHasMovedSinceDownL = false;
            if (mouse_was_in_win)
            {
                if (on_mouse_exit_window) on_mouse_exit_window();
            }
        }
        mouse_was_in_win = mouse_in_win;

        if (render) render();
    }



    WINDOWPLACEMENT last_win_pos;
    void setFullscreen(bool enable)
    {
        is_fullscreen = enable;
        if (enable)
        {
            last_win_pos.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(hwnd, &last_win_pos);

            // todo: BUG: transparency is lost when we full screen
            // ShowWindow(hwnd, SW_MAXIMIZE); // or SW_SHOWMAXIMIZED?
            // for now just change our window size to the monitor
            // but leave 1 pixel along the bottom because this method causes the same bug as SW_MAXIMIZE
            // todo: consider different edge than bottom so we can click there?

            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
            {
                SetWindowPos(hwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top -1,   // todo: note this workaround for bug explained above
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

                // move to top so we're above taskbar
                // todo: only works if we set as topmost.. setting it temporarily for now
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
        }
        else
        {
            if (last_win_pos.length)
            {
                // restore our old position todo: replace if we get SW_MAXIMIZE / SW_RESTORE working
                SetWindowPos(hwnd, 0,
                    last_win_pos.rcNormalPosition.left,
                    last_win_pos.rcNormalPosition.top,
                    last_win_pos.rcNormalPosition.right -
                    last_win_pos.rcNormalPosition.left,
                    last_win_pos.rcNormalPosition.bottom -
                    last_win_pos.rcNormalPosition.top, 0);
            }

            // unset our temp topmost from fullscreening if we aren't actually set that way
            if (!is_topmost) SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }

        // need to recreate wallpaper window if mode is set
        if (is_wallpaper)
        {
            // just do this for now
            set_wallpaper(false);
            set_wallpaper(true);
        }
    }


    void set_title(char *title)  // sets hwnd title and sys tray hover text
    {
        icon_update_systray(hwnd, icon, title);
        SetWindowText(hwnd, title);
    }

    void set_icon(HICON new_icon)
    {
        icon = new_icon;
        if (!is_clickthrough) icon_set(hwnd, icon);
    }

    void set_topmost(bool enable)
    {
        is_topmost = enable;
        hwnd_set_topmost(hwnd, enable);
    }
    void set_clickthrough(bool enable)
    {
        is_clickthrough = enable;
        hwnd_set_clickthrough(hwnd, enable);
    }
    void set_layered(bool enable)  // needed for opacity
    {
        hwnd_set_layered(hwnd, enable);
    }
    void set_opacity(double opac)   // 0-1
    {
        if (is_clickthrough && opac > GHOST_MODE_MAX_OPACITY)
            opac = GHOST_MODE_MAX_OPACITY;
        opacity = hwnd_set_opacity(hwnd, opac);
    }

    // might save/restore opacity / topmost depending on settings
    void set_ghostmode(bool enable)
    {
        set_clickthrough(enable);
        if (enable)
        {
            // SetIcon(this_icon);
            if (ghost_icon) icon_set(hwnd, ghost_icon);
            if (GHOST_MODE_SETS_TOPMOST) set_topmost(true);
            if (opacity > GHOST_MODE_MAX_OPACITY)
            {
                last_opacity = opacity;
                had_to_cache_opacity = true;
                opacity = GHOST_MODE_MAX_OPACITY;
                set_opacity(opacity);
            }
            else
            {
                had_to_cache_opacity = false;
            }
        }
        else
        {
            // SetIcon(hwnd, this_icon);
            if (ghost_icon) icon_set(hwnd, icon);
            if (had_to_cache_opacity && opacity != last_opacity)
            {
                opacity = last_opacity;
                set_opacity(opacity);
            }
        }
    }
    void set_snappy(bool enable)
    {
        is_snappy = enable;
        if (is_snappy)
        {
            RECT winRect; GetWindowRect(hwnd, &winRect);

            SnapWinRectToMonitor(hwnd, winRect, &winRect);

            SetWindowPos(hwnd, 0,
                winRect.left, winRect.top,
                winRect.right  - winRect.left,
                winRect.bottom - winRect.top,
                0);
        }
    }
    void set_ratiolocked(bool enable)
    {
        is_ratiolocked = enable;
        if (is_ratiolocked)
        {
            hwnd_set_to_aspect_ratio(hwnd, aspect_ratio);
            // need to set wallpaper hwnd too? todo: other cases similar?
        }
    }

    bool registered_wallpaper_class_once = false;
    void set_wallpaper(bool enable)
    {
        // cleanup first if we are already in wallpaper mode and trying to enable
        if (is_wallpaper && enable)
        {
            if (wallpaper_window) DestroyWindow(wallpaper_window);
        }

        is_wallpaper = enable;
        if (enable)
        {
            set_topmost(false); // todo: test if needed

            // wallpaper mode method via
            // https://www.codeproject.com/Articles/856020/Draw-Behind-Desktop-Icons-in-Windows

            HWND progman = FindWindow("Progman", "Program Manager");

            // Send 0x052C to Progman. This message directs Progman to spawn a
            // WorkerW behind the desktop icons. If it is already there, nothing happens.
            u64 *result = 0;
            SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000 /*timeout in ms*/, result);

            EnumWindows(EnumWindowsProc, 0);

            if (!workerw) { MessageBox(0,"wallpaper failed!",0,0); }

            // HWND newParent = workerw;  // may have to use this in 8+
            HWND newParent = progman;

            // trick is to hide this or it could intermittently hide our own window
            ShowWindow(workerw, SW_HIDE);

            RECT win; GetWindowRect(hwnd, &win);

            if (!registered_wallpaper_class_once)
            {
                // register class for wallpaper window (could do this once at startup)
                WNDCLASS wc = {};
                wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
                wc.lpfnWndProc =  DefWindowProc;//WndProc;
                wc.hInstance = hInstance;
                wc.hCursor = LoadCursor(NULL, IDC_ARROW);
                wc.lpszClassName = "glass wallpaper";
                if (!RegisterClass(&wc)) { MessageBox(0,"RegisterClass for wallpaper window failed.",0,0); }
                registered_wallpaper_class_once = true;
            }

            wallpaper_window = CreateWindowEx(
                0,
                // WS_EX_TOOLWINDOW, // don't show taskbar
                "glass wallpaper", "glass wallpaper",
                WS_POPUP | WS_VISIBLE,
                win.left, win.top,
                win.right - win.left,
                win.bottom - win.top,
                0, 0, hInstance, 0);
            if (!wallpaper_window) { MessageBox(0,"wallpaper mode failed!",0,0); }

            // // set dc format to be same as our main dc
            // // (i'm not sure this is needed??) maybe only if using gdi to render?
            // PIXELFORMATDESCRIPTOR pf = {};
            // pf.nSize = sizeof(pf);
            // pf.nVersion = 1;
            // pf.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
            // pf.iPixelType = PFD_TYPE_RGBA;
            // pf.cColorBits = 32;
            // pf.cAlphaBits = 8;
            // HDC hdc = GetDC(wallpaper_window);
            // int format_index = ChoosePixelFormat(hdc, &pf);
            // SetPixelFormat(hdc, format_index, &pf);
            // ReleaseDC(wallpaper_window, hdc);

            // only need this if we're using the new wallpaper window icon in the taskbar
            icon_set(wallpaper_window, icon);

            // only seems to work when setting this after window has been created
            SetParent(wallpaper_window, newParent);

            ShowWindow(hwnd, SW_HIDE);
        }
        else
        {
            if (wallpaper_window)
                DestroyWindow(wallpaper_window);

            ShowWindow(hwnd, SW_SHOW);
        }
    }



    bool is_mouse_in_window()
    {
        // todo: special handling of wallpaper needed here?
        // todo: special handling req anywhere else?
        POINT mPos;   GetCursorPos(&mPos);
        RECT winRect; GetWindowRect(hwnd, &winRect);
        return PtInRect(&winRect, mPos);
    }




    void DragWindow(int x, int y)
    {
        if (DEBUG_MCLICK_MSGS) OutputDebugString("DRAG\n");

        if (is_fullscreen)
        {
            setFullscreen(false);  // this restores window size, sets not topmost (if needed) etc

            RECT winRect; GetWindowRect(hwnd, &winRect);
            int width = winRect.right-winRect.left;
            int height = winRect.bottom-winRect.top;

            // move window to mouse..
            int mouseX = x;
            int mouseY = y;
            int winX = mouseX - width/2;
            int winY = mouseY - height/2;
            MoveWindow(hwnd, winX, winY, width, height, true);
        }

        // basically mups after drags aren't treated as regular mups
        mDown = false; // kind of out-of-place but mouseup() is not getting called after drags
        next_mup_was_closing_menu = false; // also needs to be reset here
        // todo: any other mup flags need to be reset here?

        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }



    void onMouseMove(HWND hwnd, int cx, int cy)
    {
        // if (DEBUG_MCLICK_MSGS) OutputDebugString("MMOVE\n");

        // // this is for progress bar timeout.. rename/move?
        // msOfLastMouseMove = app_timer.MsSinceStart();
        if (on_mouse_move) on_mouse_move(cx, cy);

        // if (get_mdown())
        if (mDown)
        {
            // need to determine if click or drag here, not in buttonup
            // because mousemove will trigger (i think) at the first pixel of movement
            POINT mPos = { cx, cy };
            double dx = (double)mPos.x - (double)mDownPoint.x;
            double dy = (double)mPos.y - (double)mDownPoint.y;
            double distance = sqrt(dx*dx + dy*dy);
            double MOVEMENT_ALLOWED_IN_CLICK = 2.5;
            if (DEBUG_MCLICK_MSGS) PRINT("moved %f\n", distance);
            if (distance <= MOVEMENT_ALLOWED_IN_CLICK)
            {
                // we haven't moved enough to be considered a drag
                // or to eliminate a double click possibility (edit: although system handles that now)
            }
            else
            {
                mouseHasMovedSinceDownL = true;

                if (on_mouse_drag) on_mouse_drag(cx, cy);
                else DragWindow(cx, cy); // default to a drag
            }
        }
    }

    void onMouseUpL(int cx, int cy)
    {
        if (DEBUG_MCLICK_MSGS) OutputDebugString("LUP\n");

        mDown = false;

        if (next_mup_was_closing_menu)
        {
            next_mup_was_closing_menu = false;
            return;
        }

        if (mouseHasMovedSinceDownL)
        {
            // end of a drag
            // todo: i don't think we ever actually get here on the end of a drag
            // basically onMouseUpL is never called after a drag, not sure why
            // but it means some special mup state reset needs to happen in appDrag
        }
        else
        {
            // if (!clientPointIsOnProgressBar(
            //                 mDownPoint.x,
            //                 mDownPoint.y))
            // {
                // since this could be the mouse up in between the two clicks of a double click,
                // wait a little bit before we actually pause (should work with 0 delay as well)

                // don't queue up another pause if this is the end of a double click
                // we'll be restoring our pause state in restoreVideoPositionAfterDoubleClick() below
                if (!next_mup_was_double_click)
                {
                    registeredLastSingleClick = false; // we haven't until the timer runs out
                    mup_timer_point = {cx, cy};
                    single_click_timer_id = SetTimer(NULL, 0, MS_PAUSE_DELAY_FOR_DOUBLECLICK, &SingleLClickCallback);
                }
                else
                {
                    // if we are ending the click and we already registered the first click as a pause,
                    // toggle pause again to undo that
                    // doesn't this kind of assume the application L clicks undo each other?
                    if (registeredLastSingleClick)
                    {
                        if (DEBUG_MCLICK_MSGS) OutputDebugString("undo that click\n");

                        if (on_click) on_click(cx, cy);
                    }
                }
            // }
        }

        if (on_mup) on_mup();

        mouseHasMovedSinceDownL = false;
        // clickingOnProgressBar = false;

        if (next_mup_was_double_click)
        {
            next_mup_was_double_click = false;

            // cancel any pending single click effects lingering from the first mup of this dclick
            KillTimer(0, single_click_timer_id); // todo: rare race condition here right?

            // only restore if we actually paused/unpaused, otherwise we can just keep everything rolling as is
            if (registeredLastSingleClick)
            {
                if (DEBUG_MCLICK_MSGS) OutputDebugString("oops that was a double click\n");
                if (on_oops_that_was_a_double_click) on_oops_that_was_a_double_click();
                // restoreVideoPositionAfterDoubleClick();
            }
            else
            {
                if (DEBUG_MCLICK_MSGS) OutputDebugString("that was a fast doubleclick! no stutter\n");
            }
        }
    }

    void onDoubleClickDownL()
    {
        if (DEBUG_MCLICK_MSGS) OutputDebugString("LDOUBLECLICK\n");

        // if (clientPointIsOnProgressBar(
        //     mDownPoint.x,
        //     mDownPoint.y))
        // {
        //     // OutputDebugString("on bar dbl\n");
        //     clickingOnProgressBar = true;
        //     return;
        // }

        setFullscreen(!is_fullscreen);

        // instead make a note to restore in mouseUp
        next_mup_was_double_click = true;

    }

    void onMouseDownL(int clientX, int clientY)
    {
        if (DEBUG_MCLICK_MSGS) OutputDebugString("LDOWN\n");

        // i think we can just ignore if context menu is open
        if (menu_open)
            return;

        // mouse state / info about click...
        mDown = true;
        mouseHasMovedSinceDownL = false;
        mDownPoint = {clientX, clientY};

        // if (clientPointIsOnProgressBar(clientX, clientY))
        // {
        //     // OutputDebugString("on bar\n");
        //     clickingOnProgressBar = true;
        //     appSetProgressBar(clientX, clientY);
        // }
        // else
        // {
            // note this only works because onMouseDownL doesn't trigger on the second click of a double click
            // saveVideoPositionForAfterDoubleClick();
            if (on_mdown) on_mdown(clientX, clientY);
        // }
    }








};


// we won't need this singleton-like usage if there is
// a way to determine what instance we're in when wndproc is called
// could create map of hwnd to instance, but that seems like overkill
// todo: is there a way to pass a pointer to wndproc? maybe when registering?
// todo: uh, do we need for menu stuff too?
glass_window glass;




// find the workerW for wallpaper mode
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND p = FindWindowEx(hwnd, 0, "SHELLDLL_DefView", 0);
    if (p != 0)
    {
        // Gets the WorkerW Window after the current one.
        glass.workerw = FindWindowEx(0, hwnd, "WorkerW", 0);
    }
    return true;
}





#define GLASS_ID_EXIT         1001
#define GLASS_ID_CLICKTHRU    1002
#define GLASS_ID_TOPMOST      1003
#define GLASS_ID_FULLSCREEN   1004
#define GLASS_ID_SNAP         1005
#define GLASS_ID_OPAC10       1006
#define GLASS_ID_OPAC15       1007
#define GLASS_ID_OPAC25       1008
#define GLASS_ID_OPAC50       1009
#define GLASS_ID_OPAC75       1010
#define GLASS_ID_OPAC90       1011
#define GLASS_ID_OPAC100      1012
#define GLASS_ID_WALL         1013
#define GLASS_ID_LOCKRATIO    1014

void glass_default_open_menu_at(HWND hwnd, POINT point)
{
    UINT snap = glass.is_snappy          ? MF_CHECKED : MF_UNCHECKED;
    UINT clic = glass.is_clickthrough    ? MF_CHECKED : MF_UNCHECKED;
    UINT topm = glass.is_topmost         ? MF_CHECKED : MF_UNCHECKED;
    UINT full = glass.is_fullscreen      ? MF_CHECKED : MF_UNCHECKED;
    UINT wall = glass.is_wallpaper       ? MF_CHECKED : MF_UNCHECKED;
    UINT rati = glass.is_ratiolocked     ? MF_CHECKED : MF_UNCHECKED;

    UINT op10 = glass.opacity == 0.10    ? MF_CHECKED : MF_UNCHECKED;
    UINT op15 = glass.opacity == 0.15    ? MF_CHECKED : MF_UNCHECKED;
    UINT op25 = glass.opacity == 0.25    ? MF_CHECKED : MF_UNCHECKED;
    UINT op50 = glass.opacity == 0.50    ? MF_CHECKED : MF_UNCHECKED;
    UINT op75 = glass.opacity == 0.75    ? MF_CHECKED : MF_UNCHECKED;
    UINT op90 = glass.opacity == 0.90    ? MF_CHECKED : MF_UNCHECKED;
    UINT op00 = glass.opacity == 1.00    ? MF_CHECKED : MF_UNCHECKED;

    HMENU hPopupMenu = CreatePopupMenu();
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, GLASS_ID_EXIT, L"Exit");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | op10, GLASS_ID_OPAC10 , L"Opacity 10");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | op15, GLASS_ID_OPAC15 , L"Opacity 15");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | op25, GLASS_ID_OPAC25 , L"Opacity 25");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | op50, GLASS_ID_OPAC50 , L"Opacity 50");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | op75, GLASS_ID_OPAC75 , L"Opacity 75");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | op90, GLASS_ID_OPAC90 , L"Opacity 90");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | op00, GLASS_ID_OPAC100, L"Opacity 100");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | wall, GLASS_ID_WALL, L"Stick to wallpaper");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | snap, GLASS_ID_SNAP, L"Snap to edges");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | clic, GLASS_ID_CLICKTHRU, L"Cannot be clicked");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | rati, GLASS_ID_LOCKRATIO, L"Lock aspect ratio");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | topm, GLASS_ID_TOPMOST, L"Always on top");
    InsertMenuW(hPopupMenu, 0, MF_BYPOSITION | MF_STRING | full, GLASS_ID_FULLSCREEN, L"Fullscreen");
    SetForegroundWindow(hwnd);

    SetTimer(glass.hwnd, glass.render_timer_id, glass.sleep_ms, 0); // 0 to get WM_TIMER msgs
    glass.menu_open = true;
    glass.next_mup_was_closing_menu = true;
    TrackPopupMenu(hPopupMenu, TPM_TOPALIGN | TPM_LEFTALIGN, point.x, point.y, 0, hwnd, NULL);
    glass.menu_open = false;
    KillTimer(glass.hwnd, glass.render_timer_id);
}

void (*glass_custom_open_menu_at)(HWND, POINT) = 0;
void glass_open_menu_at(HWND hwnd, POINT point)
{
    glass.menu_open = true;
    if (glass_custom_open_menu_at) glass_custom_open_menu_at(hwnd, point);
    else glass_default_open_menu_at(hwnd, point);
}
















VOID CALLBACK SingleLClickCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    if (DEBUG_MCLICK_MSGS) OutputDebugString("DELAYED M LUP\n");

    // todo: anyway to pass a pointer to this function?
    // update: yes, only by co-opting the idEvent (timer id) int as a pointer
    // but if we try to start two timers with the same pointer... what happens?
    // maybe that's ok? each instance is only calling one of these at a time right?
    // (because otherwise it would be a double click?)
    // glass_window *glass = (glass_window*)___;

    // KillTimer(0, glass.single_click_timer_id);
    KillTimer(0, idEvent); // this way we're sure to kill it
    {
        int cx = glass.mup_timer_point.x;
        int cy = glass.mup_timer_point.y;
        if (glass.on_click) glass.on_click(cx, cy);

        // we have to track whether get here or not
        // so we know if we've toggled pause between our double click or not
        // (it could be either case now that we have a little delay in our pause)
        glass.registeredLastSingleClick = true;
    }
}



// for snapping
static int sys_moving_anchor_x;
static int sys_moving_anchor_y;

// for allowing placement above monitor edge
static bool in_movesize_loop;
static bool inhibit_movesize_loop;

LRESULT CALLBACK glass_WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {

        case WM_ENTERSIZEMOVE: {
            POINT mPos;   GetCursorPos(&mPos);
            RECT winRect; GetWindowRect(hwnd, &winRect);
            sys_moving_anchor_x = mPos.x - winRect.left;
            sys_moving_anchor_y = mPos.y - winRect.top;

            in_movesize_loop = true;
            inhibit_movesize_loop = false;

            SetTimer(hwnd, glass.render_timer_id, glass.sleep_ms, 0); // 0 to get WM_TIMER msgs
        } break;
        case WM_EXITSIZEMOVE: {
            in_movesize_loop = false;
            inhibit_movesize_loop = false;

            KillTimer(hwnd, glass.render_timer_id);
        } break;
        case WM_CAPTURECHANGED: {
            inhibit_movesize_loop = in_movesize_loop;
        } break;
        case WM_WINDOWPOSCHANGING: {
            if (inhibit_movesize_loop) {
                WINDOWPOS* wp = (WINDOWPOS*)(lParam);
                wp->flags |= SWP_NOMOVE;
                return 0;
            }
        } break;


        case WM_TIMER: {
            glass.main_update();
        } break;


        case WM_CLOSE: {
            glass.loop_running = false;
        } break;

        case WM_SIZE: {
            glass.cached_w = GET_X_LPARAM(lParam);
            glass.cached_h = GET_Y_LPARAM(lParam);
            glass.cached_size_up_to_date = true;
        }

        case WM_SIZING: {  // when dragging border
            if (!wParam) break; // if no W, then our L seems to be no good (not sure why)
            if (!lParam) break; // no L when minimizing apparently

            RECT rc = *(RECT*)lParam;
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            if (glass.is_ratiolocked)
            {
                double ratio = glass.aspect_ratio;
                switch (wParam)
                {
                    case WMSZ_LEFT:
                    case WMSZ_RIGHT:
                        rc.bottom = rc.top + (int)((double)w / ratio);
                        break;

                    case WMSZ_TOP:
                    case WMSZ_BOTTOM:
                        rc.right = rc.left + (int)((double)h * ratio);
                        break;

                    case WMSZ_LEFT + WMSZ_TOP:
                    case WMSZ_LEFT + WMSZ_BOTTOM:
                        rc.left = rc.right - (int)((double)h * ratio);
                        break;

                    case WMSZ_RIGHT + WMSZ_TOP:
                        rc.top = rc.bottom - (int)((double)w / ratio);
                        break;

                    case WMSZ_RIGHT + WMSZ_BOTTOM:
                        rc.bottom = rc.top + (int)((double)w / ratio);
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
            // todo: way to remove this from this layer?
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
            glass.onMouseDownL(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        } break;
        // case WM_NCLBUTTONDOWN: {
        // } break;
        case WM_LBUTTONDBLCLK: {
            glass.onDoubleClickDownL();
        } break;

        case WM_MOUSEMOVE: {
            glass.onMouseMove(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        } break;

        case WM_LBUTTONUP: {
            glass.onMouseUpL(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        } break;
        case WM_NCLBUTTONUP: {
            POINT newPoint = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &newPoint);
            glass.onMouseUpL(newPoint.x, newPoint.y);
        } break;

        case WM_RBUTTONDOWN: {    // rclicks in client area (HTCLIENT)
            POINT openPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &openPoint);
            glass_open_menu_at(hwnd, openPoint);
        } break;
        case WM_NCRBUTTONDOWN: {  // non-client area, apparently lParam is treated diff?
            POINT openPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            glass_open_menu_at(hwnd, openPoint);
        } break;


        case WM_MOVING: {
            if (glass.is_snappy)
            {
                POINT mPos;   GetCursorPos(&mPos);

                int width = ((RECT*)lParam)->right - ((RECT*)lParam)->left;
                int height = ((RECT*)lParam)->bottom - ((RECT*)lParam)->top;

                RECT positionIfNoSnap;
                positionIfNoSnap.left = mPos.x - sys_moving_anchor_x;
                positionIfNoSnap.top = mPos.y - sys_moving_anchor_y;
                positionIfNoSnap.right = positionIfNoSnap.left + width;
                positionIfNoSnap.bottom = positionIfNoSnap.top + height;

                SnapWinRectToMonitor(hwnd, positionIfNoSnap, (RECT*)lParam);
            }
            if (glass.cached_size_up_to_date)
            {
                RECT winRect = *(RECT*)lParam;
                winRect.right = winRect.left + glass.cached_w;
                winRect.bottom = winRect.top + glass.cached_h;
                *(RECT*)lParam = winRect;
            }
        } break;

        case WM_SYSKEYDOWN:  // req for alt?
        case WM_KEYDOWN: {
            if (wParam == VK_RETURN) // enter
            {
                if ((HIWORD(lParam) & KF_ALTDOWN)) // +alt
                {
                    glass.setFullscreen(!glass.is_fullscreen);
                }
            }
        } break;


        // can also implement IDropTarget, but who wants to do that?
        case WM_DROPFILES: {
            char filePath[MAX_PATH];
            // 0 = just take the first one
            if (DragQueryFile((HDROP)wParam, 0, (LPSTR)&filePath, MAX_PATH))
            {
                // OutputDebugString(filePath);
                // QueueLoadMovie(filePath);
                if (glass.on_dragdrop_file) glass.on_dragdrop_file(filePath);
            }
            else
            {
                // MsgBox("Unable to determine file path of dropped file.");
                // QueueNewMsg("not a file?", 0x7676eeff);
            }
        } break;


        case ID_SYSTRAY_MSG: {
            switch (lParam) {
                case WM_LBUTTONUP:
                    if (!glass.is_wallpaper)
                    {
                        SetForegroundWindow(hwnd);
                        glass.set_ghostmode(!glass.is_clickthrough);
                    }
                    else
                    {
                        // todo: make this behavior more consistent or improve it some how?
                        // todo: make this into a callback maybe so our abstraction doesn't leak?
                        hacky_extra_toggle_pause_function_for_glass();
                    }
                break;
                case WM_RBUTTONUP:
                    POINT mousePos;
                    GetCursorPos(&mousePos);
                    glass_open_menu_at(hwnd, mousePos);
                break;
            }
        } break;


        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case GLASS_ID_EXIT:       glass.loop_running = false;                   break;
                case GLASS_ID_CLICKTHRU:  glass.set_ghostmode(!glass.is_clickthrough);  break;
                case GLASS_ID_TOPMOST:    glass.set_topmost(!glass.is_topmost);         break;
                case GLASS_ID_FULLSCREEN: glass.setFullscreen(!glass.is_fullscreen);    break;
                case GLASS_ID_SNAP:       glass.set_snappy(!glass.is_snappy);           break;
                case GLASS_ID_WALL:       glass.set_wallpaper(!glass.is_wallpaper);     break;
                case GLASS_ID_LOCKRATIO:  glass.set_ratiolocked(!glass.is_ratiolocked); break;

                case GLASS_ID_OPAC10 : glass.set_opacity(0.10); break;
                case GLASS_ID_OPAC15 : glass.set_opacity(0.15); break;
                case GLASS_ID_OPAC25 : glass.set_opacity(0.25); break;
                case GLASS_ID_OPAC50 : glass.set_opacity(0.50); break;
                case GLASS_ID_OPAC75 : glass.set_opacity(0.75); break;
                case GLASS_ID_OPAC90 : glass.set_opacity(0.90); break;
                case GLASS_ID_OPAC100: glass.set_opacity(1.00); break;
            }

        } break;


    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}




bool glass_create_window(HINSTANCE hInstance, int x, int y, int w, int h, int nest = 0,
                         HICON icon = 0, char *title = "[no video]")
{
    glass.hInstance = hInstance;

    WNDCLASS wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = glass_WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "glass window";
    if (!RegisterClass(&wc)) { MessageBox(0,"RegisterClass failed.",0,0); return false; }

    RECT startRect = {x, y, x+w, y+h};

    // todo: test with multiple monitors
    if (nest != 0)
    {
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(MonitorFromWindow(0, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            if (nest == 1)
                startRect = { 0, 0, w, h};
            if (nest == 2)
                startRect = { mi.rcMonitor.right-w, 0, mi.rcMonitor.right, h};
            if (nest == 3)
                startRect = { 0, mi.rcWork.bottom-h, w, mi.rcWork.bottom};
            if (nest == 4)
                startRect = { mi.rcWork.right-w, mi.rcWork.bottom-h, mi.rcWork.right, mi.rcWork.bottom};
            if (nest == 5)
                startRect = { 0, mi.rcMonitor.bottom-h, w, mi.rcMonitor.bottom};
            if (nest == 6)
                startRect = { mi.rcMonitor.right-w, mi.rcMonitor.bottom-h, mi.rcMonitor.right, mi.rcMonitor.bottom};
        }
    }

    HWND hwnd = CreateWindowEx(
        // glass.opacity>=1? 0 : WS_EX_LAYERED,  // don't do this, better to set layered later
        // WS_EX_LAYERED, // needed for alpha, but gives us black bars on resize
        0,                // actually starting without WS_EX_LAYERED and setting later seems to fix black bars?
        wc.lpszClassName, title,
        WS_MINIMIZEBOX |   // i swear sometimes we can't shrink (via show desktop eg) without WS_MINIMIZEBOX
        WS_POPUP | WS_VISIBLE,
        // WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        startRect.left, startRect.top,
        startRect.right-startRect.left, startRect.bottom-startRect.top,
        0, 0, hInstance, 0);
    if (!hwnd) { MessageBox(0,"Couldn't open window.",0,0); return false; }
    glass.hwnd = hwnd;





    // if (!glass.icon) glass.icon = (HICON)GetClassLongPtr(hwnd, -14);  // -14 = GCL_HICON
    if (!glass.icon) glass.icon = icon_load(hInstance, ID_ICON);

    icon_set(glass.hwnd, glass.icon);
    icon_add_systray(glass.hwnd, glass.icon, title);


    glass.set_opacity(glass.opacity);
    glass.set_topmost(glass.is_topmost);

    // do not call here, wait until movie as been loaded and window is correct size
    // setFullscreen(is_fullscreen);

    if (glass.is_clickthrough) glass.set_ghostmode(true);

    // this has to be called after loading video so size is correct (maybe other things too)
    // (it's now called after every new video which is better anyway)
    // if (wallpaperMode)
    //     setWallpaperMode(hwnd, wallpaperMode);


    // ENABLE DRAG DROP
    DragAcceptFiles(hwnd, true);

    return true;
}

void glass_create_window_from_args_string(HINSTANCE hinstance, wchar_t **argList, int argCount)
{
    // defaults
    int startX = 0;
    int startY = 0;
    int startW = 960;
    int startH = 720;
    int nest = 0; // todo: enum 0=none, 1=tl, 2=tr, 3=bl, 4=br

    // parse args
    for (int i = 1; i < argCount; i++)  // skip first one which is name of exe
    {
        char nextArg[256]; // todo what max to use
        wcstombs(nextArg, argList[i], 256);
        if (nextArg[0] != '-')
        {
            // QueueLoadMovie(nextArg);
        }

        if (strcmp(nextArg, "-top") == 0) glass.is_topmost = true;
        if (strcmp(nextArg, "-notop") == 0) glass.is_topmost = false;

        if (strcmp(nextArg, "-aspect") == 0) glass.is_ratiolocked = true;
        if (strcmp(nextArg, "-noaspect") == 0) glass.is_ratiolocked = false;

        // if (strcmp(nextArg, "-repeat") == 0) { repeat = true; }
        // if (strcmp(nextArg, "-norepeat") == 0) { repeat = false; }

        if (strcmp(nextArg, "-ghost") == 0) glass.is_clickthrough = true;
        if (strcmp(nextArg, "-noghost") == 0) glass.is_clickthrough = false;

        if (strcmp(nextArg, "-snap") == 0) glass.is_snappy = true;
        if (strcmp(nextArg, "-nosnap") == 0) glass.is_snappy = false;

        // if (strcmp(nextArg, "-wall") == 0) wallpaperMode = true;
        // if (strcmp(nextArg, "-nowall") == 0) wallpaperMode = false;

        if (strcmp(nextArg, "-fullscreen") == 0) glass.is_fullscreen = true;
        if (strcmp(nextArg, "-nofullscreen") == 0) glass.is_fullscreen = false;

        // if (strcmp(nextArg, "-blinky") == 0) icon = GetIconByInt(randomInt(4) + 4*2);
        // if (strcmp(nextArg, "-pinky") == 0) icon = GetIconByInt(randomInt(4) + 4*1);
        // if (strcmp(nextArg, "-inky") == 0) icon = GetIconByInt(randomInt(4) + 4*0);
        // if (strcmp(nextArg, "-clyde") == 0) icon = GetIconByInt(randomInt(4) + 4*3);

        if (glass_string_starts_with(nextArg, "-opac"))
        {
            char *opacNum = nextArg + 5; // 5 = length of "-opac"
            glass.opacity = (double)atoi(opacNum) / 100.0;
        }

        // if (glass_string_starts_with(nextArg, "-vol"))
        // {
        //     char *volNum = nextArg + 4; // 4 = length of "-vol"
        //     volume = (double)atoi(volNum) / 100.0;
        // }

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

        if (glass_string_starts_with(nextArg, "-tl")) nest = 1;
        if (glass_string_starts_with(nextArg, "-tr")) nest = 2;
        if (glass_string_starts_with(nextArg, "-bl")) nest = 3;
        if (glass_string_starts_with(nextArg, "-br")) nest = 4;
        if (glass_string_starts_with(nextArg, "-Bl")) nest = 5;  // todo: better name for above/below taskbar
        if (glass_string_starts_with(nextArg, "-Br")) nest = 6;
    }

    glass_create_window(hinstance, startX, startY, startW, startH, nest);
}


void glass_close()
{
    icon_remove_systray(glass.hwnd);
    // UnhookWindowsHookEx(mouseHook);
}

// todo: there is a bug rendering with this method,
// if we click and drag the mouse only a few pixels,
// we'll lose a number of frames of rendering
// because this thread isn't getting any messages
// (not even timer messages or callbacks)

void glass_run_msg_render_loop()
{
    // MSG LOOP
    while (glass.loop_running)
    {
        MSG Message;
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        glass.main_update();

        Sleep(glass.sleep_ms);
    }
    glass_close();
}
