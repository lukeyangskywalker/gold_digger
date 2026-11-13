/* Rasterizer for tools/pack_zh_font.py, using the stb_truetype copy that ships
   with LVGL (handles the CFF outlines of Source Han Sans). Usage:
     zh_glyphs FONT.otf CODEPOINTS.txt OUT.bin
   CODEPOINTS.txt holds decimal code points, one per line. For each one OUT.bin
   gets: uint8 width, then width*12 coverage bytes (rows top to bottom), with
   the glyph drawn at 12 px per em and the baseline 10 px from the top, as
   Pillow places it in ../Spartan/tools/pack_zh_font.py. */
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "src/libs/tiny_ttf/stb_truetype_htcw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SIZE = 12, BASELINE = 10 };

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: zh_glyphs FONT CODEPOINTS OUT\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) return perror(argv[1]), 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) return fprintf(stderr, "read failed\n"), 1;
    fclose(f);
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, data, stbtt_GetFontOffsetForIndex(data, 0))) return fprintf(stderr, "bad font\n"), 1;
    float scale = stbtt_ScaleForMappingEmToPixels(&font, SIZE);
    FILE *in = fopen(argv[2], "r"), *out = fopen(argv[3], "wb");
    if (!in || !out) return perror("open"), 1;
    int cp;
    while (fscanf(in, "%d", &cp) == 1) {
        int advance, bearing;
        stbtt_GetCodepointHMetrics(&font, cp, &advance, &bearing);
        int width = (int)lroundf(advance * scale);
        width = width < 1 ? 1 : width > SIZE ? SIZE : width;
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font, cp, scale, scale, &x0, &y0, &x1, &y1);
        unsigned char cell[SIZE * SIZE] = {0};
        int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0) {
            unsigned char *g = calloc((size_t)(w * h), 1);
            stbtt_MakeCodepointBitmap(&font, g, w, h, w, scale, scale, cp);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    int cx = x0 + x, cy = BASELINE + y0 + y;
                    if (cx >= 0 && cx < width && cy >= 0 && cy < SIZE) cell[cy * width + cx] = g[y * w + x];
                }
            free(g);
        }
        fputc(width, out);
        fwrite(cell, 1, (size_t)(width * SIZE), out);
    }
    fclose(in);
    fclose(out);
    return 0;
}
