#include "video.h"
#include "graphics.h"
#include "ui.h"
#include "config.h"
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspmpeg.h>
#include <pspaudio.h>
#include <psphttp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RING_BUFFER_SIZE   (512 * 1024)
#define RING_PACKETS       (RING_BUFFER_SIZE / 2048)
#define MPEG_BUF_SIZE      (64 * 1024)
#define AUDIO_SAMPLES      512
#define TMP_FILE           "ms0:/PSP/GAME/PSPTube/tmp_video.mp4"

static u8 __attribute__((aligned(64))) g_ring_data[RING_BUFFER_SIZE];
static u8 __attribute__((aligned(64))) g_mpeg_buf[MPEG_BUF_SIZE];

/* Ring buffer callback: called by sceMpeg to fill packets from file */
static SceInt32 ring_callback(ScePVoid pData, SceInt32 iNumPackets, ScePVoid pParam) {
    FILE *f = (FILE*)pParam;
    int bytes = iNumPackets * 2048;
    int n = fread(pData, 1, bytes, f);
    return n / 2048;
}

/* Download video to Memory Stick */
static int download_video(const char *url) {
    char chunk[8192];
    int template_id, conn_id, req_id, status, ret;

    template_id = sceHttpCreateTemplate("PSPTube/1.0", 1, 1);
    if (template_id < 0) return template_id;
    sceHttpSetRecvTimeOut(template_id, 30000000);

    conn_id = sceHttpCreateConnectionWithURL(template_id, (char*)url, 0);
    if (conn_id < 0) { sceHttpDeleteTemplate(template_id); return conn_id; }

    req_id = sceHttpCreateRequestWithURL(conn_id, PSP_HTTP_METHOD_GET, (char*)url, 0);
    if (req_id < 0) { sceHttpDeleteConnection(conn_id); sceHttpDeleteTemplate(template_id); return req_id; }

    ret = sceHttpSendRequest(req_id, NULL, 0);
    if (ret < 0) goto dl_cleanup;

    sceHttpGetStatusCode(req_id, &status);
    if (status != 200) { ret = -1; goto dl_cleanup; }

    {
        FILE *f = fopen(TMP_FILE, "wb");
        if (!f) { ret = -1; goto dl_cleanup; }

        int total = 0;
        while (1) {
            int n = sceHttpReadData(req_id, chunk, sizeof(chunk));
            if (n <= 0) break;
            fwrite(chunk, 1, n, f);
            total += n;

            if ((total % (64 * 1024)) == 0) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Download: %d KB", total / 1024);
                graphics_clear(COLOR_PSPTUBE);
                ui_draw_header("PSPTube - Download");
                draw_text_center(120, COLOR_WHITE, msg);
                draw_rect(40, 150, 400, 12, COLOR_GRAY);
                graphics_flip();
            }
        }
        fclose(f);
        ret = total;
    }

dl_cleanup:
    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);
    sceHttpDeleteTemplate(template_id);
    return ret;
}

int video_play(const char *url, const VideoInfo *info) {
    int ret;

    /* Step 1: download */
    ui_draw_message("Download video...", COLOR_WHITE);
    ret = download_video(url);
    if (ret <= 0) {
        ui_draw_message("Download fallito.", COLOR_RED);
        sceKernelDelayThread(2000000);
        return -1;
    }

    /* Step 2: open file */
    FILE *f = fopen(TMP_FILE, "rb");
    if (!f) {
        ui_draw_message("Impossibile aprire il file.", COLOR_RED);
        sceKernelDelayThread(2000000);
        return -1;
    }

    /* Step 3: init MPEG */
    ret = sceMpegInit();
    if (ret < 0) { fclose(f); return ret; }

    SceMpegRingbuffer ringbuf;
    ret = sceMpegRingbufferConstruct(&ringbuf, RING_PACKETS,
                                     g_ring_data, RING_BUFFER_SIZE,
                                     ring_callback, f);
    if (ret < 0) { sceMpegFinish(); fclose(f); return ret; }

    SceMpeg mpeg;
    memset(g_mpeg_buf, 0, sizeof(g_mpeg_buf));
    ret = sceMpegCreate(&mpeg, g_mpeg_buf, MPEG_BUF_SIZE, &ringbuf, RING_PACKETS, 0, 0);
    if (ret < 0) {
        sceMpegRingbufferDestruct(&ringbuf);
        sceMpegFinish();
        fclose(f);
        return ret;
    }

    /* Feed initial data */
    {
        int avail = sceMpegRingbufferAvailableSize(&ringbuf);
        if (avail > 0) sceMpegRingbufferPut(&ringbuf, avail, avail);
    }

    /* Get stream info */
    SceInt32 stream_offset = 0;
    sceMpegQueryStreamOffset(&mpeg, g_ring_data, &stream_offset);

    SceMpegStream *vstream = sceMpegGetAtracEsStream(&mpeg);  /* video */
    SceMpegStream *astream = sceMpegGetAvcEsStream(&mpeg);    /* audio */

    /* Audio */
    int audio_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_SAMPLES,
                                     PSP_AUDIO_FORMAT_STEREO);

    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));
    int paused = 0, show_info = 1, info_timer = 180;

    ui_draw_message("Riproduzione...", COLOR_WHITE);

    /* Playback loop */
    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int pressed = pad.Buttons & ~old_pad.Buttons;
        old_pad = pad;

        if (pressed & PSP_CTRL_CIRCLE) break;
        if (pressed & PSP_CTRL_CROSS)  paused ^= 1;
        if (pressed & PSP_CTRL_START)  { show_info ^= 1; info_timer = 180; }

        if (paused) { sceKernelDelayThread(16000); continue; }

        /* Refill ring buffer */
        {
            int avail = sceMpegRingbufferAvailableSize(&ringbuf);
            if (avail > 0) sceMpegRingbufferPut(&ringbuf, avail, avail);
        }

        /* Decode & display one AVC frame */
        if (vstream) {
            SceMpegAu au;
            ret = sceMpegGetAvcAu(&mpeg, vstream, &au, NULL);
            if (ret < 0) break;

            /* Decode into display buffer directly */
            void *yuv_buf = NULL;
            ret = sceMpegAvcDecodeMode(&mpeg, NULL);
            ret = sceMpegAvcDecode(&mpeg, &au, 512, NULL, &yuv_buf);
            if (ret < 0) break;

            sceMpegAvcDecodeFlush(&mpeg);
        }

        /* Decode audio */
        if (astream && audio_ch >= 0) {
            static s16 __attribute__((aligned(64))) pcm[AUDIO_SAMPLES * 2];
            SceMpegAu au;
            ret = sceMpegGetAtracAu(&mpeg, astream, &au, NULL);
            if (ret == 0) {
                sceMpegAtracDecode(&mpeg, &au, pcm, 0);
                sceAudioOutputPannedBlocking(audio_ch,
                    g_config.volume * PSP_AUDIO_VOLUME_MAX / 100,
                    g_config.volume * PSP_AUDIO_VOLUME_MAX / 100,
                    pcm);
            }
        }

        if (show_info) {
            ui_draw_video_info(info);
            if (--info_timer <= 0) show_info = 0;
        }

        graphics_flip();
        sceDisplayWaitVblankStart();
    }

    /* Cleanup */
    if (audio_ch >= 0) sceAudioChRelease(audio_ch);
    sceMpegDelete(&mpeg);
    sceMpegRingbufferDestruct(&ringbuf);
    sceMpegFinish();
    fclose(f);
    sceIoRemove(TMP_FILE);

    return 0;
}
