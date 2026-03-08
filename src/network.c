#include "network.h"
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <psputility.h>
#include <psphttp.h>
#include <pspssl.h>
#include <pspwlan.h>
#include <stdio.h>
#include <string.h>

static int g_net_init = 0;
static int g_connected = 0;

/* ---- Debug log ---- */
#define LOG_FILE "ms0:/PSP/GAME/PSPTube/debug.log"

static FILE *g_log = NULL;

static void log_open(void) {
    g_log = fopen(LOG_FILE, "w");
}

static void log_close(void) {
    if (g_log) { fclose(g_log); g_log = NULL; }
}

static void log_write(const char *msg, int code) {
    if (!g_log) return;
    fprintf(g_log, "%s -> 0x%08X (%d)\n", msg, (unsigned int)code, code);
    fflush(g_log);
}

int network_init(void) {
    int ret;

    log_open();
    log_write("=== network_init START ===", 0);

    /* Switch WLAN fisico */
    int wlan = sceWlanGetSwitchState();
    log_write("sceWlanGetSwitchState", wlan);
    if (wlan == 0) {
        log_write("ERRORE: switch WLAN spento", -1);
        log_close();
        return -0xDEAD;
    }

    /* Carica moduli rete - solo quelli necessari */
    ret = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    log_write("LoadNetModule COMMON", ret);
    if (ret < 0 && ret != (int)0x80110902) { log_close(); return ret; }

    ret = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    log_write("LoadNetModule INET", ret);
    if (ret < 0 && ret != (int)0x80110902) { log_close(); return ret; }

    /* PARSEURI e PARSEHTTP non esistono su tutti i CFW, li saltiamo */
    sceUtilityLoadNetModule(PSP_NET_MODULE_PARSEURI);
    sceUtilityLoadNetModule(PSP_NET_MODULE_PARSEHTTP);

    ret = sceUtilityLoadNetModule(PSP_NET_MODULE_HTTP);
    log_write("LoadNetModule HTTP", ret);
    if (ret < 0 && ret != (int)0x80110902) { log_close(); return ret; }

    sceUtilityLoadNetModule(PSP_NET_MODULE_SSL);
    log_write("LoadNetModule SSL (opzionale)", 0);
    /* SSL puo non essere disponibile su tutti i CFW, continuiamo comunque */

    /* Stack di rete */
    ret = sceNetInit(128 * 1024, 42, 4096, 42, 4096);
    log_write("sceNetInit", ret);
    if (ret < 0) { log_close(); return ret; }

    ret = sceNetInetInit();
    log_write("sceNetInetInit", ret);
    if (ret < 0) { sceNetTerm(); log_close(); return ret; }

    ret = sceNetApctlInit(0x8000, 48);
    log_write("sceNetApctlInit", ret);
    if (ret < 0) { sceNetInetTerm(); sceNetTerm(); log_close(); return ret; }

    ret = sceHttpInit(256 * 1024);
    log_write("sceHttpInit", ret);
    if (ret < 0) { log_close(); return ret; }

    ret = sceSslInit(0x28000);
    log_write("sceSslInit", ret);
    /* non fatale se SSL fallisce */

    log_write("=== network_init OK ===", 0);
    g_net_init = 1;
    log_close();
    return 0;
}

void network_shutdown(void) {
    if (!g_net_init) return;
    if (g_connected) sceNetApctlDisconnect();
    sceSslEnd();
    sceHttpEnd();
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    sceUtilityUnloadNetModule(PSP_NET_MODULE_SSL);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_HTTP);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_PARSEHTTP);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_PARSEURI);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    g_net_init = 0;
    g_connected = 0;
}

int network_connect(void) {
    int slot, ret, state, timeout;

    log_open();
    log_write("=== network_connect START ===", 0);

    for (slot = 1; slot <= 3; slot++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "sceNetApctlConnect slot %d", slot);
        ret = sceNetApctlConnect(slot);
        log_write(msg, ret);
        if (ret < 0) continue;

        timeout = 0;
        while (timeout < 60) {
            ret = sceNetApctlGetState(&state);
            if (ret < 0) {
                log_write("sceNetApctlGetState error", ret);
                break;
            }
            snprintf(msg, sizeof(msg), "  stato slot %d timeout %d", slot, timeout);
            log_write(msg, state);
            if (state == PSP_NET_APCTL_STATE_GOT_IP) {
                log_write("=== CONNESSO! ===", slot);
                g_connected = 1;
                log_close();
                return slot;
            }
            sceKernelDelayThread(500000);
            timeout++;
        }
        sceNetApctlDisconnect();
        sceKernelDelayThread(500000);
    }

    log_write("=== TUTTI GLI SLOT FALLITI ===", -1);
    log_close();
    return -1;
}

int network_disconnect(void) {
    sceNetApctlDisconnect();
    g_connected = 0;
    return 0;
}

int network_is_connected(void) {
    return g_connected;
}

int http_get(const char *url, char *buf, int buf_size) {
    int template_id, conn_id, req_id;
    int ret, status, total = 0;

    template_id = sceHttpCreateTemplate("PSPTube/1.0", 1, 1);
    if (template_id < 0) return template_id;

    sceHttpSetResolveTimeOut(template_id, 5000000);
    sceHttpSetRecvTimeOut(template_id, 10000000);
    sceHttpSetSendTimeOut(template_id, 10000000);

    conn_id = sceHttpCreateConnectionWithURL(template_id, (char*)url, 0);
    if (conn_id < 0) {
        sceHttpDeleteTemplate(template_id);
        return conn_id;
    }

    req_id = sceHttpCreateRequestWithURL(conn_id, PSP_HTTP_METHOD_GET, (char*)url, 0);
    if (req_id < 0) {
        sceHttpDeleteConnection(conn_id);
        sceHttpDeleteTemplate(template_id);
        return req_id;
    }

    ret = sceHttpSendRequest(req_id, NULL, 0);
    if (ret < 0) goto cleanup;

    ret = sceHttpGetStatusCode(req_id, &status);
    if (ret < 0 || status != 200) { ret = -status; goto cleanup; }

    while (total < buf_size - 1) {
        int n = sceHttpReadData(req_id, buf + total, buf_size - total - 1);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';
    ret = total;

cleanup:
    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);
    sceHttpDeleteTemplate(template_id);
    return ret;
}

void url_encode(const char *src, char *dst, int dst_size) {
    static const char hex[] = "0123456789ABCDEF";
    int i = 0;
    while (*src && i < dst_size - 4) {
        unsigned char c = (unsigned char)*src++;
        if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
            (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') {
            dst[i++] = c;
        } else {
            dst[i++] = '%';
            dst[i++] = hex[c >> 4];
            dst[i++] = hex[c & 0xF];
        }
    }
    dst[i] = '\0';
}
