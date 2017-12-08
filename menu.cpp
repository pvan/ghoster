


// todo: make these more reusable
// menu class?
// submenu class?



const char *POPUP_CLASS_NAME = "ghoster popup window class";
const char *ICONMENU_CLASS_NAME = "ghoster icon submenu window class";



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
#define ID_WALLPAPER 1013
#define ID_VOLUME 1014
#define ID_SEP 1015
#define ID_ICONMENU 1016
#define ID_COPYURL 1017
#define ID_COPYURL_TIME 1018
#define ID_RANDO 1019

#define ID_SET_R 2001
#define ID_SET_P 2002
#define ID_SET_C 2003
#define ID_SET_Y 2004


struct menuItem
{
    int code;
    WCHAR *string;
    bool *checked;
    double *value;
    HBITMAP *hbitmap;
    menuItem *subMenu;
};

const int MI_WID = 250;
const int SMI_WID = 100;
const int MI_HEI = 21;
const int MI_HEI_SEP = 10;


menuItem iconMenuItems[] =
{
    {ID_SET_R, L"Blinky", 0, 0, &global_bitmap_r1 },
    {ID_SET_P, L"Pinky" , 0, 0, &global_bitmap_p1 },
    {ID_SET_C, L"Inky"  , 0, 0, &global_bitmap_c1 },
    {ID_SET_Y, L"Clyde" , 0, 0, &global_bitmap_y1 },
};

// todo: how to clean this up (this is a little better)
menuItem menuItems[] =
{
    {ID_PAUSE        , L"Play"                           , 0                                    , 0, 0, 0 },
    {ID_FULLSCREEN   , L"Fullscreen"                     , &global_ghoster.system.fullscreen    , 0, 0, 0 },
    {ID_REPEAT       , L"Repeat"                         , &global_ghoster.state.repeat         , 0, 0, 0 },
    {ID_VOLUME       , L"Volume"                         , 0, &global_ghoster.state.volume         , 0, 0 },
    {ID_SEP          , L""                               , 0                                    , 0, 0, 0 },
    {ID_RANDO        , L"Play Random"                    , 0                                    , 0, 0, 0 },
    {ID_PASTE        , L"Paste Clipboard URL"            , 0                                    , 0, 0, 0 },
    {ID_COPYURL      , L"Copy URL"                       , 0                                    , 0, 0, 0 },
    {ID_COPYURL_TIME , L"Copy URL At Current Time"       , 0                                    , 0, 0, 0 },
    {ID_SEP          , L""                               , 0                                    , 0, 0, 0 },
    {ID_RESET_RES    , L"Resize To Native Resolution"    , 0                                    , 0, 0, 0 },
    {ID_ASPECT       , L"Lock Aspect Ratio"              , &global_ghoster.state.lock_aspect    , 0, 0, 0 },
    {ID_SNAPPING     , L"Snap To Edges"                  , &global_ghoster.state.enableSnapping , 0, 0, 0 },
    {ID_SEP          , L""                               , 0                                    , 0, 0, 0 },
    {ID_ICONMENU     , L"Choose Icon"                    , 0                                    , 0, 0, iconMenuItems },
    {ID_SEP          , L""                               , 0                                    , 0, 0, 0 },
    {ID_TOPMOST      , L"Always On Top"                  , &global_ghoster.state.topMost        , 0, 0, 0 },
    {ID_CLICKTHRU    , L"Ghost Mode (Cannot Be Clicked)" , &global_ghoster.state.clickThrough   , 0, 0, 0 },
    {ID_WALLPAPER    , L"Stick To Wallpaper"             , &global_ghoster.state.wallpaperMode  , 0, 0, 0 },
    {ID_TRANSPARENCY , L"Opacity"                        , 0, &global_ghoster.state.opacity        , 0, 0 },
    {ID_SEP          , L""                               , 0                                    , 0, 0, 0 },
    {ID_EXIT         , L"Exit"                           , 0                                    , 0, 0, 0 },
};



void ShowPopupWindow(int x, int y, int width, int height)
{
    // OutputDebugString("show menu\n");

    SetWindowPos(global_popup_window, HWND_TOP, x, y, width, height, SWP_SHOWWINDOW);
    // ShowWindow(global_popup_window, SW_SHOW);
}

