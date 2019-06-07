#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/resource.h>

#include <gst/gst.h>

#include <gst/rtsp-server/rtsp-server.h>

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

gchar *rtmp_host;
gchar *rtmp_port;
gchar *rtsp_host;
gchar *rtsp_port;

const guint timeout = 10;

GstRTSPServer *rtsp_server;

GHashTable *stream_table;

static void
stream_insert (gchar *key)
{
  g_hash_table_insert (stream_table, g_strdup (key), NULL);
}

static void
stream_remove (gchar *key)
{
  g_hash_table_remove (stream_table, key);
}

static void
rtsp_new_stream (GstRTSPMedia *media, GstRTSPStream *stream, gchar *path)
{
  g_print ("rtmp2rtsp: %s: new stream\n", path);
}

static void
rtsp_removed_stream (GstRTSPMedia *media, GstRTSPStream *stream, gchar *path)
{
  g_print ("rtmp2rtsp: %s: removed stream\n", path);
}

static void
rtsp_prepared (GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: prepared\n", path);

  stream_insert (path);
}

static void
rtsp_unprepared (GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: unprepared\n", path);

  stream_remove (path);
  gst_rtsp_mount_points_remove_factory (
      gst_rtsp_server_get_mount_points (rtsp_server), path);
}

static void
rtsp_target_state (GstRTSPMedia *media, GstState state, gchar *path)
{
  g_print ("rtmp2rtsp: %s: target state %d\n", path, state);

  if (state == GST_STATE_NULL) {
    switch (gst_rtsp_media_get_status (media)) {
      case GST_RTSP_MEDIA_STATUS_UNPREPARED:
      case GST_RTSP_MEDIA_STATUS_ERROR:
        stream_remove (path);
        gst_rtsp_mount_points_remove_factory (
            gst_rtsp_server_get_mount_points (rtsp_server), path);
        break;
      default:
        break;
    }
  }
}

static void
rtsp_new_state (GstRTSPMedia *media, GstState state, gchar *path)
{
  g_print ("rtmp2rtsp: %s: new state %d\n", path, state);
}

static void
rtsp_media_configure (GstRTSPMediaFactory *factory, GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: media configure\n", path);

  gst_rtsp_media_set_reusable (media, TRUE);

  g_signal_connect_data (media, "new-stream",
      (GCallback) rtsp_new_stream,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "removed-stream",
      (GCallback) rtsp_removed_stream,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "prepared",
      (GCallback) rtsp_prepared,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "unprepared",
      (GCallback) rtsp_unprepared,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "target-state",
      (GCallback) rtsp_target_state,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "new-state",
      (GCallback) rtsp_new_state,
      g_strdup (path), (GClosureNotify) g_free, 0);
}

static void
rtsp_media_constructed (GstRTSPMediaFactory *factory, GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: media constructed\n", path);
}

static void
rtsp_options_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  GstRTSPMountPoints *mounts;
  GstRTSPMediaFactory *factory;
  gchar *path = ctx->uri->abspath;

  g_print ("rtmp2rtsp: %s: options request\n", path);

  mounts = gst_rtsp_server_get_mount_points (server);

  factory = gst_rtsp_mount_points_match (mounts, path, NULL);

  if (!factory)
  {
    gchar *launch;

    factory = gst_rtsp_media_factory_new ();

    launch = g_strdup_printf (
        "( rtmpsrc location=rtmp://%s:%s%s timeout=%u ! flvdemux name=demux "
        "  demux.video ! queue ! h264parse ! rtph264pay name=pay0 pt=96 "
        "  demux.audio ! queue !  aacparse ! rtpmp4apay name=pay1 pt=97 )",
        rtmp_host, rtmp_port, path, timeout);

    gst_rtsp_media_factory_set_launch (factory, launch);
    gst_rtsp_media_factory_set_shared (factory, TRUE);
    gst_rtsp_media_factory_set_eos_shutdown (factory, TRUE);

    g_signal_connect_data (factory, "media-configure",
        (GCallback) rtsp_media_configure,
        g_strdup (path), (GClosureNotify) g_free, 0);
    g_signal_connect_data (factory, "media-constructed",
        (GCallback) rtsp_media_constructed,
        g_strdup (path), (GClosureNotify) g_free, 0);

    gst_rtsp_mount_points_add_factory (mounts, path, factory);

    g_free (launch);
  }
  else
  {
    g_object_unref (factory);
  }

  g_object_unref (mounts);
}

static void
rtsp_describe_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  g_print ("rtmp2rtsp: %s: describe request\n", ctx->uri->abspath);
}

static void
rtsp_setup_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  g_print ("rtmp2rtsp: %s: setup request\n", ctx->uri->abspath);
}

static void
rtsp_play_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  g_print ("rtmp2rtsp: %s: play request\n", ctx->uri->abspath);
}

static void
rtsp_pause_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  g_print ("rtmp2rtsp: %s: pause request\n", ctx->uri->abspath);
}

static void
rtsp_teardown_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  g_print ("rtmp2rtsp: %s: teardown request\n", ctx->uri->abspath);
}

static void
rtsp_get_parameter_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  g_print ("rtmp2rtsp: %s: get parameter request\n", ctx->uri->abspath);
}

static void
rtsp_set_parameter_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
{
  g_print ("rtmp2rtsp: %s: set parameter request\n", ctx->uri->abspath);
}

