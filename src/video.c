#include "video.h"
#include "graphics.h"
#include "ui.h"
#include "config.h"
#include <pspmpeg.h>
#include <pspaudio.h>
#include <pspctrl.h>
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>

/*
 * PSP video playback via sceMpeg (Media Engine).
 * The proxy server must serve the video as MPEG-4 AVC Baseline
 * at 480x272, 29.97fps, AAC audio – the native PSP format.
 *
 * For simplicity we download to the Memory Stick first, then play.
 * A streaming approach would require ring-buffer management.
 */

#define RING_BUFFER_SIZE  (512 * 1024)
#define VIDEO_BUFFER_SIZE (2 * 1024 * 1024)
#define AUDIO_SAMPLES     512
#define TMP_FILE          "ms0:/PSP/GAME/PSPTube/tmp_video.mp4"

static SceMpegRingbuffer  g_ring;
static SceMpeg            g_mpeg;
static SceMpegStream     *g_vstream = NULL;
static SceMpegStream     *g_astream = NULL;

static u8 __attribute__((aligned(64))) g_ring_buf[RING_BUFFER_SIZE];
static u8 __attribute__((aligned(64))) g_mpeg_buf[SceMpegQueryMemSize];
static u8 __attribute__((aligned(64))) g_yuv_buf[480 * 272 * 3 / 2]; /* YCbCr 4:2:0 */
static s16 __attribute__((aligned(64))) g_pcm_buf[AUDIO_SAMPLES * 2];

/* --- Download helpers --- */
static int download_video(const char *url) {
    /* Uses http_get in chunks – for large files we stream to disk */
    #include "network.h"
    char chunk[8192];
    int template_id, conn_id, req_id, status, ret;

    template_id = sceHttpCreateTemplate("PSPTube/1.0", 1, 1);
    if (template_id < 0) return template_id;
    sceHttpSetRecvTimeOut(template_id, 30000000);

    conn_id = sceHttpCreateConnectionWithURL(template_id, url, 0);
    req_id  = sceHttpCreateRequestWithURL(conn_id, PSP_HTTP_METHOD_GET, url, 0);

    ret = sceHttpSendRequest(req_id, NULL, 0);
    if (ret < 0) goto dl_cleanup;

    sceHttpGetStatusCode(req_id, &status);
    if (status != 200) { ret = -1; goto dl_cleanup; }

    FILE *f = fopen(TMP_FILE, "wb");
    if (!f) { ret = -1; goto dl_cleanup; }

    int total = 0;
    while (1) {
        int n = sceHttpReadData(req_id, chunk, sizeof(chunk));
        if (n <= 0) break;
        fwrite(chunk, 1, n, f);
        total += n;
        /* Progress display */
        if (total % (64 * 1024) == 0) {
            graphics_clear(COLOR_PSPTUBE);
            ui_draw_header("PSPTube - Download");
            draw_text_center(120, COLOR_WHITE, "Download in corso...");
            draw_text_center(138, COLOR_GRAY,  "%d KB", total / 1024);
            /* Progress bar */
            draw_rect(40, 155, 400, 12, COLOR_GRAY);
            graphics_flip();
        }
    }
    fclose(f);
    ret = total;

dl_cleanup:
    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);
    sceHttpDeleteTemplate(template_id);
    return ret;
}

/* --- Ring buffer callback --- */
static int ring_callback(SceMpegRingbuffer *rb, void *data, int size, void *cb_data) {
    FILE *f = (FILE*)cb_data;
    return fread(data, 1, size, f);
}

