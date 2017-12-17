

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "../lib/stb_truetype.h"


stbtt_fontinfo ttfont;
bool ttf_initialized = false;

void ttf_init()
{
    u8 *file_buffer = (u8*)malloc(1<<20);
    fread(file_buffer, 1, 1<<20, fopen("c:/windows/fonts/segoeui.ttf", "rb"));

    if (!stbtt_InitFont(&ttfont, file_buffer, 0))
    {
        MessageBox(0, "stbtt init font failed", 0,0);
    }
    // free(file_buffer);  // looks like we need to keep this around?
    ttf_initialized = true;
}

d3d_textured_quad ttf_create(char *text, int fsize, float alpha, bool centerH, bool centerV, int bgA = 0)
{
    if (!ttf_initialized) { OutputDebugString("Don't forget to init stbtt!\n"); assert(false); }

    d3d_textured_quad fontquad = {0};
    // if (!text) fontquad;  // needed or not?

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


    u8 *color_temp_bitmap = (u8*)malloc(bitmapW * bitmapH * 4);
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
            // *a = 255;
        }
    }

    fontquad.update(color_temp_bitmap, bitmapW,bitmapH);


    free(color_temp_bitmap);
    free(gray_bitmap);

    return fontquad;
}




