#ifndef VIDEO_H
#define VIDEO_H

#include "youtube.h"

/* Play a video from a URL (MPEG-4/AVC stream) */
int video_play(const char *url, const VideoInfo *info);

#endif /* VIDEO_H */
