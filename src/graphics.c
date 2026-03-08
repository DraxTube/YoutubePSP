#include "graphics.h"
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspgum.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ---- Internal framebuffer ---- */
static u32 __attribute__((aligned(16))) g_list[262144];
static u8 *g_vram_base = (u8*)0x04000000;
static int g_frame = 0;

/* Simple 8x8 bitmap font (ASCII 32-127) */
#include "font8x8.h"

void graphics_init(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);

    sceGuDrawBuffer(GU_PSM_8888, (void*)0, SCREEN_STRIDE);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void*)(SCREEN_STRIDE * SCREEN_HEIGHT * 4), SCREEN_STRIDE);
    sceGuDepthBuffer((void*)(SCREEN_STRIDE * SCREEN_HEIGHT * 8), SCREEN_STRIDE);

    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);

    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_BLEND);
    sceGuShadeModel(GU_FLAT);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void graphics_shutdown(void) {
    sceGuTerm();
}

static u32 *get_vram_addr(int frame) {
    if (frame == 0)
        return (u32*)(g_vram_base);
    else
        return (u32*)(g_vram_base + SCREEN_STRIDE * SCREEN_HEIGHT * 4);
}

void graphics_clear(u32 color) {
    u32 *fb = get_vram_addr(g_frame);
    int i;
    for (i = 0; i < SCREEN_STRIDE * SCREEN_HEIGHT; i++)
        fb[i] = color;
}

void graphics_flip(void) {
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf(get_vram_addr(g_frame),
                          SCREEN_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    g_frame ^= 1;
}

void draw_pixel(int x, int y, u32 color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    u32 *fb = get_vram_addr(g_frame);
    fb[y * SCREEN_STRIDE + x] = color;
}

void draw_rect_filled(int x, int y, int w, int h, u32 color) {
    int i, j;
    u32 *fb = get_vram_addr(g_frame);
    for (j = y; j < y + h; j++) {
        if (j < 0 || j >= SCREEN_HEIGHT) continue;
        for (i = x; i < x + w; i++) {
            if (i < 0 || i >= SCREEN_WIDTH) continue;
            fb[j * SCREEN_STRIDE + i] = color;
        }
    }
}

void draw_rect(int x, int y, int w, int h, u32 color) {
    draw_rect_filled(x, y, w, 1, color);
    draw_rect_filled(x, y + h - 1, w, 1, color);
    draw_rect_filled(x, y, 1, h, color);
    draw_rect_filled(x + w - 1, y, 1, h, color);
}

void draw_line(int x0, int y0, int x1, int y1, u32 color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
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
    int w = strlen(text) * 8;
    draw_text((SCREEN_WIDTH - w) / 2, y, color, "%s", text);
}

void draw_text_large(int x, int y, u32 color, const char *text) {
    /* 2x scaled text */
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
    return strlen(text) * 8;
}

void draw_scrollbar(int x, int y, int h, int total, int visible, int offset) {
    draw_rect_filled(x, y, 4, h, COLOR_DARKGRAY);
    if (total <= visible) return;
    int bar_h = (h * visible) / total;
    if (bar_h < 10) bar_h = 10;
    int bar_y = y + (h - bar_h) * offset / (total - visible);
    draw_rect_filled(x, bar_y, 4, bar_h, COLOR_GRAY);
}