void HidePopupWindow()
{
    ShowWindow(global_popup_window, SW_HIDE);
}


void OpenRClickMenuAt(HWND hwnd, POINT point)
{

    int itemCount = sizeof(menuItems) / sizeof(menuItem);

    int width = MI_WID;
    // int height = MI_HEI * itemCount;
    int height = 8; // 5 top 3 bottom
    for (int i = 0; i < itemCount; i++)
    {
        if (menuItems[i].code == ID_SEP) height += MI_HEI_SEP;
        else height += MI_HEI;
    }

    int posX = point.x;
    int posY = point.y;

    // adjust if close to bottom or right edge of sreen
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
    {
        // or snap to edge?
        if (posX + width > mi.rcWork.right) posX -= width;
        if (posY + height > mi.rcWork.bottom) posY -= height;
    }


    global_ghoster.system.contextMenuOpen = true;

    // try this for making our notification menu close correctly
    // (seems to work)
    SetForegroundWindow(hwnd);


    // try show/hide now
    ShowPopupWindow(posX, posY, width, height);


    return;
}




void ShowSubMenu(int posX, int posY, int wid, int hei)
{
    // OutputDebugString("show sub\n");
    global_is_submenu_shown = true;
    SetWindowPos(
        global_icon_menu_window,
        0, posX, posY, wid, hei, 0);
    ShowWindow(global_icon_menu_window, SW_SHOW);
}
void HideSubMenu()
{
    // OutputDebugString("hide sub\n");
    global_is_submenu_shown = false;
    ShowWindow(global_icon_menu_window, SW_HIDE);
}


void CloseMenu()
{
    // OutputDebugString("hide menu\n");

    // DestroyWindow(hwnd);
    HidePopupWindow();

    // probably want this right?
    HideSubMenu();

    // now i think we can just set this false right away, no need for a timer
    global_ghoster.system.contextMenuOpen = false;

    // i forget why we couldn't just set this closed right away with when using trackpopup
    // maybe that same mouse event would trigger on the main window?
    // global_ghoster.state.menuCloseTimer.Start();
}

void onMenuItemClick(HWND hwnd, menuItem item)
{
    switch (item.code)
    {
        case ID_VOLUME:
            break;
        case ID_EXIT:
            global_ghoster.state.appRunning = false;
            break;
        case ID_PAUSE:
            global_ghoster.appTogglePause();
            break;
        case ID_ASPECT:
            SetWindowToAspectRatio(global_ghoster.system.window, global_ghoster.rolling_movie.aspect_ratio);
            global_ghoster.state.lock_aspect = !global_ghoster.state.lock_aspect;
            break;
        case ID_PASTE:
            PasteClipboard();
            break;
        case ID_COPYURL:
            CopyUrlToClipboard();
            break;
        case ID_COPYURL_TIME:
            CopyUrlToClipboard(true);
            break;
        case ID_RESET_RES:
            SetWindowToNativeRes(global_ghoster.system.window, global_ghoster.rolling_movie);
            break;
        case ID_REPEAT:
            global_ghoster.state.repeat = !global_ghoster.state.repeat;
            break;
        // case ID_TRANSPARENCY:  // now a slider
        //     global_ghoster.state.transparent = !global_ghoster.state.transparent;
        //     if (global_ghoster.state.transparent) setOpacity(global_ghoster.system.window, 0.5);
        //     if (!global_ghoster.state.transparent) setOpacity(global_ghoster.system.window, 1.0);
        //     break;
        case ID_CLICKTHRU:
            setGhostMode(global_ghoster.system.window, !global_ghoster.state.clickThrough);
            break;
        case ID_RANDICON:
            global_ghoster.system.icon = RandomIcon();
            if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
            break;
        case ID_TOPMOST:
            setTopMost(global_ghoster.system.window, !global_ghoster.state.topMost);
            break;
        int color;
        case ID_SET_C: color = 0;
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
            if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
            break;
        case ID_SET_P: color = 1;
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
            if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
            break;
        case ID_SET_R: color = 2;
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
            if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
            break;
        case ID_SET_Y: color = 3;
            global_ghoster.system.icon = GetIconByInt(randomInt(4) + 4*color);
            if (!global_ghoster.state.clickThrough) SetIcon(global_ghoster.system.window, global_ghoster.system.icon);
            break;
        case ID_FULLSCREEN:
            toggleFullscreen();
            break;
        case ID_SNAPPING:
            global_ghoster.state.enableSnapping = !global_ghoster.state.enableSnapping;
            if (global_ghoster.state.enableSnapping)
            {
                RECT winRect; GetWindowRect(global_ghoster.system.window, &winRect);

                SnapRectToMonitor(winRect, &winRect);

                SetWindowPos(global_ghoster.system.window, 0,
                    winRect.left, winRect.top,
                    winRect.right  - winRect.left,
                    winRect.bottom - winRect.top,
                    0);
            }
            break;
        case ID_WALLPAPER:
            setWallpaperMode(global_ghoster.system.window, !global_ghoster.state.wallpaperMode);
            break;
        case ID_RANDO:
            global_ghoster.message.QueuePlayRandom();
            break;
        // case ID_SEP:
        //     return; // don't close popup
        //     break;
    }

    // actually i don't think this ever triggers now that we check popupSliderCapture on mouseup
    if (item.value) return;  // don't close if clicking on slider

    // if (closeDontHide)
        // CloseMenu(hwnd);
    // else
    //     ShowWindow(hwnd, SW_HIDE);
}


