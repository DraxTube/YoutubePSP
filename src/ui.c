#include "ui.h"
#include "graphics.h"
#include "youtube.h"
#include "video.h"
#include "config.h"
#include <pspctrl.h>
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>

/* ---- Splash ---- */
void ui_draw_splash(void) {
    graphics_clear(COLOR_PSPTUBE);

    /* Logo box */
    draw_rect_filled(150, 90, 180, 50, COLOR_ACCENT);
    draw_rect_filled(152, 92, 176, 46, COLOR_ACCENT2);

    /* Play triangle */
    int cx = 240, cy = 115;
    int i;
    for (i = 0; i < 18; i++) {
        draw_line(cx - i/2, cy - i, cx + i, cy, COLOR_WHITE);
        draw_line(cx - i/2, cy + i, cx + i, cy, COLOR_WHITE);
    }

    draw_text_center(150, COLOR_WHITE,    "PSPTube");
    draw_text_center(164, COLOR_GRAY,     "YouTube per PSP");
    draw_text_center(220, COLOR_DARKGRAY, "v1.0  -  homebrew");

    graphics_flip();
}

/* ---- Generic message ---- */
void ui_draw_message(const char *msg, unsigned int color) {
    graphics_clear(COLOR_PSPTUBE);
    ui_draw_header("PSPTube");
    draw_text_center(120, color, msg);
    graphics_flip();
}

/* ---- Header bar ---- */
void ui_draw_header(const char *title) {
    draw_rect_filled(0, 0, SCREEN_WIDTH, 22, COLOR_ACCENT);
    draw_text(6, 7, COLOR_WHITE, title);
    /* battery indicator placeholder */
    draw_rect(450, 6, 22, 10, COLOR_WHITE);
    draw_rect_filled(472, 9, 3, 4, COLOR_WHITE);
}

/* ---- Footer hints ---- */
void ui_draw_footer(const char *hints) {
    draw_rect_filled(0, SCREEN_HEIGHT - 16, SCREEN_WIDTH, 16, COLOR_DARKGRAY);
    draw_text(4, SCREEN_HEIGHT - 12, COLOR_GRAY, hints);
}

/* ---- Main menu ---- */
static const char *MENU_ITEMS[] = {
    "  Cerca video",
    "  Video di tendenza",
    "  Impostazioni",
    "  Esci"
};
#define MENU_COUNT 4

void ui_draw_main_menu(int selected) {
    graphics_clear(COLOR_PSPTUBE);
    ui_draw_header("PSPTube - Menu principale");

    int i;
    for (i = 0; i < MENU_COUNT; i++) {
        int y = 50 + i * 36;
        if (i == selected) {
            draw_rect_filled(10, y - 4, SCREEN_WIDTH - 20, 26, COLOR_ACCENT2);
            draw_rect(10, y - 4, SCREEN_WIDTH - 20, 26, COLOR_ACCENT);
            draw_text(20, y + 4, COLOR_WHITE, MENU_ITEMS[i]);
        } else {
            draw_text(20, y + 4, COLOR_GRAY, MENU_ITEMS[i]);
        }
    }

    ui_draw_footer("Cruz: naviga  X: seleziona");
    graphics_flip();
}

/* ---- Search bar ---- */
void ui_draw_search_bar(const char *query, int cursor) {
    graphics_clear(COLOR_PSPTUBE);
    ui_draw_header("PSPTube - Cerca");

    draw_text_center(60, COLOR_GRAY, "Inserisci termine di ricerca:");
    draw_rect_filled(20, 82, SCREEN_WIDTH - 40, 22, COLOR_DARKGRAY);
    draw_rect(20, 82, SCREEN_WIDTH - 40, 22, COLOR_ACCENT);
    draw_text(28, 88, COLOR_WHITE, query);

    /* Cursor */
    int cx = 28 + cursor * 8;
    draw_line(cx, 86, cx, 100, COLOR_WHITE);

    ui_draw_footer("X: cerca  O: annulla  Start: tastiera");
    graphics_flip();
}

