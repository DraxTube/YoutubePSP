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

#define RING_BUFFER_SIZE  (512 * 1024)
#define RING_PACKETS      (RING_BUFFER_SIZE / 2048)
#define MPEG_BUF_SIZE     (64 * 1024)
#define AUDIO_SAMPLES     512
#define TMP_FILE          "ms0:/PSP/GAME/PSPTube/tmp_video.mp4"

/* Framebuffer per il video: 480x272 ABGR */
static u32 __attribute__((aligned(64))) g_frame_buf[512 * 272];
static u8  __attribute__((aligned(64))) g_ring_data[RING_BUFFER_SIZE];
static u8  __attribute__((aligned(64))) g_mpeg_buf[MPEG_BUF_SIZE];

/* Ring buffer callback */
static SceInt32 ring_callback(ScePVoid pData, SceInt32 iNumPackets, ScePVoid pParam) {
    FILE *f = (FILE*)pParam;
    int n = fread(pData, 2048, iNumPackets, f);
    return (SceInt32)n;
}

/* Download su Memory Stick */
static int download_video(const char *url) {
    char chunk[8192];
    int template_id, conn_id, req_id, status;
    int ret = -1;

    template_id = sceHttpCreateTemplate("PSPTube/1.0", 1, 1);
    if (template_id < 0) return template_id;
    sceHttpSetRecvTimeOut(template_id, 30000000);

    conn_id = sceHttpCreateConnectionWithURL(template_id, (char*)url, 0);
    if (conn_id < 0) { sceHttpDeleteTemplate(template_id); return conn_id; }

    req_id = sceHttpCreateRequestWithURL(conn_id, PSP_HTTP_METHOD_GET, (char*)url, 0);
    if (req_id < 0) {
        sceHttpDeleteConnection(conn_id);
        sceHttpDeleteTemplate(template_id);
        return req_id;
    }

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

            if ((total & 0xFFFF) == 0) {   /* ogni ~64KB */
                char msg[48];
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

    /* 1. Download */
    ui_draw_message("Download video...", COLOR_WHITE);
    ret = download_video(url);
    if (ret <= 0) {
        ui_draw_message("Download fallito.", COLOR_RED);
        sceKernelDelayThread(2000000);
        return -1;
    }

    /* 2. Apri file */
    FILE *f = fopen(TMP_FILE, "rb");
    if (!f) {
        ui_draw_message("Impossibile aprire il file.", COLOR_RED);
        sceKernelDelayThread(2000000);
        return -1;
    }

    /* 3. Init MPEG */
    ret = sceMpegInit();
    if (ret < 0) { fclose(f); return ret; }

    SceMpegRingbuffer ringbuf;
    ret = sceMpegRingbufferConstruct(&ringbuf, RING_PACKETS,
                                     (ScePVoid)g_ring_data, RING_BUFFER_SIZE,
                                     ring_callback, (ScePVoid)f);
    if (ret < 0) { sceMpegFinish(); fclose(f); return ret; }

    SceMpeg mpeg;
    memset(g_mpeg_buf, 0, MPEG_BUF_SIZE);
    ret = sceMpegCreate(&mpeg, g_mpeg_buf, MPEG_BUF_SIZE,
                        &ringbuf, RING_PACKETS, 0, 0);
    if (ret < 0) {
        sceMpegRingbufferDestruct(&ringbuf);
        sceMpegFinish();
        fclose(f);
        return ret;
    }

    /* 4. Riempi ring buffer iniziale */
    {
        int avail = sceMpegRingbufferAvailableSize(&ringbuf);
        if (avail > 0) sceMpegRingbufferPut(&ringbuf, avail, avail);
    }

    /* 5. Registra stream video (AVC=0xe0) e audio (ATRAC=0) */
    SceMpegStream *vstream = sceMpegRegistStream(&mpeg, 0xe0, 0);
    SceMpegStream *astream = sceMpegRegistStream(&mpeg, 0,    0);

    /* 6. Audio */
    int audio_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_SAMPLES,
                                     PSP_AUDIO_FORMAT_STEREO);

    static s16 __attribute__((aligned(64))) pcm[AUDIO_SAMPLES * 2];

    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));
    int paused = 0, show_info = 1, info_timer = 180;

    /* 7. Loop di riproduzione */
    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int pressed = pad.Buttons & ~old_pad.Buttons;
        old_pad = pad;

        if (pressed & PSP_CTRL_CIRCLE) break;
        if (pressed & PSP_CTRL_CROSS)  paused ^= 1;
        if (pressed & PSP_CTRL_START)  { show_info ^= 1; info_timer = 180; }

        if (paused) { sceKernelDelayThread(16000); continue; }

        /* Riempi ring buffer */
        {
            int avail = sceMpegRingbufferAvailableSize(&ringbuf);
            if (avail > 0) sceMpegRingbufferPut(&ringbuf, avail, avail);
        }

        /* Decode video */
        if (vstream) {
            SceMpegAu au;
            au.iPts      = 0;
            au.iDts      = 0;
            au.iEsBuffer = 0;
            au.iAuSize   = 0;

            ret = sceMpegGetAvcAu(&mpeg, vstream, &au, NULL);
            if (ret == 0) {
                SceInt32 init = 0;
                sceMpegAvcDecode(&mpeg, &au, 512,
                                 (ScePVoid)g_frame_buf, &init);
                if (init) {
                    /* Blit g_frame_buf → display tramite GU/display diretto */
                    sceDisplaySetFrameBuf((void*)g_frame_buf, 512,
                                         PSP_DISPLAY_PIXEL_FORMAT_8888,
                                         PSP_DISPLAY_SETBUF_NEXTFRAME);
                }
            } else if (ret < 0) {
                break; /* fine stream */
            }
        }

        /* Decode audio */
        if (astream && audio_ch >= 0) {
            SceMpegAu au;
            au.iPts      = 0;
            au.iDts      = 0;
            au.iEsBuffer = 0;
            au.iAuSize   = 0;

            ret = sceMpegGetAtracAu(&mpeg, astream, &au, NULL);
            if (ret == 0) {
                sceMpegAtracDecode(&mpeg, &au, (ScePVoid)pcm, 0);
                sceAudioOutputPannedBlocking(
                    audio_ch,
                    g_config.volume * PSP_AUDIO_VOLUME_MAX / 100,
                    g_config.volume * PSP_AUDIO_VOLUME_MAX / 100,
                    pcm);
            }
        }

        /* Info overlay */
        if (show_info) {
            ui_draw_video_info(info);
            if (--info_timer <= 0) show_info = 0;
        }

        sceDisplayWaitVblankStart();
    }

    /* 8. Cleanup */
    if (vstream) sceMpegUnRegistStream(&mpeg, vstream);
    if (astream) sceMpegUnRegistStream(&mpeg, astream);
    if (audio_ch >= 0) sceAudioChRelease(audio_ch);
    sceMpegAvcDecodeStop(&mpeg, 512, (ScePVoid)g_frame_buf, NULL);
    sceMpegDelete(&mpeg);
    sceMpegRingbufferDestruct(&ringbuf);
    sceMpegFinish();
    fclose(f);
    sceIoRemove(TMP_FILE);

    return 0;
}
