#ifndef YOUTUBE_H
#define YOUTUBE_H

#define MAX_RESULTS     10
#define MAX_TITLE_LEN   80
#define MAX_ID_LEN      16
#define MAX_CHANNEL_LEN 48
#define MAX_DURATION_LEN 12

typedef struct {
    char video_id[MAX_ID_LEN];
    char title[MAX_TITLE_LEN];
    char channel[MAX_CHANNEL_LEN];
    char duration[MAX_DURATION_LEN];
    char view_count[16];
} VideoInfo;

typedef struct {
    VideoInfo items[MAX_RESULTS];
    int       count;
    char      next_page_token[32];
    char      prev_page_token[32];
} SearchResults;

/* Search YouTube via proxy server */
int  youtube_search(const char *query, const char *page_token, SearchResults *out);

/* Get direct stream URL for a video via proxy */
int  youtube_get_stream_url(const char *video_id, char *url_out, int url_size);

/* Trending / home feed */
int  youtube_trending(SearchResults *out);

#endif /* YOUTUBE_H */
