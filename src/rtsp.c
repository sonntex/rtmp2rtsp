#include "rtsp.h"

#include <gst/rtsp-server/rtsp-server.h>

typedef struct _GstRTSPOpaque GstRTSPOpaque;

struct _GstRTSPOpaque
{
  GstRTSPMediaTable *media_table;
  gchar *rtmp_host;
  gchar *rtmp_port;
  guint rtmp_timeout;
  gchar *rtsp_host;
  gchar *rtsp_port;
  guint rtsp_timeout;
};

static GstRTSPOpaque *
gst_rtsp_opaque_new (GstRTSPMediaTable *media_table,
    const gchar *rtmp_host, const gchar *rtmp_port, guint rtmp_timeout,
    const gchar *rtsp_host, const gchar *rtsp_port, guint rtsp_timeout)
{
  GstRTSPOpaque *opaque;

  opaque = g_new0 (GstRTSPOpaque, 1);
  opaque->media_table = media_table;
  opaque->rtmp_host = g_strdup (rtmp_host);
  opaque->rtmp_port = g_strdup (rtmp_port);
  opaque->rtmp_timeout = rtmp_timeout;
  opaque->rtsp_host = g_strdup (rtsp_host);
  opaque->rtsp_port = g_strdup (rtsp_port);
  opaque->rtsp_timeout = rtsp_timeout;

  return opaque;
}

static void
gst_rtsp_opaque_free (GstRTSPOpaque *opaque)
{
  g_free (opaque->rtmp_host);
  g_free (opaque->rtmp_port);
  g_free (opaque->rtsp_host);
  g_free (opaque->rtsp_port);
  g_free (opaque);
}

GstRTSPMediaTable *
gst_rtsp_media_table_new ()
{
  GstRTSPMediaTable *media_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);

  return media_table;
}

void
gst_rtsp_media_table_free (GstRTSPMediaTable *media_table)
{
  g_hash_table_destroy (media_table);
}

static void
gst_rtsp_url_decode_application_and_uuid (const GstRTSPUrl *uri, gchar **application, gchar **uuid)
{
  gchar **com, **ptr;

  com = gst_rtsp_url_decode_path_components (uri);
  ptr = com;

  if (*(ptr + 0) &&
      *(ptr + 1) &&
      *(ptr + 2))
  {
     *application = g_strdup (*(ptr + 1));
     *uuid = g_strdup (*(ptr + 2));
  }

  g_strfreev (com);
}

void
json_builder_data (JsonBuilder *builder, GstRTSPMedia *media)
{
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "data");
  json_builder_begin_object (builder);
  json_builder_data_value (builder, media);
  json_builder_end_object (builder);
  json_builder_end_object (builder);
}

void
json_builder_data_value (JsonBuilder *builder, GstRTSPMedia *media)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");
  gchar *application = NULL;
  gchar *uuid = NULL;

  gst_rtsp_url_decode_application_and_uuid (uri, &application, &uuid);

  if (!application || !uuid)
    return;

  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, "streams");

  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, uuid);

  json_builder_set_member_name (builder, "attributes");
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "path");
  json_builder_add_string_value (builder, uri->abspath);
  json_builder_set_member_name (builder, "application");
  json_builder_add_string_value (builder, application);
  json_builder_set_member_name (builder, "uuid");
  json_builder_add_string_value (builder, uuid);
  json_builder_end_object (builder);

  g_free (application);
  g_free (uuid);
}

void
json_builder_data_list (JsonBuilder *builder, GstRTSPMediaTable *media_table)
{
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "data");
  json_builder_begin_array (builder);
  json_builder_data_list_value (builder, media_table);
  json_builder_end_array (builder);
  json_builder_end_object (builder);
}

void
json_builder_data_list_value (JsonBuilder *builder, GstRTSPMediaTable *media_table)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, media_table);

  while (g_hash_table_iter_next (&iter, &key, &value))
  {
    json_builder_begin_object (builder);
    json_builder_data_value (builder, value);
    json_builder_end_object (builder);
  }
}

gchar *
json_builder_to_body (JsonBuilder *builder)
{
  JsonGenerator *generator;
  JsonNode *root;
  gchar *body;

  generator = json_generator_new ();
  root = json_builder_get_root (builder);
  json_generator_set_root (generator, root);
  body = json_generator_to_data (generator, NULL);
  json_node_free (root);
  g_object_unref (generator);

  return body;
}