int video_play(const char *url, const VideoInfo *info) {
    int ret;

    /* Download */
    ui_draw_message("Download video...", COLOR_WHITE);
    ret = download_video(url);
    if (ret < 0) {
        ui_draw_message("Download fallito.", COLOR_RED);
        sceKernelDelayThread(2000000);
        return ret;
    }

    /* Init MPEG library */
    ret = sceMpegInit();
    if (ret < 0) return ret;

    int buf_size = sceMpegQueryMemSize(0);
    if (buf_size > (int)sizeof(g_mpeg_buf)) buf_size = sizeof(g_mpeg_buf);

    ret = sceMpegCreate(&g_mpeg, g_mpeg_buf, buf_size, &g_ring, RING_BUFFER_SIZE, 0, 0);
    if (ret < 0) { sceMpegFinish(); return ret; }

    /* Open temp file */
    FILE *f = fopen(TMP_FILE, "rb");
    if (!f) { sceMpegDelete(&g_mpeg); sceMpegFinish(); return -1; }

    ret = sceMpegRingbufferConstruct(&g_ring, RING_BUFFER_SIZE / 2048, RING_BUFFER_SIZE,
                                     ring_callback, f);
    if (ret < 0) goto vp_cleanup;

    /* Read MPEG header */
    while (sceMpegRingbufferAvailableSize(&g_ring) > 0)
        sceMpegRingbufferPut(&g_ring, 4, 4);

    SceMpegAtracContext atrac;
    sceMpegQueryStreamOffset(&g_mpeg, g_ring_buf, &atrac);

    g_vstream = sceMpegQueryStreamSize(&g_mpeg, &atrac, SCE_MPEG_VIDEO_STREAM_ID);
    g_astream = sceMpegQueryStreamSize(&g_mpeg, &atrac, SCE_MPEG_ATRAC_STREAM_ID);

    /* Audio channel */
    int audio_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_SAMPLES,
                                     PSP_AUDIO_FORMAT_STEREO);

    SceCtrlData pad, old_pad;
    memset(&old_pad, 0, sizeof(old_pad));
    int paused = 0;
    int show_info = 1, info_timer = 180;

    /* Playback loop */
    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int pressed = pad.Buttons & ~old_pad.Buttons;
        old_pad = pad;

        if (pressed & PSP_CTRL_CIRCLE) break; /* back */
        if (pressed & PSP_CTRL_CROSS)  { paused ^= 1; }
        if (pressed & PSP_CTRL_START)  { show_info ^= 1; info_timer = 180; }

        if (paused) { sceKernelDelayThread(16000); continue; }

        /* Feed ring buffer */
        int avail = sceMpegRingbufferAvailableSize(&g_ring);
        if (avail > 0) sceMpegRingbufferPut(&g_ring, avail / 2048, avail / 2048);

        /* Decode video frame */
        SceMpegYCbCrBuffer ycbcr;
        ycbcr.iWidth  = 480;
        ycbcr.iHeight = 272;
        ycbcr.pYBuffer  = (void*)(((u32)g_yuv_buf + 63) & ~63);
        ycbcr.pCbBuffer = ycbcr.pYBuffer + 480 * 272;
        ycbcr.pCrBuffer = ycbcr.pCbBuffer + 240 * 136;

        ret = sceMpegDecodeVideoYCbCr(&g_mpeg, g_vstream, &ycbcr, 0);
        if (ret < 0) break;

        /* Convert YCbCr → RGB and blit to framebuffer (simplified) */
        /* In production use GE YCbCr-to-RGB texture conversion */
        sceMpegDecodeVideoYCbCrAbort(&g_mpeg, &ycbcr);

        /* Decode audio */
        if (g_astream) {
            sceMpegDecodeAtrac(&g_mpeg, g_astream, g_pcm_buf, 0);
            sceAudioOutputPannedBlocking(audio_ch,
                g_config.volume * PSP_AUDIO_VOLUME_MAX / 100,
                g_config.volume * PSP_AUDIO_VOLUME_MAX / 100,
                g_pcm_buf);
        }

        /* Overlay */
        if (show_info) {
            ui_draw_video_info(info);
            if (--info_timer <= 0) show_info = 0;
        }

        if (pressed & PSP_CTRL_CIRCLE) break;

        graphics_flip();
        sceDisplayWaitVblankStart();
    }

    if (audio_ch >= 0) sceAudioChRelease(audio_ch);

vp_cleanup:
    fclose(f);
    sceMpegRingbufferDestruct(&g_ring);
    sceMpegDelete(&g_mpeg);
    sceMpegFinish();

    /* Clean up temp file */
    sceIoRemove(TMP_FILE);
    return 0;
}
