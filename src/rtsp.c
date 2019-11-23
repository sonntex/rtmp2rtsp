#include "rtsp.h"

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
rtsp_opaque_new (GstRTSPMediaTable *media_table,
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
rtsp_opaque_free (GstRTSPOpaque *opaque)
{
  g_free (opaque->rtmp_host);
  g_free (opaque->rtmp_port);
  g_free (opaque->rtsp_host);
  g_free (opaque->rtsp_port);
  g_free (opaque);
}

GstRTSPMediaTable *
rtsp_media_table_new ()
{
  GstRTSPMediaTable *media_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);

  return media_table;
}

void
rtsp_media_table_free (GstRTSPMediaTable *media_table)
{
  g_hash_table_destroy (media_table);
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

static gchar * rtsp_media_get_status (GstRTSPMedia *media);

static GstStructure * rtsp_media_get_structure (GstRTSPMedia *media, const gchar *name);

static gboolean
rtsp_media_get_video_props (GstRTSPMedia *media,
    gchar **codec, gint *width, gint *height, gint *framerate_num, gint *framerate_den);
static gboolean
rtsp_media_get_audio_props (GstRTSPMedia *media,
    gchar **codec, gint *channels, gint *rate);

static gchar * rtsp_url_get_id (const GstRTSPUrl *uri);

static void rtsp_media_stat (GstRTSPMedia *media,
    guint *streams_num, guint *streams_bps,
    guint *clients_num, guint *clients_bps);
static void rtsp_media_table_stat (GstRTSPMediaTable *media_table,
    guint *streams_num, guint *streams_bps,
    guint *clients_num, guint *clients_bps);

void
rtsp_init (GstRTSPMediaTable *media_table,
    const gchar *rtmp_host, const gchar *rtmp_port, guint rtmp_timeout,
    const gchar *rtsp_host, const gchar *rtsp_port, guint rtsp_timeout)
{
  GstRTSPOpaque *opaque;
  GstRTSPServer *server;

  opaque = rtsp_opaque_new (media_table,
      rtmp_host, rtmp_port, rtmp_timeout,
      rtsp_host, rtsp_port, rtsp_timeout);

  server = gst_rtsp_server_new ();

  g_object_set_data_full (G_OBJECT (server), "opaque", opaque, (GDestroyNotify) rtsp_opaque_free);

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
        "! h264parse name=parse0 "
        "! rtph264pay name=pay0 pt=96 "
        "demux.audio "
        "! queue "
        "! aacparse name=parse1 "
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

  gst_rtsp_media_get_stream (media, 0);
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

static gchar *
rtsp_media_get_status (GstRTSPMedia *media)
{
  switch (gst_rtsp_media_get_status (media))
  {
  case GST_RTSP_MEDIA_STATUS_UNPREPARED:
    return "unprepared";
  case GST_RTSP_MEDIA_STATUS_UNPREPARING:
    return "unpreparing";
  case GST_RTSP_MEDIA_STATUS_PREPARED:
    return "prepared";
  case GST_RTSP_MEDIA_STATUS_PREPARING:
    return "preparing";
  case GST_RTSP_MEDIA_STATUS_SUSPENDED:
    return "suspended";
  case GST_RTSP_MEDIA_STATUS_ERROR:
    return "error";
  default:
    return "";
  }
}

static GstStructure *
rtsp_media_get_structure (GstRTSPMedia *media, const gchar *name)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");
  GstElement *bin, *element;
  GstStructure *structure;
  GList *item;

  bin = gst_rtsp_media_get_element (media);
  if (!bin)
  {
    g_print ("rtmp2rtsp: %s: failed to get bin\n", uri->abspath);
    return NULL;
  }

  element = gst_bin_get_by_name (GST_BIN (bin), name);
  if (!element)
  {
    g_print ("rtmp2rtsp: %s: failed to get element %s\n", uri->abspath, name);
    return NULL;
  }

  for (item = GST_ELEMENT_PADS (element); item; item = g_list_next (item))
  {
    if (GST_PAD_IS_SRC (GST_PAD (item->data)))
    {
      structure = gst_caps_get_structure (
          gst_pad_get_current_caps (GST_PAD (item->data)), 0);

      return structure;
    }
  }

  return NULL;
}

static gboolean
rtsp_media_get_video_props (GstRTSPMedia *media,
    gchar **codec, gint *width, gint *height, gint *framerate_num, gint *framerate_den)
{
  GstStructure *structure;

  structure = rtsp_media_get_structure (media, "parse0");

  if (structure)
  {
    if (gst_structure_get_int (structure, "width", width) &&
        gst_structure_get_int (structure, "height", height) &&
        gst_structure_get_fraction (structure, "framerate", framerate_num, framerate_den))
    {
      if (g_str_equal (gst_structure_get_name (structure), "video/x-h264"))
      {
        *codec = "h264";
        return TRUE;
      }
    }
  }

  return FALSE;
}

static gboolean
rtsp_media_get_audio_props (GstRTSPMedia *media,
    gchar **codec, gint *channels, gint *rate)
{
  GstStructure *structure;

  structure = rtsp_media_get_structure (media, "parse1");

  if (structure)
  {
    if (gst_structure_get_int (structure, "channels", channels) &&
        gst_structure_get_int (structure, "rate", rate))
    {
      if (g_str_equal (gst_structure_get_name (structure), "audio/mpeg"))
      {
        *codec = "aac";
        return TRUE;
      }
    }
  }

  return FALSE;
}

static gchar *
rtsp_url_get_id (const GstRTSPUrl *uri)
{
  gchar **com, **ptr;
  gchar *id;

  com = gst_rtsp_url_decode_path_components (uri);
  ptr = com;

  if (*(ptr + 0) && *(ptr + 1) && *(ptr + 2))
    id = g_strdup (*(ptr + 2));
  else
    id = g_strdup ("");

  g_strfreev (com);

  return id;
}

void
json_builder_stream (JsonBuilder *builder, GstRTSPMedia *media)
{
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "data");
  json_builder_begin_object (builder);
  json_builder_stream_value (builder, media);
  json_builder_end_object (builder);
  json_builder_end_object (builder);
}

