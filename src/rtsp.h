#ifndef __RTSP_H__
#define __RTSP_H__

#include <glib.h>
#include <glib/gprintf.h>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <json-glib/json-glib.h>

typedef GHashTable GstRTSPMediaTable;

GstRTSPMediaTable *rtsp_media_table_new ();
void rtsp_media_table_free (GstRTSPMediaTable *media_table);

void rtsp_init (GstRTSPMediaTable *media_table,
    const gchar *rtmp_host, const gchar *rtmp_port, guint rtmp_timeout,
    const gchar *rtsp_host, const gchar *rtsp_port, guint rtsp_timeout);

void rtsp_stat (GstRTSPMediaTable *media_table,
    guint *num_streams, guint64 *num_streams_bytes,
    guint *num_clients, guint64 *num_clients_bytes);

void json_builder_stream (JsonBuilder *builder, GstRTSPMedia *media);
void json_builder_stream_value (JsonBuilder *builder, GstRTSPMedia *media);
void json_builder_stream_list (JsonBuilder *builder, GstRTSPMediaTable *media_table);
void json_builder_stream_list_value (JsonBuilder *builder, GstRTSPMediaTable *media_table);
gchar * json_builder_to_body (JsonBuilder *builder);

#endif
