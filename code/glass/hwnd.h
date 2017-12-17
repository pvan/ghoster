

// basically small set of generalized tools
// for manipulating properties of HWNDs


const bool FORCE_NON_ZERO_OPACITY = false;  // not sure if we want this or not


void hwnd_set_topmost(HWND hwnd, bool enable)
{
    if (enable)
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    else
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}
void hwnd_set_clickthrough(HWND hwnd, bool enable)
{
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable) style = style | WS_EX_TRANSPARENT;
    else        style = style & ~WS_EX_TRANSPARENT;
    SetWindowLong(hwnd, GWL_EXSTYLE, style);
}
void hwnd_set_layered(HWND hwnd, bool enable)  // needed for opacity
{
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable) style = style | WS_EX_LAYERED;
    else        style = style & ~WS_EX_LAYERED;
    SetWindowLong(hwnd, GWL_EXSTYLE, style);
}
double hwnd_set_opacity(HWND hwnd, double opac)   // 0-1
{
    if (opac < 0) opac = 0;
    if (opac > 1) opac = 1;

    if (FORCE_NON_ZERO_OPACITY)
    {
        // todo: .004 is about 1 on 1-255 scale
        if (opac < .01) // if we become completely invisible, we'll lose clicks
            opac = .01;
    }

    int opac0to255 = opac * 255.999;

    hwnd_set_layered(hwnd, opac0to255 == 255 ? false : true); // required for alpha, but adds black bars to resize
    SetLayeredWindowAttributes(hwnd, 0, opac0to255, LWA_ALPHA);

    return opac;
}


// void SetWindowSize(HWND hwnd, int wid, int hei)
// {
//     // UpdateWindowSize(wid, hei);

//     RECT winRect;
//     GetWindowRect(hwnd, &winRect);
//     MoveWindow(hwnd, winRect.left, winRect.top, wid, hei, true);
// }



// void hwnd_set_ghostmode(HWND hwnd, bool enable)
// {
//     is_clickthrough = enable;
//     setClickThrough(enable);
//     if (enable)
//     {
//         // SetIcon(this_icon);
//         if (GHOST_MODE_SETS_TOPMOST) setTopMost(true);
//         if (opacity > GHOST_MODE_MAX_OPACITY)
//         {
//             last_opacity = opacity;
//             had_to_cache_opacity = true;
//             opacity = GHOST_MODE_MAX_OPACITY;
//             setOpacity(opacity, false);
//         }
//         else
//         {
//             had_to_cache_opacity = false;
//         }
//     }
//     else
//     {
//         // SetIcon(hwnd, this_icon);
//         if (had_to_cache_opacity)
//         {
//             opacity = last_opacity;
//             setOpacity(opacity, false);
//         }
//     }
// }