static void rtsp_client_connected (GstRTSPServer *server, GstRTSPClient *client);

static void rtsp_options_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server);
static void rtsp_describe_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server);
static void rtsp_setup_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server);
static void rtsp_play_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server);
static void rtsp_pause_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server);
static void rtsp_teardown_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server);

static void rtsp_media_configure (GstRTSPMediaFactory *factory, GstRTSPMedia *media, GstRTSPServer *server);
static void rtsp_media_prepared (GstRTSPMedia *media, GstRTSPServer *server);
static void rtsp_media_unprepared (GstRTSPMedia *media, GstRTSPServer *server);
static void rtsp_media_target_state (GstRTSPMedia *media, GstState state, GstRTSPServer *server);
static void rtsp_media_new_state (GstRTSPMedia *media, GstState state, GstRTSPServer *server);

static gboolean rtsp_session_pool_cleanup (GstRTSPServer *server);

static void rtsp_media_insert (GstRTSPMediaTable *media_table, GstRTSPMedia *media);
static void rtsp_media_remove (GstRTSPMediaTable *media_table, GstRTSPMedia *media);

void
rtsp_init (GstRTSPMediaTable *media_table,
    const gchar *rtmp_host, const gchar *rtmp_port, guint rtmp_timeout,
    const gchar *rtsp_host, const gchar *rtsp_port, guint rtsp_timeout)
{
  GstRTSPOpaque *opaque;
  GstRTSPServer *server;

  opaque = gst_rtsp_opaque_new (media_table,
      rtmp_host, rtmp_port, rtmp_timeout,
      rtsp_host, rtsp_port, rtsp_timeout);

  server = gst_rtsp_server_new ();

  g_object_set_data_full (G_OBJECT (server), "opaque", opaque, (GDestroyNotify) gst_rtsp_opaque_free);

  gst_rtsp_server_set_address (server, rtsp_host);
  gst_rtsp_server_set_service (server, rtsp_port);

  if (gst_rtsp_server_attach (server, NULL) == 0) {
    g_print ("rtmp2rtsp: failed to attach\n");
    return;
  }

  g_signal_connect (server, "client-connected", (GCallback) rtsp_client_connected, NULL);

  g_timeout_add_seconds (opaque->rtsp_timeout, (GSourceFunc) rtsp_session_pool_cleanup, server);

  g_print ("rtmp2rtsp: run rtsp at %s:%s from %s:%s\n", rtsp_host, rtsp_port, rtmp_host, rtmp_port);
}

static void
rtsp_client_connected (GstRTSPServer *server, GstRTSPClient *client)
{
  g_print ("rtmp2rtsp: client connected\n");

  g_signal_connect (client, "options-request", (GCallback) rtsp_options_request, server);
  g_signal_connect (client, "describe-request", (GCallback) rtsp_describe_request, server);
  g_signal_connect (client, "setup-request", (GCallback) rtsp_setup_request, server);
  g_signal_connect (client, "play-request", (GCallback) rtsp_play_request, server);
  g_signal_connect (client, "pause-request", (GCallback) rtsp_pause_request, server);
  g_signal_connect (client, "teardown-request", (GCallback) rtsp_teardown_request, server);
}

static void
rtsp_options_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  GstRTSPOpaque *opaque = g_object_get_data (G_OBJECT (server), "opaque");
  GstRTSPUrl *uri = ctx->uri;
  GstRTSPMountPoints *mp;
  GstRTSPMediaFactory *factory;

  g_print ("rtmp2rtsp: %s: options request\n", uri->abspath);

  mp = gst_rtsp_server_get_mount_points (server);

  factory = gst_rtsp_mount_points_match (mp, uri->abspath, NULL);

  if (!factory)
  {
    gchar *launch;

    factory = gst_rtsp_media_factory_new ();

    g_object_set_data_full (G_OBJECT (factory), "uri", gst_rtsp_url_copy (uri), (GDestroyNotify) gst_rtsp_url_free);

    launch = g_strdup_printf (
        "( "
        "rtmpsrc location=rtmp://%s:%s%s timeout=%u "
        "! flvdemux name=demux "
        "demux.video "
        "! queue "
        "! h264parse "
        "! rtph264pay name=pay0 pt=96 "
        "demux.audio "
        "! queue "
        "! aacparse "
        "! rtpmp4apay name=pay1 pt=97 "
        ")",
        opaque->rtmp_host, opaque->rtmp_port, uri->abspath, opaque->rtmp_timeout);

    gst_rtsp_media_factory_set_launch (factory, launch);
    gst_rtsp_media_factory_set_shared (factory, TRUE);
    gst_rtsp_media_factory_set_eos_shutdown (factory, TRUE);

    g_signal_connect (factory, "media-configure", (GCallback) rtsp_media_configure, server);

    gst_rtsp_mount_points_add_factory (mp, uri->abspath, factory);

    g_free (launch);
  }
  else
  {
    g_object_unref (factory);
  }

  g_object_unref (mp);
}

