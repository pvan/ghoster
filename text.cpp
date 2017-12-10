

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "lib/stb_truetype.h"

unsigned char gray_temp_bitmap[512*512];
u8 *color_temp_bitmap;

stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs
// GLuint ftex;

textured_quad fontquad;

void tt_initfont(void)
{
    u8 *ttfile_buffer = (u8*)malloc(1<<20);
    fread(ttfile_buffer, 1, 1<<20, fopen("c:/windows/fonts/segoeui.ttf", "rb"));
    stbtt_BakeFontBitmap(ttfile_buffer,0, 64.0, gray_temp_bitmap,512,512, 32,96, cdata); // no guarantee this fits!
    free(ttfile_buffer);


    color_temp_bitmap = (u8*)malloc(512 * 512 * 4);
    for (int x = 0; x < 512; x++)
    {
        for (int y = 0; y < 512; y++)
        {
            u8 *r = color_temp_bitmap + ((y*512)+x)*4 + 0;
            u8 *g = color_temp_bitmap + ((y*512)+x)*4 + 1;
            u8 *b = color_temp_bitmap + ((y*512)+x)*4 + 2;
            u8 *a = color_temp_bitmap + ((y*512)+x)*4 + 3;
            *r = *(gray_temp_bitmap + ((y*512)+x));
            *g = *(gray_temp_bitmap + ((y*512)+x));
            *b = *(gray_temp_bitmap + ((y*512)+x));
            *a = *(gray_temp_bitmap + ((y*512)+x));
        }
    }

    fontquad.update(color_temp_bitmap, 512,512,  -0.5, -0.5, 0.5, 0.5);

    // glGenTextures(1, &ftex);
    // glBindTexture(GL_TEXTURE_2D, ftex);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 512,512, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
    // // can free temp_bitmap at this point
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}
void tt_print(float x, float y, char *text)
{


    // // assume orthographic projection with units = screen pixels, origin at top left
    // glEnable(GL_TEXTURE_2D);
    // glBindTexture(GL_TEXTURE_2D, ftex);
    // glBegin(GL_QUADS);
    // while (*text) {
    //    if (*text >= 32 && *text < 128) {
    //       stbtt_aligned_quad q;
    //       stbtt_GetBakedQuad(cdata, 512,512, *text-32, &x,&y,&q,1);//1=opengl & d3d10+,0=d3d9
    //       glTexCoord2f(q.s0,q.t1); glVertex2f(q.x0,q.y0);
    //       glTexCoord2f(q.s1,q.t1); glVertex2f(q.x1,q.y0);
    //       glTexCoord2f(q.s1,q.t0); glVertex2f(q.x1,q.y1);
    //       glTexCoord2f(q.s0,q.t0); glVertex2f(q.x0,q.y1);
    //    }
    //    ++text;
    // }
    // glEnd();
}