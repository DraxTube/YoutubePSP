#ifndef NETWORK_H
#define NETWORK_H

#include <psptypes.h>

#define HTTP_BUF_SIZE   (64 * 1024)
#define MAX_URL_LEN     512

int  network_init(void);
void network_shutdown(void);
int  network_connect(void);
int  network_disconnect(void);
int  network_is_connected(void);

/* HTTP GET – fills buf, returns bytes read or <0 on error */
int  http_get(const char *url, char *buf, int buf_size);

/* URL encode a string into dst */
void url_encode(const char *src, char *dst, int dst_size);

#endif /* NETWORK_H */
