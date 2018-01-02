

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

void hwnd_resize(HWND hwnd, int w, int h)
{
    RECT r; GetWindowRect(hwnd, &r);
    MoveWindow(hwnd, r.left, r.top, w, h, true);
}

RECT hwnd_set_rect_to_ratio(RECT rect, double aspect_ratio)
{
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    int nw = (int)((double)h * aspect_ratio);
    int nh = (int)((double)w / aspect_ratio);

    if (nw == 0) { assert(false);/*oops*/ return rect; }

    // always adjusting width for now
    return {rect.left, rect.top, rect.left+nw, rect.top+h};
}

// does not appear to work when dragging the window around?
// update: added workaround to cache w/h on SIZE and set rect to that on MOVING
void hwnd_set_to_aspect_ratio(HWND hwnd, double aspect_ratio)
{
    RECT winRect;
    GetWindowRect(hwnd, &winRect);
    int w = winRect.right - winRect.left;
    int h = winRect.bottom - winRect.top;

    // which to adjust tho?
    int nw = (int)((double)h * aspect_ratio);
    int nh = (int)((double)w / aspect_ratio);

    // this always makes smaller (not great)
    // if (nw < w) MoveWindow(hwnd, winRect.left, winRect.top, nw, h, true);
    // else MoveWindow(hwnd, winRect.left, winRect.top, w, nh, true);

    // now always adjusting width
    MoveWindow(hwnd, winRect.left, winRect.top, nw, h, true);
}


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

