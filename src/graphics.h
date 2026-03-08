#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <psptypes.h>

/* PSP screen dimensions */
#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  272
#define SCREEN_STRIDE  512

/* Colors (ABGR format for PSP) */
#define COLOR_BLACK    0xFF000000
#define COLOR_WHITE    0xFFFFFFFF
#define COLOR_RED      0xFF0000FF
#define COLOR_GREEN    0xFF00FF00
#define COLOR_BLUE     0xFFFF0000
#define COLOR_YELLOW   0xFF00FFFF
#define COLOR_GRAY     0xFF808080
#define COLOR_DARKGRAY 0xFF404040
#define COLOR_PSPTUBE  0xFF1A1A2E  /* Dark navy background */
#define COLOR_ACCENT   0xFF0045FF  /* YouTube-like red (BGR) */
#define COLOR_ACCENT2  0xFF003CB4  /* Darker red */

typedef struct {
    int x, y, w, h;
} Rect;

void  graphics_init(void);
void  graphics_shutdown(void);
void  graphics_clear(u32 color);
void  graphics_flip(void);
void  draw_pixel(int x, int y, u32 color);
void  draw_rect(int x, int y, int w, int h, u32 color);
void  draw_rect_filled(int x, int y, int w, int h, u32 color);
void  draw_line(int x0, int y0, int x1, int y1, u32 color);
void  draw_text(int x, int y, u32 color, const char *fmt, ...);
void  draw_text_center(int y, u32 color, const char *text);
void  draw_text_large(int x, int y, u32 color, const char *text);
int   text_width(const char *text);
void  draw_image(int x, int y, int w, int h, const u8 *data);
void  draw_scrollbar(int x, int y, int h, int total, int visible, int offset);

#endif /* GRAPHICS_H */
