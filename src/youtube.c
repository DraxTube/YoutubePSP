#include "youtube.h"
#include "network.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * PSPTube communicates with a lightweight proxy server that:
 *   - Wraps the YouTube Data API v3
 *   - Transcodes video to PSP-compatible format on demand
 *
 * Set PROXY_HOST in config.h / config file to your server address.
 *
 * Proxy endpoints:
 *   GET /search?q=<query>&page=<token>    → JSON list
 *   GET /trending                         → JSON list
 *   GET /stream?id=<video_id>             → redirect to MP4 stream
 *
 * JSON format (simple, hand-parsed – no heap alloc needed):
 *   {"items":[{"id":"...","title":"...","channel":"...","dur":"...","views":"..."},...],
 *    "next":"...","prev":"..."}
 */

static char g_buf[64 * 1024];

/* ---- Minimal JSON value extractor ---- */
static const char *json_str(const char *json, const char *key, char *out, int out_size) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p) { out[0] = '\0'; return NULL; }
    p += strlen(search);
    int i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\') { p++; if (*p) out[i++] = *p++; }
        else out[i++] = *p++;
    }
    out[i] = '\0';
    return p;
}

/* Parse a JSON items array from the proxy response */
static int parse_results(const char *json, SearchResults *out) {
    memset(out, 0, sizeof(*out));
    json_str(json, "next", out->next_page_token, sizeof(out->next_page_token));
    json_str(json, "prev", out->prev_page_token, sizeof(out->prev_page_token));

    const char *p = strstr(json, "\"items\":[");
    if (!p) return 0;
    p += 9;

    while (*p && out->count < MAX_RESULTS) {
        p = strchr(p, '{');
        if (!p) break;
        VideoInfo *v = &out->items[out->count];
        json_str(p, "id",      v->video_id,   sizeof(v->video_id));
        json_str(p, "title",   v->title,       sizeof(v->title));
        json_str(p, "channel", v->channel,     sizeof(v->channel));
        json_str(p, "dur",     v->duration,    sizeof(v->duration));
        json_str(p, "views",   v->view_count,  sizeof(v->view_count));
        if (v->video_id[0]) out->count++;
        p = strchr(p, '}');
        if (!p) break;
        p++;
    }
    return out->count;
}

int youtube_search(const char *query, const char *page_token, SearchResults *out) {
    char enc_query[256];
    char url[MAX_URL_LEN];

    url_encode(query, enc_query, sizeof(enc_query));

    if (page_token && page_token[0])
        snprintf(url, sizeof(url), "http://%s:%d/search?q=%s&page=%s",
                 g_config.proxy_host, g_config.proxy_port, enc_query, page_token);
    else
        snprintf(url, sizeof(url), "http://%s:%d/search?q=%s",
                 g_config.proxy_host, g_config.proxy_port, enc_query);

    int ret = http_get(url, g_buf, sizeof(g_buf));
    if (ret < 0) return ret;

    return parse_results(g_buf, out);
}

int youtube_trending(SearchResults *out) {
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "http://%s:%d/trending",
             g_config.proxy_host, g_config.proxy_port);

    int ret = http_get(url, g_buf, sizeof(g_buf));
    if (ret < 0) return ret;

    return parse_results(g_buf, out);
}

int youtube_get_stream_url(const char *video_id, char *url_out, int url_size) {
    char req_url[MAX_URL_LEN];
    snprintf(req_url, sizeof(req_url), "http://%s:%d/stream?id=%s",
             g_config.proxy_host, g_config.proxy_port, video_id);

    int ret = http_get(req_url, g_buf, sizeof(g_buf));
    if (ret < 0) return ret;

    /* Proxy returns: {"url":"http://..."} */
    json_str(g_buf, "url", url_out, url_size);
    return url_out[0] ? 0 : -1;
}