static void
rtsp_describe_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  GstRTSPUrl *uri = ctx->uri;

  g_print ("rtmp2rtsp: %s: describe request\n", uri->abspath);
}

static void
rtsp_setup_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  GstRTSPUrl *uri = ctx->uri;

  g_print ("rtmp2rtsp: %s: setup request\n", uri->abspath);
}

static void
rtsp_play_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  GstRTSPUrl *uri = ctx->uri;

  g_print ("rtmp2rtsp: %s: play request\n", uri->abspath);
}

static void
rtsp_pause_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  GstRTSPUrl *uri = ctx->uri;

  g_print ("rtmp2rtsp: %s: pause request\n", uri->abspath);
}

static void
rtsp_teardown_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  GstRTSPUrl *uri = ctx->uri;

  g_print ("rtmp2rtsp: %s: teardown request\n", uri->abspath);
}

static void
rtsp_media_configure (GstRTSPMediaFactory *factory, GstRTSPMedia *media, GstRTSPServer *server)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (factory), "uri");

  g_print ("rtmp2rtsp: %s: media configure\n", uri->abspath);

  g_object_set_data_full (G_OBJECT (media), "uri", gst_rtsp_url_copy (uri), (GDestroyNotify) gst_rtsp_url_free);

  gst_rtsp_media_set_reusable (media, TRUE);

  g_signal_connect (media, "prepared", (GCallback) rtsp_media_prepared, server);
  g_signal_connect (media, "unprepared", (GCallback) rtsp_media_unprepared, server);
  g_signal_connect (media, "target-state", (GCallback) rtsp_media_target_state, server);
  g_signal_connect (media, "new-state", (GCallback) rtsp_media_new_state, server);
}

static void
rtsp_media_prepared (GstRTSPMedia *media, GstRTSPServer *server)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");

  g_print ("rtmp2rtsp: %s: media prepared\n", uri->abspath);
}

static void
rtsp_media_unprepared (GstRTSPMedia *media, GstRTSPServer *server)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");

  g_print ("rtmp2rtsp: %s: media unprepared\n", uri->abspath);
}

static void
rtsp_media_target_state (GstRTSPMedia *media, GstState state, GstRTSPServer *server)
{
  GstRTSPOpaque *opaque = g_object_get_data (G_OBJECT (server), "opaque");
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");

  g_print ("rtmp2rtsp: %s: media target state %d\n", uri->abspath, state);

  if (state == GST_STATE_PLAYING)
  {
    rtsp_media_insert (opaque->media_table, media);
  }

  if (state == GST_STATE_NULL)
  {
    rtsp_media_remove (opaque->media_table, media);
    gst_rtsp_mount_points_remove_factory (
        gst_rtsp_server_get_mount_points (server), uri->abspath);
  }
}

static void
rtsp_media_new_state (GstRTSPMedia *media, GstState state, GstRTSPServer *server)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");

  g_print ("rtmp2rtsp: %s: media new state %d\n", uri->abspath, state);
}

static gboolean
rtsp_session_pool_cleanup (GstRTSPServer *server)
{
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

static void
rtsp_media_insert (GstRTSPMediaTable *media_table, GstRTSPMedia *media)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");

  g_hash_table_insert (media_table, g_strdup (uri->abspath), media);
}

static void
rtsp_media_remove (GstRTSPMediaTable *media_table, GstRTSPMedia *media)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");

  g_hash_table_remove (media_table, uri->abspath);
}
