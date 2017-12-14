

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "lib/stb_truetype.h"

const int TTW = 1024;
const int TTH = 1024;
const float TTSIZE = 128.0;
unsigned char gray_temp_bitmap[TTW*TTH];
u8 *color_temp_bitmap;

stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs

textured_quad fontquad;



stbtt_fontinfo ttfont;

void tt_init_nobake()
{
    u8 *ttfile_buffer = (u8*)malloc(1<<20);
    fread(ttfile_buffer, 1, 1<<20, fopen("c:/windows/fonts/segoeui.ttf", "rb"));

    if (!stbtt_InitFont(&ttfont, ttfile_buffer, 0))
    {
        MsgBox("stbtt init font failed");
    }
    // free(ttfile_buffer);  // looks like we need to keep this around?
}

void tt_print_nobake(float tx, float ty, char *text, int fsize, int sw, int sh,
                     float alpha, bool centerH, bool centerV, int bgA)
{
    int bitmapW = 0;
    int bitmapH = 0;
    int fontH = fsize;
    float scale = stbtt_ScaleForPixelHeight(&ttfont, fontH);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&ttfont, &ascent,&descent,&lineGap);
    ascent*=scale; descent*=scale; lineGap*=scale;

    int widthThisLine = 0;
    bitmapH = fontH; // should be same as ascent-descent because of our scale factor
    for (int i = 0; i < strlen(text); i++)
    {
        if (text[i] == '\n')
        {
            widthThisLine = 0;
            bitmapH += (fontH+lineGap);
        }
        else
        {
            // int x0,y0,x1,y1; stbtt_GetCodepointBitmapBox(&ttfont, text[i], scale,scale, &x0,&y0,&x1,&y1);
            int advance;    stbtt_GetCodepointHMetrics(&ttfont, text[i], &advance, 0);
            int kerning =   stbtt_GetCodepointKernAdvance(&ttfont, text[i], text[i+1]);
            widthThisLine += ((float)advance*scale + (float)kerning*scale) + 1; // +1 so we don't get truncated
            if (widthThisLine > bitmapW) bitmapW = widthThisLine;
        }
    }

    // bitmapW += 2; // need a little extra if we start inset a bit below?
    // bitmapH += 1; // little extra this way?

    u8 *gray_bitmap = (u8*)malloc(bitmapW * bitmapH);
    ZeroMemory(gray_bitmap, bitmapW * bitmapH);


    // TODO: the docs recommend we go char by char on a temp bitmap and alpha blend into the final
    // as the method we're using could cutoff chars that ovelap e.g. lj in arialbd

    int xpos = 2; // "leave a little padding in case the character extends left"
    int ypos = ascent; // not sure why we start at ascent actually..
    for (int i = 0; i < strlen(text); i++)
    {
        if (text[i] == '\n')
        {
            xpos = 0;
            ypos += (fontH+lineGap);
        }
        else
        {
            // int L,R,T,B;     stbtt_GetCodepointBitmapBox(&ttfont, text[i], scale,scale, &L,&T,&R,&B);
            int x0,y0,x1,y1; stbtt_GetCodepointBitmapBox(&ttfont, text[i], scale,scale, &x0,&y0,&x1,&y1);
            int advance;     stbtt_GetCodepointHMetrics(&ttfont, text[i], &advance, 0);
            int kerning =    stbtt_GetCodepointKernAdvance(&ttfont, text[i], text[i+1]);

            float x_shift = xpos - (float)floor(xpos);
            int offset = ((ypos+y0)*bitmapW) + (int)xpos+x0;
            if (text[i] != ' ' && text[i] != NBSP)
                stbtt_MakeCodepointBitmapSubpixel(&ttfont, gray_bitmap+offset, x1-x0, y1-y0, bitmapW, scale,scale,x_shift,0, text[i]);

            xpos += ((float)advance*scale + (float)kerning*scale);
        }
    }


    color_temp_bitmap = (u8*)malloc(bitmapW * bitmapH * 4);
    for (int px = 0; px < bitmapW; px++)
    {
        for (int py = 0; py < bitmapH; py++)
        {
            u8 *r = color_temp_bitmap + ((py*bitmapW)+px)*4 + 0;
            u8 *g = color_temp_bitmap + ((py*bitmapW)+px)*4 + 1;
            u8 *b = color_temp_bitmap + ((py*bitmapW)+px)*4 + 2;
            u8 *a = color_temp_bitmap + ((py*bitmapW)+px)*4 + 3;
            *r = *(gray_bitmap + ((py*bitmapW)+px));
            *g = *(gray_bitmap + ((py*bitmapW)+px));
            *b = *(gray_bitmap + ((py*bitmapW)+px));
            *a = *(gray_bitmap + ((py*bitmapW)+px));
            if (bgA != 0) *a = bgA;
        }
    }

    fontquad.update(color_temp_bitmap, bitmapW,bitmapH);

    // center text on input
    if (centerH) tx -= bitmapW/2.0; // since we calc these they should be decent way to center the text
    if (centerV) ty -= bitmapH/2.0;

    float verts[] = {
    //  x                            y                            z   u   v
        r_PixelToNDC(tx        ,sw), r_PixelToNDC(ty        ,sh), 0,  0,  1,
        r_PixelToNDC(tx        ,sw), r_PixelToNDC(bitmapH+ty,sh), 0,  0,  0,
        r_PixelToNDC(bitmapW+tx,sw), r_PixelToNDC(ty        ,sh), 0,  1,  1,
        r_PixelToNDC(bitmapW+tx,sw), r_PixelToNDC(bitmapH+ty,sh), 0,  1,  0,
    };
    fontquad.update_custom_verts(verts);
    fontquad.render(alpha);


    free(color_temp_bitmap);
    free(gray_bitmap);
}