int MouseOverMenuItem(POINT point, HWND hwnd, menuItem *menu, int count)
{
    RECT winRect;  GetWindowRect(hwnd, &winRect);

    int indexOfClick = 0;

    if (point.x < 0 || point.x > winRect.right-winRect.left) return -1; // off window
    if (point.y < 0 || point.y > winRect.bottom-winRect.top) return -1;

    int currentY = 5;
    for (int i = 0; i < count; i++)
    {
        int thisH = MI_HEI;
        if (menuItems[i].code == ID_SEP)
            thisH = MI_HEI_SEP;
        currentY += thisH;

        if (point.y < currentY)
            break;

        indexOfClick++;
    }
    if (indexOfClick >= count) return -1;
    return indexOfClick;
}

double PercentClickedOnSlider(HWND hwnd, POINT point, RECT winRect)
{

    double win_width = winRect.right - winRect.left;

    // // looks like we're actually getting wrap around on this..
    // // as though a negative int is getting cast as a u16 somewhere
    // // ah, of course LOWORD() was doing it, switched to GET_X_LPARAM
    // char buf[123];
    // sprintf(buf, "%i\n", (int)point.x);
    // OutputDebugString(buf);

    // gross!
    double min = 27; //guttersize
    min += 3; // "highlight"
    min += 5; // shrink for slider
    double max = win_width;
    max -= 3;
    max -= (27+5);

    double result = (point.x - min) / (max-min);

    if (result < 0) result = 0;
    if (result > 1.0) result = 1.0;

    // // another idea
    // POINT TL = {(LONG)min, winRect.top};
    // POINT BR = {(LONG)max+1, winRect.bottom};
    // ClientToScreen(hwnd, &TL);
    // ClientToScreen(hwnd, &BR);
    // RECT sliderZone = {TL.x, TL.y, BR.x, BR.y};
    // ClipCursor(&sliderZone);

    // char buf[123];
    // sprintf(buf, "%f\n", result);
    // OutputDebugString(buf);

    return result;
}


bool popupMouseDown = false;
double *popupSliderCapture = 0;  // are we dragging a slider? if so don't register mup on other items

// returns pointer to value of slider we clicked on (or are dragging a slider) false otherwise
double *updateSliders(HWND hwnd, POINT mouse)
{
    int index = MouseOverMenuItem(mouse, hwnd, menuItems, sizeof(menuItems) / sizeof(menuItem));
    if (index >= 0)
    {
        if (menuItems[index].value
            || popupSliderCapture)  // also update if we're off the slider but dragging it around
        {
            double *destination_value;
            if (popupSliderCapture != 0) // feels a bit awkward
                destination_value = popupSliderCapture;
            else
                destination_value = menuItems[index].value;

            RECT winRect; GetClientRect(hwnd, &winRect);
            // todo: really need a getMenuItemRect(index)
            double result = PercentClickedOnSlider(hwnd, mouse, winRect);
            *destination_value = result;

            // actually we need to call the official handlers... for now just check each one
            if (destination_value == &global_ghoster.state.opacity)
                setOpacity(global_ghoster.system.window, result);
            if (destination_value == &global_ghoster.state.volume)
                setVolume(result);

            return destination_value;
        }
    }
    return 0;
}

