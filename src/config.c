#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

AppConfig g_config = {
    DEFAULT_PROXY_HOST,
    DEFAULT_PROXY_PORT,
    1,   /* wifi slot */
    80   /* volume */
};

void config_load(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[64];
        if (sscanf(line, "%63[^=]=%63s", key, val) != 2) continue;
        if (strcmp(key, "proxy_host") == 0)
            strncpy(g_config.proxy_host, val, sizeof(g_config.proxy_host) - 1);
        else if (strcmp(key, "proxy_port") == 0)
            g_config.proxy_port = atoi(val);
        else if (strcmp(key, "wifi_config") == 0)
            g_config.wifi_config = atoi(val);
        else if (strcmp(key, "volume") == 0)
            g_config.volume = atoi(val);
    }
    fclose(f);
}

void config_save(void) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) return;
    fprintf(f, "proxy_host=%s\n", g_config.proxy_host);
    fprintf(f, "proxy_port=%d\n", g_config.proxy_port);
    fprintf(f, "wifi_config=%d\n", g_config.wifi_config);
    fprintf(f, "volume=%d\n",      g_config.volume);
    fclose(f);
}
