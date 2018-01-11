#ifndef __CONSUL_H__
#define __CONSUL_H__

#include "rtsp.h"

void consul_init (GstRTSPMediaTable *media_table, const gchar* host, const gchar *port);

void consul_add (const gchar *port);
void consul_del (const gchar *port);

#endif
