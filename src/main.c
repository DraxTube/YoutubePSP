#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <psputility.h>
#include <psphttp.h>
#include <pspaudio.h>
#include <pspmpeg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graphics.h"
#include "network.h"
#include "ui.h"
#include "video.h"
#include "config.h"

PSP_MODULE_INFO("PSPTube", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(20480);

/* ---- Exit callback ---- */
static int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int tid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (tid >= 0) sceKernelStartThread(tid, 0, 0);
}

/* ---- App states ---- */
typedef enum {
    STATE_SPLASH,
    STATE_WIFI_CONNECT,
    STATE_MAIN_MENU,
    STATE_SEARCH,
    STATE_RESULTS,
    STATE_PLAYER,
    STATE_SETTINGS
} AppState;

AppState g_state = STATE_SPLASH;

/* ---- Main ---- */
int main(void) {
    setup_callbacks();

    /* Init graphics */
    graphics_init();

    /* Show splash screen */
    ui_draw_splash();
    sceDisplayWaitVblankStart();
    sceKernelDelayThread(2000000); /* 2 seconds */

    /* Connect to WiFi */
    g_state = STATE_WIFI_CONNECT;
    ui_draw_message("Connessione WiFi...", COLOR_WHITE);

    if (network_init() < 0) {
        ui_draw_message("WiFi non disponibile.\nControlla le impostazioni.", COLOR_RED);
        sceKernelDelayThread(3000000);
        sceKernelExitGame();
        return 0;
    }

    if (network_connect() < 0) {
        ui_draw_message("Impossibile connettersi.\nVerifica la rete WiFi.", COLOR_RED);
        sceKernelDelayThread(3000000);
        network_shutdown();
        sceKernelExitGame();
        return 0;
    }

    /* Load config */
    config_load();

    /* Main application loop */
    g_state = STATE_MAIN_MENU;
    app_run();

    /* Cleanup */
    network_shutdown();
    graphics_shutdown();

    return 0;
}