static void
rtsp_client_connected (GstRTSPServer *server, GstRTSPClient *client)
{
  g_print ("rtmp2rtsp: client connected\n");

  g_signal_connect_object (client, "options-request",
      (GCallback) rtsp_options_request,
      server, G_CONNECT_AFTER);
  g_signal_connect_object (client, "describe-request",
      (GCallback) rtsp_describe_request,
      server, G_CONNECT_AFTER);
  g_signal_connect_object (client, "setup-request",
      (GCallback) rtsp_setup_request,
      server, G_CONNECT_AFTER);
  g_signal_connect_object (client, "play-request",
      (GCallback) rtsp_play_request,
      server, G_CONNECT_AFTER);
  g_signal_connect_object (client, "pause-request",
      (GCallback) rtsp_pause_request,
      server, G_CONNECT_AFTER);
  g_signal_connect_object (client, "teardown-request",
      (GCallback) rtsp_teardown_request,
      server, G_CONNECT_AFTER);
  g_signal_connect_object (client, "get-parameter-request",
      (GCallback) rtsp_get_parameter_request,
      server, G_CONNECT_AFTER);
  g_signal_connect_object (client, "set-parameter-request",
      (GCallback) rtsp_set_parameter_request,
      server, G_CONNECT_AFTER);
}

static gboolean
rtsp_timeout (GstRTSPServer *server)
{
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

static void
soup_message_streams_get (SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query, SoupClientContext *context, gpointer data)
{
  JsonBuilder *builder;
  JsonGenerator *gen;
  JsonNode *root;
  gchar *body;
  GHashTableIter iter;
  gpointer key, value;

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "data");
  json_builder_begin_array (builder);

  g_hash_table_iter_init (&iter, stream_table);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "streams");
    json_builder_set_member_name (builder, "id");
    json_builder_add_string_value (builder, key);
    json_builder_set_member_name (builder, "attributes");
    json_builder_begin_object (builder);
    json_builder_end_object (builder);
    json_builder_end_object (builder);
  }

  json_builder_end_array (builder);

  json_builder_set_member_name (builder, "meta");
  json_builder_begin_object (builder);
  json_builder_end_object (builder);

  json_builder_end_object (builder);

  gen = json_generator_new ();
  root = json_builder_get_root (builder);
  json_generator_set_root (gen, root);

  body = json_generator_to_data (gen, NULL);

  json_node_free (root);
  g_object_unref (gen);
  g_object_unref (builder);

  soup_message_set_response (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen(body));

  soup_message_set_status (msg, SOUP_STATUS_OK);
}

static void
soup_message_streams (SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query, SoupClientContext *context, gpointer data)
{
  if (strcmp (msg->method, "GET") == 0) {
    soup_message_streams_get (server, msg, path, query, context, data);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_METHOD_NOT_ALLOWED);
  }
}

static void
soup_message (SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query, SoupClientContext *context, gpointer data)
{
  if (strcmp (path, "/api/v1/streams") == 0) {
    soup_message_streams (server, msg, path, query, context, data);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
  }
}

static void
callback_signal (int signal_number)
{
  void *array[1024];
  int size;
  signal (signal_number, SIG_DFL);
  size = backtrace (array, sizeof (array));
  backtrace_symbols_fd (array, size, STDERR_FILENO);
  kill (getpid (), signal_number);
}

static void
setup_backtrace ()
{
  signal (SIGSEGV, callback_signal);
  signal (SIGABRT, callback_signal);
}

static void
setup_core ()
{
  struct rlimit limit;
  getrlimit (RLIMIT_CORE, &limit);
  limit.rlim_cur = RLIM_INFINITY;
  limit.rlim_max = RLIM_INFINITY;
  setrlimit (RLIMIT_CORE, &limit);
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  GError *error = NULL;
  SoupServer *soup_server;

  rtmp_host = getenv("RTMP_HOST");
  rtmp_port = getenv("RTMP_PORT");
  rtsp_host = getenv("RTSP_HOST");
  rtsp_port = getenv("RTSP_PORT");

  if (!rtmp_host ||
      !rtmp_port ||
      !rtsp_host ||
      !rtsp_port) {
    g_print ("rtmp2rtsp: failed to parse arguments\n");
    return -1;
  }

  setup_backtrace ();
  setup_core ();

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  rtsp_server = gst_rtsp_server_new ();

  gst_rtsp_server_set_address (rtsp_server, rtsp_host);
  gst_rtsp_server_set_service (rtsp_server, rtsp_port);

  if (gst_rtsp_server_attach (rtsp_server, NULL) == 0) {
    g_print ("rtmp2rtsp: failed to attach\n");
    return -1;
  }

  g_signal_connect_object (rtsp_server, "client-connected",
      (GCallback) rtsp_client_connected,
      NULL, G_CONNECT_AFTER);

  g_timeout_add_seconds (timeout, (GSourceFunc) rtsp_timeout, rtsp_server);

  g_print ("rtmp2rtsp: run rtsp at %s:%s from %s:%s\n", rtsp_host, rtsp_port, rtmp_host, rtmp_port);

  soup_server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-httpd ", NULL);
  soup_server_listen_local (soup_server, 8080, 0, &error);

  soup_server_add_handler (soup_server, NULL, soup_message, NULL, NULL);

  g_print ("rtmp2rtsp: run soup at 127.0.0.1:8080\n");

  stream_table = g_hash_table_new (g_str_hash, g_str_equal);

  g_main_loop_run (loop);

  g_hash_table_destroy (stream_table);

  return 0;
}