/* ---- Results list ---- */
void ui_draw_results(const SearchResults *res, int selected, int scroll) {
    graphics_clear(COLOR_PSPTUBE);
    ui_draw_header("PSPTube - Risultati");

    if (res->count == 0) {
        draw_text_center(130, COLOR_GRAY, "Nessun risultato trovato.");
    } else {
        int i;
        for (i = 0; i < res->count && i < 6; i++) {
            int idx = i + scroll;
            if (idx >= res->count) break;
            const VideoInfo *v = &res->items[idx];
            int y = 28 + i * 38;

            if (idx == selected) {
                draw_rect_filled(4, y - 2, SCREEN_WIDTH - 18, 36, 0xFF2A2A4A);
                draw_rect(4, y - 2, SCREEN_WIDTH - 18, 36, COLOR_ACCENT);
            }

            /* Thumbnail placeholder */
            draw_rect_filled(8, y, 56, 32, COLOR_DARKGRAY);
            draw_rect(8, y, 56, 32, COLOR_GRAY);
            /* Play icon in thumb */
            draw_line(20, y + 8,  20, y + 24, COLOR_WHITE);
            draw_line(20, y + 8,  36, y + 16, COLOR_WHITE);
            draw_line(20, y + 24, 36, y + 16, COLOR_WHITE);

            /* Title (truncated) */
            char title[40];
            strncpy(title, v->title, 39);
            title[39] = '\0';
            draw_text(70, y + 2,  COLOR_WHITE, title);
            draw_text(70, y + 14, COLOR_GRAY,  v->channel);
            draw_text(70, y + 24, COLOR_DARKGRAY, "%s  %s visualiz.", v->duration, v->view_count);
        }

        /* Scrollbar */
        if (res->count > 6)
            draw_scrollbar(SCREEN_WIDTH - 8, 26, SCREEN_HEIGHT - 44, res->count, 6, scroll);
    }

    ui_draw_footer("Cruz: naviga  X: riproduci  O: indietro  R/L: pagina");
    graphics_flip();
}

/* ---- Video info overlay ---- */
void ui_draw_video_info(const VideoInfo *v) {
    draw_rect_filled(0, SCREEN_HEIGHT - 50, SCREEN_WIDTH, 50, 0xCC000000);
    draw_text(8, SCREEN_HEIGHT - 46, COLOR_WHITE, v->title);
    draw_text(8, SCREEN_HEIGHT - 32, COLOR_GRAY,  v->channel);
    draw_text(8, SCREEN_HEIGHT - 18, COLOR_GRAY,  "%s  |  %s views", v->duration, v->view_count);
}

/* ---- Settings ---- */
static const char *SETTINGS_LABELS[] = {
    "Proxy host",
    "Proxy port",
    "WiFi config (1-3)",
    "Volume",
    "Salva e torna"
};
#define SETTINGS_COUNT 5

void ui_draw_settings(int selected) {
    graphics_clear(COLOR_PSPTUBE);
    ui_draw_header("PSPTube - Impostazioni");

    char vals[5][32];
    snprintf(vals[0], 32, "%s", g_config.proxy_host);
    snprintf(vals[1], 32, "%d", g_config.proxy_port);
    snprintf(vals[2], 32, "%d", g_config.wifi_config);
    snprintf(vals[3], 32, "%d%%", g_config.volume);
    snprintf(vals[4], 32, "");

    int i;
    for (i = 0; i < SETTINGS_COUNT; i++) {
        int y = 35 + i * 36;
        if (i == selected) {
            draw_rect_filled(8, y - 3, SCREEN_WIDTH - 16, 28, 0xFF2A2A4A);
            draw_rect(8, y - 3, SCREEN_WIDTH - 16, 28, COLOR_ACCENT);
            draw_text(16, y + 2,  COLOR_WHITE, SETTINGS_LABELS[i]);
            draw_text(280, y + 2, COLOR_YELLOW, vals[i]);
        } else {
            draw_text(16, y + 2,  COLOR_GRAY, SETTINGS_LABELS[i]);
            draw_text(280, y + 2, COLOR_WHITE, vals[i]);
        }
    }

    ui_draw_footer("Cruz: naviga  X: modifica  O: indietro");
    graphics_flip();
}

/* ---- On-screen keyboard (simple) ---- */
static const char *ROWS[] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl ",
    "zxcvbnm.-_"
};
#define KB_ROWS 4
#define KB_COLS 10