void PaintMenu(HWND hwnd, menuItem *menu, int menuCount, int selectedIndex)
{
    // TODO: cleanup the magic numbers in this paint handling
    // todo: support other windows styles? hrm

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);


    int width = ps.rcPaint.right-ps.rcPaint.left;
    int height = ps.rcPaint.bottom-ps.rcPaint.top;

    HDC memhdc = CreateCompatibleDC(hdc);
    HBITMAP buffer_bitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(memhdc, buffer_bitmap);


    RECT menuRect = ps.rcPaint;

    int gutterSize = 27;//GetSystemMetrics(SM_CXMENUCHECK);


    RECT thisRect;
    thisRect = menuRect;


    HTHEME theme = OpenThemeData(hwnd, L"MENU");

    DrawThemeBackground(theme, memhdc, MENU_POPUPGUTTER, 0, &thisRect, &thisRect);
    DrawThemeBackground(theme, memhdc, MENU_POPUPBORDERS, 0, &thisRect, &thisRect);


    LOGFONTW outFont;
    GetThemeSysFont(theme, TMT_MENUFONT, &outFont);
    HFONT font = CreateFontIndirectW(&outFont);
    SelectFont(memhdc, font);
    DeleteObject(font);

    SetBkMode(memhdc, TRANSPARENT);

    int currentY = 5;
    for (int i = 0; i < menuCount; i++)
    {

        // TODO: omg what a mess

        int thisH = MI_HEI;
        if (menu[i].code == ID_SEP)
            thisH = MI_HEI_SEP;

        RECT itemRect = {0, currentY, MI_WID, currentY+thisH};
        currentY += thisH;

        // pad
        itemRect.left += gutterSize;//+2;
        // itemRect.top += 1;
        // itemRect.right -= 1;
        // itemRect.bottom -= 1;


        // SEPARATORS
        if (menu[i].code == ID_SEP)
        {
            RECT sepRect = itemRect;
            sepRect.left -= gutterSize;
            sepRect.left += 4; sepRect.right -= 2;
            sepRect.top -= 3;
            sepRect.bottom -= 3;
            GetThemeBackgroundContentRect(theme, memhdc, MENU_POPUPSEPARATOR, 0, &sepRect, &thisRect);
            DrawThemeBackground(theme, memhdc, MENU_POPUPSEPARATOR, 0, &thisRect, &thisRect);
        }
        else
        {

            // HIGHLIGHT   // draw first to match win 7 native
            if (i == selectedIndex
                && !menu[i].value)  // don't HL sliders
            {
                RECT hlRect = itemRect;
                hlRect.left -= gutterSize;
                hlRect.left += 3;
                hlRect.right -= 3;
                hlRect.top -= 1;
                hlRect.bottom += 0;

                // todo: submenu width is hardcoded for now
                if (menu == iconMenuItems)
                {
                    hlRect.right -= (MI_WID-SMI_WID);
                }

                // GetThemeBackgroundContentRect(theme, memhdc, MENU_POPUPITEM, MPI_HOT, &hlRect, &hlRect); //needed?
                DrawThemeBackground(theme, memhdc, MENU_POPUPITEM, MPI_HOT, &hlRect, &hlRect);
            }

            // BITMAPS
            if (menu[i].hbitmap != 0)
            {
                RECT imgRect = itemRect;
                imgRect.left -= gutterSize;
                imgRect.left += 3;
                imgRect.top -= 1;
                imgRect.bottom -= 0;
                imgRect.right = gutterSize - 2;

                HBITMAP hbitmap = *(menu[i].hbitmap);
                HDC bitmapDC = CreateCompatibleDC(memhdc);
                SelectObject(bitmapDC, hbitmap);

                BITMAP imgBitmap;
                GetObject(hbitmap, sizeof(BITMAP), &imgBitmap);

                // center image in rect
                imgRect.left += ( (imgRect.right - imgRect.left) - (imgBitmap.bmWidth) ) /2;
                imgRect.top += ( (imgRect.bottom - imgRect.top) - (imgBitmap.bmHeight) ) /2;

                BitBlt(memhdc,
                       imgRect.left,
                       imgRect.top,
                       imgBitmap.bmWidth,
                       imgBitmap.bmHeight,
                       bitmapDC,
                       0, 0, SRCCOPY);

                DeleteDC    (bitmapDC);
            }


            // CHECKMARKS
            if (menu[i].checked)  //pointer exists
            {
                if (*(menu[i].checked))  // value is true
                {
                    RECT checkRect = itemRect;
                    checkRect.left -= gutterSize;
                    checkRect.left += 3;
                    checkRect.top -= 1;
                    checkRect.bottom -= 0;
                    checkRect.right = gutterSize - 2;

                    DrawThemeBackground(theme, memhdc, MENU_POPUPCHECKBACKGROUND, MCB_NORMAL, &checkRect, &checkRect);
                    DrawThemeBackground(theme, memhdc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL, &checkRect, &checkRect);
                }
            }


            // SUBMENU ARROW
            if (menu[i].code == ID_ICONMENU)  //pointer exists
            {
                RECT arrowRect = itemRect;
                arrowRect.left = arrowRect.right - gutterSize;

                // enlarge
                arrowRect.left -= 5;
                arrowRect.top -= 5;
                arrowRect.right += 5;
                arrowRect.bottom += 5;

                // shift
                arrowRect.left -= 2;
                arrowRect.top -= 1;
                arrowRect.right -= 2;
                arrowRect.bottom -= 1;

                DrawThemeBackground(theme, memhdc, MENU_POPUPSUBMENU, MSM_NORMAL, &arrowRect, &arrowRect);
            }


            // SLIDERS
            if (menu[i].value)  //pointer exists
            {
                RECT bedRect = itemRect;

                // like highlight
                bedRect.left += 3;
                bedRect.right -= 3;
                bedRect.top -= 1;
                bedRect.bottom += 0;

                // shrink for slider
                bedRect.left += 5;
                bedRect.right -= (gutterSize+5);
                bedRect.top += 2;
                bedRect.bottom -= 0;


                RECT blueRect = bedRect;

                // make a "track" / recessed
                blueRect.top++;
                blueRect.left++;
                blueRect.bottom++;
                blueRect.right++;

                RECT grayRect = blueRect;
                grayRect.left--; // scoot up against end of blue

                double percent = *(menu[i].value); // i guess we assume a value of 0-1 for now
                int bedWidth = blueRect.right - blueRect.left;  // use "inside" of track to calc percent (does it matter?)
                blueRect.right = blueRect.left + nearestInt(bedWidth * percent);
                grayRect.left = grayRect.left + nearestInt(bedWidth * percent);

                if (grayRect.left < bedRect.left+1) grayRect.left = bedRect.left+1;  // don't draw to the left


                HPEN grayPen = CreatePen(PS_SOLID, 1, 0x888888);
                HBRUSH blueBrush = CreateSolidBrush(0xE6D8AD);
                HBRUSH lightGrayBrush = CreateSolidBrush(0xe0e0e0);

                SelectObject(memhdc, grayPen);
                SelectObject(memhdc, GetStockObject(NULL_BRUSH)); // supposedly not needed to delete stock objs
                Rectangle(memhdc, bedRect.left, bedRect.top, bedRect.right, bedRect.bottom);

                SelectObject(memhdc, GetStockObject(NULL_PEN));
                SelectObject(memhdc, blueBrush);
                Rectangle(memhdc, blueRect.left, blueRect.top, blueRect.right, blueRect.bottom);

                SelectObject(memhdc, GetStockObject(NULL_PEN));
                SelectObject(memhdc,  lightGrayBrush);
                Rectangle(memhdc, grayRect.left, grayRect.top, grayRect.right, grayRect.bottom);

                DeleteObject(grayPen);  // or create once and reuse?
                DeleteObject(blueBrush);
                DeleteObject(lightGrayBrush);

                WCHAR display[64];
                swprintf(display, L"%s %i", menu[i].string, nearestInt(percent*100.0));
                DrawThemeText(theme, memhdc, MENU_POPUPITEM, MPI_NORMAL, display, -1,
                              DT_CENTER|DT_VCENTER|DT_SINGLELINE, 0, &bedRect);

            }
            else
            {
                // little override for play/pause
                WCHAR *display = menu[i].string;
                if (i == 0 && !global_ghoster.rolling_movie.is_paused
                    && menu == menuItems) // so hokey
                    display = L"Pause";

                // DrawText(hdc, menu[i].string, -1, &itemRect, 0);
                itemRect.left += 2;
                itemRect.top += 2;
                itemRect.left += 5; // more gutter gap
                DrawThemeText(theme, memhdc, MENU_POPUPITEM, MPI_NORMAL, display, -1, 0, 0, &itemRect);
            }
        }

    }

    CloseThemeData(theme); // or open/close on window creation?

    BitBlt(hdc, 0, 0, width, height, memhdc, 0, 0, SRCCOPY);
    DeleteObject(buffer_bitmap);
    DeleteDC    (memhdc);
    DeleteDC    (hdc);

    EndPaint(hwnd, &ps);
}


