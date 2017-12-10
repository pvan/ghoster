

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "lib/stb_truetype.h"

const int TTW = 1024;
const int TTH = 1024;
const float TTSIZE = 128.0;
unsigned char gray_temp_bitmap[TTW*TTH];
u8 *color_temp_bitmap;

stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs

textured_quad fontquad;

void tt_initfont(void)
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
void tt_print(float x, float y, char *text, int sw, int sh)
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