int ui_keyboard(char *out, int max_len, const char *title) {
    int row = 1, col = 0;
    int len = 0;
    out[0] = '\0';
    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int pressed = pad.Buttons & ~old_pad.Buttons;
        old_pad = pad;

        if (pressed & PSP_CTRL_UP)    { row--; if (row < 0) row = KB_ROWS - 1; }
        if (pressed & PSP_CTRL_DOWN)  { row++; if (row >= KB_ROWS) row = 0; }
        if (pressed & PSP_CTRL_LEFT)  { col--; if (col < 0) col = KB_COLS - 1; }
        if (pressed & PSP_CTRL_RIGHT) { col++; if (col >= KB_COLS) col = 0; }

        if (pressed & PSP_CTRL_CROSS) {
            char c = ROWS[row][col];
            if (len < max_len - 1) { out[len++] = c; out[len] = '\0'; }
        }
        if (pressed & PSP_CTRL_SQUARE) {
            if (len > 0) out[--len] = '\0'; /* backspace */
        }
        if (pressed & PSP_CTRL_START) break;  /* confirm */
        if (pressed & PSP_CTRL_CIRCLE) { out[0] = '\0'; return 0; } /* cancel */

        /* Draw keyboard */
        graphics_clear(COLOR_PSPTUBE);
        ui_draw_header(title);

        /* Current text */
        draw_rect_filled(10, 28, SCREEN_WIDTH - 20, 18, COLOR_DARKGRAY);
        draw_text(14, 31, COLOR_WHITE, out);

        int r, c2;
        for (r = 0; r < KB_ROWS; r++) {
            for (c2 = 0; c2 < KB_COLS; c2++) {
                int kx = 14 + c2 * 46, ky = 56 + r * 44;
                if (r == row && c2 == col)
                    draw_rect_filled(kx - 2, ky - 2, 40, 36, COLOR_ACCENT);
                draw_rect(kx - 2, ky - 2, 40, 36, COLOR_GRAY);
                char s[2] = { ROWS[r][c2], '\0' };
                draw_text(kx + 12, ky + 12, COLOR_WHITE, s);
            }
        }
        ui_draw_footer("Cruz: muovi  X: inserisci  Sq: backspace  Start: ok  O: annulla");
        graphics_flip();

        sceKernelDelayThread(50000);
    }
    return len;
}

/* ================================================================
 * Main application state machine
 * ================================================================ */
