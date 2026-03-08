#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_FILE "ms0:/PSP/GAME/PSPTube/config.txt"
#define DEFAULT_PROXY_HOST "192.168.1.100"
#define DEFAULT_PROXY_PORT 8080

typedef struct {
    char proxy_host[64];
    int  proxy_port;
    int  wifi_config;   /* 1-3: PSP wifi config slot */
    int  volume;        /* 0-100 */
} AppConfig;

extern AppConfig g_config;

void config_load(void);
void config_save(void);

#endif /* CONFIG_H */
