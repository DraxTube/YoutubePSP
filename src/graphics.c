#include "graphics.h"
#include <pspdisplay.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "font8x8.h"

/* Due framebuffer in VRAM per il double buffering */
#define FB0_ADDR  0x04000000
#define FB1_ADDR  (0x04000000 + SCREEN_STRIDE * SCREEN_HEIGHT * 4)

static u32 *g_draw_buf = (u32*)FB0_ADDR;
static u32 *g_disp_buf = (u32*)FB1_ADDR;

void graphics_init(void) {
    sceDisplaySetMode(0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceDisplaySetFrameBuf((void*)FB0_ADDR, SCREEN_STRIDE,
                          PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_IMMEDIATE);
    graphics_clear(COLOR_BLACK);
    graphics_flip();
    graphics_clear(COLOR_BLACK);
}

void graphics_shutdown(void) {
    /* nulla da fare */
}

void graphics_clear(u32 color) {
    int i;
    for (i = 0; i < SCREEN_STRIDE * SCREEN_HEIGHT; i++)
        g_draw_buf[i] = color;
}

void graphics_flip(void) {
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf((void*)g_draw_buf, SCREEN_STRIDE,
                          PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    /* swap */
    u32 *tmp  = g_disp_buf;
    g_disp_buf = g_draw_buf;
    g_draw_buf  = tmp;
}

void draw_pixel(int x, int y, u32 color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    g_draw_buf[y * SCREEN_STRIDE + x] = color;
}

void draw_rect_filled(int x, int y, int w, int h, u32 color) {
    int i, j;
    int x1 = x + w, y1 = y + h;
    if (x  < 0) x  = 0;
    if (y  < 0) y  = 0;
    if (x1 > SCREEN_WIDTH)  x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    for (j = y; j < y1; j++)
        for (i = x; i < x1; i++)
            g_draw_buf[j * SCREEN_STRIDE + i] = color;
}

void draw_rect(int x, int y, int w, int h, u32 color) {
    draw_rect_filled(x,         y,         w, 1, color);
    draw_rect_filled(x,         y + h - 1, w, 1, color);
    draw_rect_filled(x,         y,         1, h, color);
    draw_rect_filled(x + w - 1, y,         1, h, color);
}

void draw_line(int x0, int y0, int x1, int y1, u32 color) {
    int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_text(int x, int y, u32 color, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    int cx = x;
    const char *p = buf;
    while (*p) {
        char c = *p++;
        if (c == '\n') { cx = x; y += 10; continue; }
        int ci = (unsigned char)c - 32;
        if (ci < 0 || ci >= 96) { cx += 8; continue; }
        const u8 *glyph = font8x8_basic[ci];
        int bx, by;
        for (by = 0; by < 8; by++)
            for (bx = 0; bx < 8; bx++)
                if (glyph[by] & (1 << bx))
                    draw_pixel(cx + bx, y + by, color);
        cx += 8;
    }
}

void draw_text_center(int y, u32 color, const char *text) {
    int w = (int)strlen(text) * 8;
    draw_text((SCREEN_WIDTH - w) / 2, y, color, "%s", text);
}

void draw_text_large(int x, int y, u32 color, const char *text) {
    const char *p = text;
    int cx = x;
    while (*p) {
        char c = *p++;
        if (c == '\n') { cx = x; y += 20; continue; }
        int ci = (unsigned char)c - 32;
        if (ci < 0 || ci >= 96) { cx += 16; continue; }
        const u8 *glyph = font8x8_basic[ci];
        int bx, by;
        for (by = 0; by < 8; by++)
            for (bx = 0; bx < 8; bx++)
                if (glyph[by] & (1 << bx)) {
                    draw_pixel(cx + bx*2,     y + by*2,     color);
                    draw_pixel(cx + bx*2 + 1, y + by*2,     color);
                    draw_pixel(cx + bx*2,     y + by*2 + 1, color);
                    draw_pixel(cx + bx*2 + 1, y + by*2 + 1, color);
                }
        cx += 16;
    }
}

int text_width(const char *text) {
    return (int)strlen(text) * 8;
}

void draw_scrollbar(int x, int y, int h, int total, int visible, int offset) {
    draw_rect_filled(x, y, 4, h, COLOR_DARKGRAY);
    if (total <= visible) return;
    int bar_h = (h * visible) / total;
    if (bar_h < 10) bar_h = 10;
    int bar_y = y + (h - bar_h) * offset / (total - visible);
    draw_rect_filled(x, bar_y, 4, bar_h, COLOR_GRAY);
}
