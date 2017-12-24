#ifndef __PROMETHEUS_H__
#define __PROMETHEUS_H__

#include "rtsp.h"

void prometheus_init (GstRTSPMediaTable *media_table, const gchar* host, const gchar *port);

#endif