void
json_builder_stream_value (JsonBuilder *builder, GstRTSPMedia *media)
{
  GstRTSPUrl *uri = g_object_get_data (G_OBJECT (media), "uri");
  gchar *id, *codec;
  gint width, height, framerate_num, framerate_den, channels, rate;

  id = rtsp_url_get_id (uri);

  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, "streams");
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, id);

  json_builder_set_member_name (builder, "meta");
  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "path");
  json_builder_add_string_value (builder, uri->abspath);
  json_builder_set_member_name (builder, "status");
  json_builder_add_string_value (builder, rtsp_media_get_status (media));

  if (rtsp_media_get_video_props (media,
          &codec,
          &width,
          &height,
          &framerate_num,
          &framerate_den))
  {
    json_builder_set_member_name (builder, "video");
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "codec");
    json_builder_add_string_value (builder, codec);
    json_builder_set_member_name (builder, "width");
    json_builder_add_int_value (builder, width);
    json_builder_set_member_name (builder, "height");
    json_builder_add_int_value (builder, height);
    json_builder_set_member_name (builder, "framerate_num");
    json_builder_add_int_value (builder, framerate_num);
    json_builder_set_member_name (builder, "framerate_den");
    json_builder_add_int_value (builder, framerate_den);

    json_builder_end_object (builder);
  }

  if (rtsp_media_get_audio_props (media,
          &codec,
          &channels,
          &rate))
  {
    json_builder_set_member_name (builder, "audio");
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "codec");
    json_builder_add_string_value (builder, codec);
    json_builder_set_member_name (builder, "channels");
    json_builder_add_int_value (builder, channels);
    json_builder_set_member_name (builder, "rate");
    json_builder_add_int_value (builder, rate);

    json_builder_end_object (builder);
  }

  json_builder_end_object (builder);

  g_free (id);
}

void
json_builder_stream_list (JsonBuilder *builder, GstRTSPMediaTable *media_table)
{
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "data");
  json_builder_begin_array (builder);
  json_builder_stream_list_value (builder, media_table);
  json_builder_end_array (builder);
  json_builder_end_object (builder);
}

void
json_builder_stream_list_value (JsonBuilder *builder, GstRTSPMediaTable *media_table)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, media_table);

  while (g_hash_table_iter_next (&iter, &key, &value))
  {
    json_builder_begin_object (builder);
    json_builder_stream_value (builder, value);
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
