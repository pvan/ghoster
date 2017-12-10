

// basic wrapper for our hardware layer

// todo: could swap these out more easily
// is it even worth having this wrapper though?


// #include "opengl.cpp"
#include "directx.cpp"


void r_init(HWND win, int w, int h)
{
    d3d_load();  // or sep function?
    d3d_init(win, w, h);
}


void r_clear(double r = 0, double g = 0, double b = 0, double a = 1)
{
    d3d_clear(r*256.0, g*256.0, b*256.0, a*256.0); // 256 since we're truncating
}

// void r_clear_rect(double r, double g, double b, double a, RECT dest)
// {
//     d3d_clear_rect(r*256.0, g*256.0, b*256.0, a*256.0, dest); // 256 since we're truncating
// }

void r_color_quad(u32 col, double wScale, double hScale)
{
    d3d_update_quad(0, 0, 0.5, 0.5, 0.5);
    // // d3d_update_texture(tex, 1, 1);
    // d3d_update_texture(tex, 1, 1);
    // d3d_render_mem_to_texture(tex, (u8*)&col, 1, 1);

    d3d_render();
}


// void r_StartFrame(HWND window)
// {
//     hdcCurrent = GetDC(window);
//     winCurrent = window;
//     wglMakeCurrent(hdcCurrent, rendering_context); // map future gl calls to our hdc

//     RECT winRect;
//     GetWindowRect(window, &winRect);
//     currentWid = winRect.right-winRect.left;
//     currentHei = winRect.bottom-winRect.top;
// }

// void r_render_quad(u8 *quadMem, int quadWid, int quadHei, double quadAlpha, RECT dest = {0})
// {
//     if (quadMem)
//         d3d_render_mem_to_texture(tex, quadMem, quadWid, quadHei);

//     d3d_viewport(dest);

//     d3d_render();
// }

void r_render_quad(u8 *quadMem, int quadWid, int quadHei)//, double quadAlpha, RECT dest = {0})
{
    if (quadMem) {
        d3d_update_quad(-1, -1, 1, 1, 0);
        d3d_update_texture(tex, 960, 720);
        d3d_render_mem_to_texture(tex, quadMem, quadWid, quadHei);
    }
    // d3d_render_pattern_to_texture(tex, temp_dt);
    d3d_render();
}

void r_swap()
{
    d3d_swap();
}

// void r_render_msg(MessageOverlay overlay, int x, int y, int scale, int horizAlign, int vertAlign)
// {
//     // todo: add option for dest rect here like quadToRect?
//     glViewport(0, 0, currentWid, currentHei);

//     if (overlay.msLeftOfDisplay > 0)
//     {
//         GLTtext *text = gltCreateText();
//         gltSetText(text, overlay.text.memory);

//         gltColor(1.0f, 1.0f, 1.0f, overlay.alpha);
//         gltDrawText2DAligned(text, x, y, scale, horizAlign, vertAlign);

//         gltDeleteText(text);
//     }
// }



RECT r_CalcLetterBoxRect(int dWID, int dHEI, double aspect_ratio)
{
    int calcWID = (int)((double)dHEI * aspect_ratio);
    int calcHEI = (int)((double)dWID / aspect_ratio);

    if (calcWID > dWID)  // letterbox
        calcWID = dWID;
    else
        calcHEI = dHEI;  // pillarbox

    int posX = ((double)dWID - (double)calcWID) / 2.0;
    int posY = ((double)dHEI - (double)calcHEI) / 2.0;

    return {posX, posY, calcWID, calcHEI};
}


void r_cleanup()
{
    d3d_cleanup();
}