// hmm feels like this is getting out of hand?
LRESULT CALLBACK IconMenuWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    menuItem *menu = iconMenuItems;
    int menuCount = sizeof(iconMenuItems) / sizeof(menuItem);

    switch(message)
    {
        case WM_KILLFOCUS: {  // if we alt-tab, etc
            HideSubMenu();
            if ((HWND)wParam != global_popup_window)
                CloseMenu();
        } break;

        case WM_MOUSEMOVE: {
            POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            // if (popupMouseDown)
            // {
            //     updateSliders(hwnd, mouse);
            // }

            subMenuSelectedItem = MouseOverMenuItem(mouse, hwnd, menu, menuCount);

            RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
        } break;


        case WM_LBUTTONDOWN: {
            SetCapture(hwnd); // this seems ok
        } break;

        case WM_LBUTTONUP: {
            // if (!popupSliderCapture)
            // {
                POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                int indexOfClick = MouseOverMenuItem(mouse, hwnd, menu, menuCount);
                onMenuItemClick(hwnd, menu[indexOfClick]);
                HideSubMenu();
                CloseMenu(); // close main menu, not this submenu!
                // RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
            // }
            popupMouseDown = false;
            ReleaseCapture(); // undo capture in WM_LBUTTONDOWN
        } break;

        case WM_PAINT: {
            PaintMenu(hwnd, menu, sizeof(iconMenuItems)/sizeof(menuItem), subMenuSelectedItem);
            return 0;
        } break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}


