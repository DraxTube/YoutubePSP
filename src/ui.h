#ifndef UI_H
#define UI_H

#include "youtube.h"

/* Draw helpers */
void ui_draw_splash(void);
void ui_draw_message(const char *msg, unsigned int color);
void ui_draw_header(const char *title);
void ui_draw_footer(const char *hints);

/* Screens */
void ui_draw_main_menu(int selected);
void ui_draw_search_bar(const char *query, int cursor);
void ui_draw_results(const SearchResults *res, int selected, int scroll);
void ui_draw_video_info(const VideoInfo *v);
void ui_draw_settings(int selected);

/* On-screen keyboard */
int  ui_keyboard(char *out, int max_len, const char *title);

/* Main application loop (handles all states) */
void app_run(void);

#endif /* UI_H */
