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
    int tid = sceKernelCreateThread("update_thread", callback_thread,
                                    0x11, 0xFA0, 0, 0);
    if (tid >= 0) sceKernelStartThread(tid, 0, 0);
}

int main(void) {
    setup_callbacks();
    graphics_init();

    /* Splash */
    ui_draw_splash();
    sceKernelDelayThread(2000000);

    /* Check switch WLAN */
    ui_draw_message("Connessione WiFi in corso...", COLOR_WHITE);

    int ret = network_init();
    if (ret == -0xDEAD) {
        ui_draw_message(
            "Switch WLAN spento!\n"
            "Attiva lo switch WiFi\n"
            "sul lato sinistro della PSP.\n"
            "\nPremi O per uscire.",
            COLOR_RED);
        SceCtrlData pad;
        while (1) {
            sceCtrlReadBufferPositive(&pad, 1);
            if (pad.Buttons & PSP_CTRL_CIRCLE) break;
            sceKernelDelayThread(16000);
        }
        sceKernelExitGame();
        return 0;
    }
    if (ret < 0) {
        ui_draw_message("Errore init rete.\nPremi O per uscire.", COLOR_RED);
        SceCtrlData pad;
        while (1) {
            sceCtrlReadBufferPositive(&pad, 1);
            if (pad.Buttons & PSP_CTRL_CIRCLE) break;
            sceKernelDelayThread(16000);
        }
        sceKernelExitGame();
        return 0;
    }

    ui_draw_message("Connessione WiFi...\n(provo slot 1, 2, 3)", COLOR_WHITE);
    ret = network_connect();
    if (ret < 0) {
        ui_draw_message(
            "WiFi non disponibile.\n"
            "Verifica che l'hotspot\n"
            "sia acceso e salvato\n"
            "nelle impostazioni PSP.\n"
            "\nPremi O per uscire.",
            COLOR_RED);
        SceCtrlData pad;
        while (1) {
            sceCtrlReadBufferPositive(&pad, 1);
            if (pad.Buttons & PSP_CTRL_CIRCLE) break;
            sceKernelDelayThread(16000);
        }
        network_shutdown();
        sceKernelExitGame();
        return 0;
    }

    config_load();
    app_run();

    network_shutdown();
    graphics_shutdown();
    return 0;
}