// basically for main menu only
void onLoseFocus(HWND hwnd)
{
    if (!global_is_submenu_shown)
        CloseMenu();
}

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {

        case WM_KILLFOCUS: {
            onLoseFocus(hwnd);
            // popupMouseDown = false; // maybe we still want this though
        } break;
        // case WM_ACTIVATE: {  // lets wait until this actually seems needed to enable it
        //     if (LOWORD(wParam) == WA_INACTIVE)
        //         onLoseFocus(hwnd);
        // } break;

        case WM_MOUSEMOVE: {
            // OutputDebugString("MOVE\n");
            POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            if (popupMouseDown)
            {
                updateSliders(hwnd, mouse);
            }

            selectedItem = MouseOverMenuItem(mouse, hwnd, menuItems, sizeof(menuItems) / sizeof(menuItem));

            // calc submenu pos
            // do inside ShowSubMenu() ?
            RECT winRect; GetWindowRect(hwnd, &winRect);
            int menuItemPos = 220;  // todo: need a getmenuitemrect
            int posX = winRect.right - 7;  // they overlap a bit
            int posY = winRect.top + menuItemPos;
            int wid = SMI_WID;
            int hei = MI_HEI * (sizeof(iconMenuItems)/sizeof(menuItem)) + 8;

            // lock submenu to monitor
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(MonitorFromWindow(global_ghoster.system.window, MONITOR_DEFAULTTOPRIMARY), &mi))
            {
                if (posX+wid > mi.rcWork.right) posX -= (posX+wid - mi.rcWork.right);
                if (posY+hei > mi.rcWork.bottom) posY -= (posY+hei - mi.rcWork.bottom);
            }

            // check if mouse is on submenu
            POINT screenMousePos = mouse;
            ClientToScreen(hwnd, &screenMousePos);
            bool mouseOnSubMenu =
                screenMousePos.x > posX && screenMousePos.x < posX+wid &&
                screenMousePos.y > posY && screenMousePos.y < posY+hei;

            if (!mouseOnSubMenu)
                subMenuSelectedItem = -1; // prevent highlight if we're not over the menu

            if (menuItems[selectedItem].code == ID_ICONMENU)
            {
                ShowSubMenu(posX, posY, wid, hei);
            }
            else
            {
                if (!mouseOnSubMenu)
                {
                    HideSubMenu();
                }
            }

            RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);

            // SetCapture(hwnd);
        } break;

        case WM_RBUTTONDOWN: {
            // close if we right-click outside the window..
            POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (MouseOverMenuItem(mouse, hwnd, menuItems, sizeof(menuItems) / sizeof(menuItem)) < 0)
            {
                CloseMenu();
                // i think to match windows we should somehow register this hit wherever it lands
            }
        } break;

        case WM_LBUTTONDOWN: {
            if (DEBUG_MCLICK_MSGS) OutputDebugString("POPUP MDOWN\n");
            SetCapture(hwnd); // this seems ok

            popupMouseDown = true;
            POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            double *slider = updateSliders(hwnd, mouse);
            if (slider)
            {
                popupSliderCapture = slider;
            }
            RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);

            // only keep capture if on window
            if (MouseOverMenuItem(mouse, hwnd, menuItems, sizeof(menuItems) / sizeof(menuItem)) > 0)
            {
                // SetCapture(hwnd);
            }

            // since we're no longer closing on WM_KILLFOCUS, try manual close like this
            if (MouseOverMenuItem(mouse, hwnd, menuItems, sizeof(menuItems) / sizeof(menuItem)) < 0)
            {
                CloseMenu();
            }
        } break;

        case WM_LBUTTONUP: {
            if (DEBUG_MCLICK_MSGS) OutputDebugString("POPUP MUP\n");
            popupMouseDown = false;
            if (!popupSliderCapture)
            {
                POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                int indexOfClick = MouseOverMenuItem(mouse, hwnd, menuItems, sizeof(menuItems) / sizeof(menuItem));
                onMenuItemClick(hwnd, menuItems[indexOfClick]);
                if (menuItems[indexOfClick].code != ID_SEP)
                    CloseMenu();
                RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
            }
            // if (popupSliderCapture) ReleaseCapture();  // not sure if automatic or not
            popupSliderCapture = false;

            ReleaseCapture(); // undo capture in WM_LBUTTONDOWN
            // SetCapture(hwnd);
        } break;


        case WM_PAINT: {
            // OutputDebugString("PAINT\n");

            PaintMenu(hwnd, menuItems, sizeof(menuItems)/sizeof(menuItem), selectedItem);

            return 0;
        } break;

    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}