void tt_init_bake()
{

    u8 *ttfile_buffer = (u8*)malloc(1<<20);
    fread(ttfile_buffer, 1, 1<<20, fopen("c:/windows/fonts/segoeui.ttf", "rb"));
    stbtt_BakeFontBitmap(ttfile_buffer,0, TTSIZE, gray_temp_bitmap,TTW,TTH, 32,96, cdata);
    free(ttfile_buffer);

    color_temp_bitmap = (u8*)malloc(TTW * TTH * 4);
    for (int x = 0; x < TTW; x++)
    {
        for (int y = 0; y < TTH; y++)
        {
            u8 *r = color_temp_bitmap + ((y*TTW)+x)*4 + 0;
            u8 *g = color_temp_bitmap + ((y*TTW)+x)*4 + 1;
            u8 *b = color_temp_bitmap + ((y*TTW)+x)*4 + 2;
            u8 *a = color_temp_bitmap + ((y*TTW)+x)*4 + 3;
            *r = *(gray_temp_bitmap + ((y*TTW)+x));
            *g = *(gray_temp_bitmap + ((y*TTW)+x));
            *b = *(gray_temp_bitmap + ((y*TTW)+x));
            *a = *(gray_temp_bitmap + ((y*TTW)+x));
        }
    }

    fontquad.update(color_temp_bitmap, TTW,TTH,  -0.5, -0.5, 0.5, 0.5);
}
// todo: outdated, args are for nobake I think
void tt_print_bake(float x, float y, char *text, int sw, int sh, float alpha, bool centerH, bool centerV)
{
    // textured_quad quad;
    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, TTW,TTH, *text-32, &x,&y,&q,0);//1=opengl & d3d10+,0=d3d9

            float verts[] = {
            //  x                      y                      z   u     v
                r_PixelToNDC(q.x0,sw), r_PixelToNDC(q.y0,sh), 0,  q.s0, q.t1,
                r_PixelToNDC(q.x0,sw), r_PixelToNDC(q.y1,sh), 0,  q.s0, q.t0,
                r_PixelToNDC(q.x1,sw), r_PixelToNDC(q.y0,sh), 0,  q.s1, q.t1,
                r_PixelToNDC(q.x1,sw), r_PixelToNDC(q.y1,sh), 0,  q.s1, q.t0,
            };
            fontquad.update_custom_verts(verts);
            fontquad.render();  // super inefficient, should be batching at least this text together
        }
        ++text;
    }
}



void tt_initfont()
{
    tt_init_nobake();
    return;
}


void tt_print(float x, float y, char *text, int fsize, int sw, int sh,
              float alpha, bool centerH, bool centerV, int bgA)
{
    tt_print_nobake(x, y, text, fsize, sw, sh, alpha, centerH, centerV, bgA);
    return;
}