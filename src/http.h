#ifndef __HTTP_H__
#define __HTTP_H__

#include <gst/gst.h>

void http_init (GstRTSPMediaTable *media_table, const gchar* host, const gchar *port);

#endif
