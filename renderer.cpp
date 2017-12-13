

// basic wrapper for our hardware layer

// todo: could swap these out more easily
// is it even worth having this wrapper though?


// #include "opengl.cpp"
#include "directx.cpp"


#define textured_quad d3d_textured_quad


// todo: derp, might want actually header files at some point
void tt_print(float x, float y, char *text, int sw, int sh, bool centerH, bool centerV);


void r_init(HWND win, int w, int h)
{
    d3d_load();  // or sep function?
    d3d_init(win, w, h);
}


void r_clear(double r = 0, double g = 0, double b = 0, double a = 1)
{
    d3d_clear(r*256.0, g*256.0, b*256.0, a*256.0); // 256 since we're truncating?
}

void r_swap()
{
    d3d_swap();
}

void r_resize(int w, int h)
{
    d3d_resize(w, h);
}

void r_render_msg(MessageOverlay overlay, int x, int y, int sw, int sh, bool centerH = true, bool centerV = true)
{

    if (overlay.msLeftOfDisplay > 0)
    {
        // todo: improve the tt api
        tt_print(x, y, overlay.text.memory, sw, sh, centerH, centerV);
    }

    // // todo: add option for dest rect here like quadToRect?
    // glViewport(0, 0, currentWid, currentHei);

    // if (overlay.msLeftOfDisplay > 0)
    // {
    //     GLTtext *text = gltCreateText();
    //     gltSetText(text, overlay.text.memory);

    //     gltColor(1.0f, 1.0f, 1.0f, overlay.alpha);
    //     gltDrawText2DAligned(text, x, y, scale, horizAlign, vertAlign);

    //     gltDeleteText(text);
    // }
}



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

    return {posX, posY, posX+calcWID, posY+calcHEI};
}

float r_PixelToNDC(int pixel, int size)
{
    return ((float)pixel / (float)size)*2.0f - 1.0f;
}

void r_cleanup()
{
    d3d_cleanup();
}