void app_run(void) {
    typedef enum { S_MENU, S_SEARCH, S_LOADING, S_RESULTS, S_PLAYING, S_SETTINGS } State;
    State state = S_MENU;

    int menu_sel = 0;
    int res_sel  = 0, res_scroll = 0;
    int set_sel  = 0;

    char search_query[128] = "";
    SearchResults results;
    memset(&results, 0, sizeof(results));

    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));

    /* Load trending on startup */
    state = S_LOADING;
    ui_draw_message("Caricamento tendenze...", COLOR_WHITE);
    if (youtube_trending(&results) > 0) {
        res_sel = 0; res_scroll = 0;
        state = S_RESULTS;
    } else {
        state = S_MENU;
    }

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int pressed = pad.Buttons & ~old_pad.Buttons;
        old_pad = pad;

        switch (state) {

        /* ---- MENU ---- */
        case S_MENU:
            ui_draw_main_menu(menu_sel);
            if (pressed & PSP_CTRL_UP)   { menu_sel--; if (menu_sel < 0) menu_sel = MENU_COUNT - 1; }
            if (pressed & PSP_CTRL_DOWN) { menu_sel++; if (menu_sel >= MENU_COUNT) menu_sel = 0; }
            if (pressed & PSP_CTRL_CROSS) {
                switch (menu_sel) {
                case 0: /* Search */
                    search_query[0] = '\0';
                    ui_keyboard(search_query, sizeof(search_query), "Cerca su PSPTube");
                    if (search_query[0]) {
                        state = S_LOADING;
                        ui_draw_message("Ricerca in corso...", COLOR_WHITE);
                        if (youtube_search(search_query, NULL, &results) > 0) {
                            res_sel = 0; res_scroll = 0;
                            state = S_RESULTS;
                        } else {
                            ui_draw_message("Errore durante la ricerca.", COLOR_RED);
                            sceKernelDelayThread(2000000);
                            state = S_MENU;
                        }
                    }
                    break;
                case 1: /* Trending */
                    state = S_LOADING;
                    ui_draw_message("Caricamento tendenze...", COLOR_WHITE);
                    if (youtube_trending(&results) > 0) {
                        res_sel = 0; res_scroll = 0;
                        state = S_RESULTS;
                    } else {
                        ui_draw_message("Impossibile caricare le tendenze.", COLOR_RED);
                        sceKernelDelayThread(2000000);
                        state = S_MENU;
                    }
                    break;
                case 2: /* Settings */
                    set_sel = 0;
                    state = S_SETTINGS;
                    break;
                case 3: /* Exit */
                    sceKernelExitGame();
                    break;
                }
            }
            break;

        /* ---- RESULTS ---- */
        case S_RESULTS:
            ui_draw_results(&results, res_sel, res_scroll);
            if (pressed & PSP_CTRL_UP) {
                if (res_sel > 0) {
                    res_sel--;
                    if (res_sel < res_scroll) res_scroll = res_sel;
                }
            }
            if (pressed & PSP_CTRL_DOWN) {
                if (res_sel < results.count - 1) {
                    res_sel++;
                    if (res_sel >= res_scroll + 6) res_scroll++;
                }
            }
            if (pressed & PSP_CTRL_CROSS) {
                /* Play selected video */
                const VideoInfo *v = &results.items[res_sel];
                char stream_url[MAX_URL_LEN];
                ui_draw_message("Ottenimento URL stream...", COLOR_WHITE);
                if (youtube_get_stream_url(v->video_id, stream_url, sizeof(stream_url)) == 0) {
                    video_play(stream_url, v);
                } else {
                    ui_draw_message("Impossibile ottenere lo stream.", COLOR_RED);
                    sceKernelDelayThread(2000000);
                }
            }
            if (pressed & PSP_CTRL_CIRCLE) state = S_MENU;
            /* Pagination */
            if ((pressed & PSP_CTRL_RTRIGGER) && results.next_page_token[0]) {
                ui_draw_message("Pagina successiva...", COLOR_WHITE);
                youtube_search(search_query, results.next_page_token, &results);
                res_sel = 0; res_scroll = 0;
            }
            if ((pressed & PSP_CTRL_LTRIGGER) && results.prev_page_token[0]) {
                ui_draw_message("Pagina precedente...", COLOR_WHITE);
                youtube_search(search_query, results.prev_page_token, &results);
                res_sel = 0; res_scroll = 0;
            }
            break;

        /* ---- SETTINGS ---- */
        case S_SETTINGS:
            ui_draw_settings(set_sel);
            if (pressed & PSP_CTRL_UP)   { set_sel--; if (set_sel < 0) set_sel = SETTINGS_COUNT - 1; }
            if (pressed & PSP_CTRL_DOWN) { set_sel++; if (set_sel >= SETTINGS_COUNT) set_sel = 0; }
            if (pressed & PSP_CTRL_CROSS) {
                char tmp[64] = "";
                switch (set_sel) {
                case 0:
                    strncpy(tmp, g_config.proxy_host, sizeof(tmp));
                    ui_keyboard(tmp, sizeof(tmp), "Proxy host");
                    if (tmp[0]) strncpy(g_config.proxy_host, tmp, sizeof(g_config.proxy_host));
                    break;
                case 1:
                    snprintf(tmp, sizeof(tmp), "%d", g_config.proxy_port);
                    ui_keyboard(tmp, sizeof(tmp), "Proxy port");
                    if (tmp[0]) g_config.proxy_port = atoi(tmp);
                    break;
                case 2:
                    g_config.wifi_config++;
                    if (g_config.wifi_config > 3) g_config.wifi_config = 1;
                    break;
                case 3:
                    g_config.volume = (g_config.volume + 10) % 110;
                    break;
                case 4:
                    config_save();
                    state = S_MENU;
                    break;
                }
            }
            if (pressed & PSP_CTRL_CIRCLE) state = S_MENU;
            break;

        default:
            state = S_MENU;
            break;
        }

        sceKernelDelayThread(16000); /* ~60fps */
    }
}
