#ifndef __RTSP_H__
#define __RTSP_H__

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <json-glib/json-glib.h>

typedef GHashTable GstRTSPMediaTable;

GstRTSPMediaTable *gst_rtsp_media_table_new ();
void gst_rtsp_media_table_free (GstRTSPMediaTable *media_table);

void json_builder_data (JsonBuilder *builder, GstRTSPMedia *media);
void json_builder_data_value (JsonBuilder *builder, GstRTSPMedia *media);
void json_builder_data_list (JsonBuilder *builder, GstRTSPMediaTable *media_table);
void json_builder_data_list_value (JsonBuilder *builder, GstRTSPMediaTable *media_table);
gchar * json_builder_to_body (JsonBuilder *builder);

void rtsp_init (GstRTSPMediaTable *media_table,
    const gchar *rtmp_host, const gchar *rtmp_port, guint rtmp_timeout,
    const gchar *rtsp_host, const gchar *rtsp_port, guint rtsp_timeout);

#endif