HWND InitPopupMenu(HINSTANCE hInstance, menuItem *menuItems, int itemCount)
{

     // register class for menu popup window if we ever use one
    WNDCLASS wc3 = {};
    wc3.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc3.lpfnWndProc =  PopupWndProc;
    wc3.hInstance = hInstance;
    wc3.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc3.lpszClassName = POPUP_CLASS_NAME;
    if (!RegisterClass(&wc3)) { LogError("RegisterClass for popup window failed."); return 0; }

    HWND popup = CreateWindowEx(
        WS_EX_TOPMOST |  WS_EX_TOOLWINDOW,
        POPUP_CLASS_NAME,
        "ghoster popup menu",
        WS_POPUP,// | WS_VISIBLE,
        0, 0,
        200, 200,
        0,0,
        hInstance,
        0);

    if (!popup) { LogError("Failed to create popup window."); }


    // todo: iterate through menu items checking for submenu
    // this means only the menu knows about submenus? yes that's even better

    // sub popup menu... can we reuse popup menu wndproc? gotta be a better way to make windows?
    WNDCLASS wc4 = {};
    wc4.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc4.lpfnWndProc =  IconMenuWndProc;
    wc4.hInstance = hInstance;
    wc4.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc4.lpszClassName = ICONMENU_CLASS_NAME;
    if (!RegisterClass(&wc4)) { LogError("RegisterClass for popup window failed."); return 0; }

    global_icon_menu_window = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc4.lpszClassName, "ghoster video player",
        WS_POPUP,
        20, 20,
        400, 400,
        // CW_USEDEFAULT, CW_USEDEFAULT,
        // CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, hInstance, 0);
    if (!global_icon_menu_window) { LogError("Couldn't open global_icon_menu_window."); }


    return popup;
}