#include "network.h"
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <psputility.h>
#include <psphttp.h>
#include <pspssl.h>
#include <stdio.h>
#include <string.h>

static int g_net_init = 0;
static int g_connected = 0;

int network_init(void) {
    int ret;

    ret = sceNetInit(128 * 1024, 42, 4096, 42, 4096);
    if (ret < 0) return ret;

    ret = sceNetInetInit();
    if (ret < 0) return ret;

    ret = sceNetApctlInit(0x8000, 48);
    if (ret < 0) return ret;

    ret = sceHttpInit(256 * 1024);
    if (ret < 0) return ret;

    ret = sceSslInit(0x28000);
    if (ret < 0) return ret;

    g_net_init = 1;
    return 0;
}

void network_shutdown(void) {
    if (!g_net_init) return;
    sceSslTerm();
    sceHttpTerm();
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    g_net_init = 0;
    g_connected = 0;
}

/* Connect using the first saved WiFi configuration */
int network_connect(void) {
    int ret, state;
    int timeout = 0;

    /* Try connection index 1 first (can be changed in config) */
    ret = sceNetApctlConnect(1);
    if (ret < 0) return ret;

    while (timeout < 30) {
        ret = sceNetApctlGetState(&state);
        if (ret < 0) break;
        if (state == PSP_NET_APCTL_STATE_GOT_IP) {
            g_connected = 1;
            return 0;
        }
        sceKernelDelayThread(500000); /* 0.5s */
        timeout++;
    }
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

    sceHttpSetResolveTimeOut(template_id, 3000000);
    sceHttpSetRecvTimeOut(template_id, 10000000);
    sceHttpSetSendTimeOut(template_id, 10000000);

    conn_id = sceHttpCreateConnectionWithURL(template_id, url, 0);
    if (conn_id < 0) {
        sceHttpDeleteTemplate(template_id);
        return conn_id;
    }

    req_id = sceHttpCreateRequestWithURL(conn_id, PSP_HTTP_METHOD_GET, url, 0);
    if (req_id < 0) {
        sceHttpDeleteConnection(conn_id);
        sceHttpDeleteTemplate(template_id);
        return req_id;
    }

    ret = sceHttpSendRequest(req_id, NULL, 0);
    if (ret < 0) goto cleanup;

    ret = sceHttpGetStatusCode(req_id, &status);
    if (ret < 0 || status != 200) { ret = -status; goto cleanup; }

    /* Read response */
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
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[i++] = c;
        } else {
            dst[i++] = '%';
            dst[i++] = hex[c >> 4];
            dst[i++] = hex[c & 0xF];
        }
    }
    dst[i] = '\0';
}
