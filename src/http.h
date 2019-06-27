#ifndef __HTTP_H__
#define __HTTP_H__

#include "rtsp.h"

void http_init (GstRTSPMediaTable *media_table, const gchar* host, const gchar *port);

#endif
