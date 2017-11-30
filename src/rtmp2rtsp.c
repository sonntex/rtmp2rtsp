#include <stdlib.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/resource.h>

#include <gst/gst.h>

#include <gst/rtsp-server/rtsp-server.h>

gchar *rtmp_host;
gchar *rtmp_port;
gchar *rtsp_host;
gchar *rtsp_port;

const guint timeout = 10;

static void
callback_new_stream (GstRTSPMedia *media, GstRTSPStream *stream, gchar *path)
{
  g_print ("rtmp2rtsp: %s: new stream\n", path);
}

static void
callback_removed_stream (GstRTSPMedia *media, GstRTSPStream *stream, gchar *path)
{
  g_print ("rtmp2rtsp: %s: removed stream\n", path);
}

static void
callback_prepared (GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: prepared\n", path);
}

static void
callback_unprepared (GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: unprepared\n", path);
}

static void
callback_target_state (GstRTSPMedia *media, GstState state, gchar *path)
{
  g_print ("rtmp2rtsp: %s: target state %d\n", path, state);
}

static void
callback_new_state (GstRTSPMedia *media, GstState state, gchar *path)
{
  g_print ("rtmp2rtsp: %s: new state %d\n", path, state);
}

static void
callback_media_configure (GstRTSPMediaFactory *factory, GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: media configure\n", path);

  g_signal_connect_data (media, "new-stream",
      (GCallback) callback_new_stream,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "removed-stream",
      (GCallback) callback_removed_stream,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "prepared",
      (GCallback) callback_prepared,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "unprepared",
      (GCallback) callback_unprepared,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "target-state",
      (GCallback) callback_target_state,
      g_strdup (path), (GClosureNotify) g_free, 0);
  g_signal_connect_data (media, "new-state",
      (GCallback) callback_new_state,
      g_strdup (path), (GClosureNotify) g_free, 0);
}

static void
callback_media_constructed (GstRTSPMediaFactory *factory, GstRTSPMedia *media, gchar *path)
{
  g_print ("rtmp2rtsp: %s: media constructed\n", path);
}

static void
callback_options_request (GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server)
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

    g_signal_connect_data (factory, "media-configure",
        (GCallback) callback_media_configure,
        g_strdup (path), (GClosureNotify) g_free, 0);
    g_signal_connect_data (factory, "media-constructed",
        (GCallback) callback_media_constructed,
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
callback_client_connected (GstRTSPServer *server, GstRTSPClient *client)
{
  g_print ("rtmp2rtsp: client connected\n");

  g_signal_connect_object (client, "options-request",
      (GCallback) callback_options_request,
      server, G_CONNECT_AFTER);
}

static gboolean
callback_timeout (GstRTSPServer *server)
{
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

static void
callback_signal(int signal_number)
{
  void *array[1024];
  int size;
  signal (signal_number, SIG_DFL);
  size = backtrace (array, sizeof (array));
  backtrace_symbols_fd (array, size, STDERR_FILENO);
  kill (getpid (), signal_number);
}

static void
setup_backtrace()
{
  signal (SIGSEGV, callback_signal);
  signal (SIGABRT, callback_signal);
}

static void
setup_core()
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
  GstRTSPServer *server;

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

  setup_backtrace();
  setup_core();

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  server = gst_rtsp_server_new ();

  gst_rtsp_server_set_address (server, rtsp_host);
  gst_rtsp_server_set_service (server, rtsp_port);

  if (gst_rtsp_server_attach (server, NULL) == 0) {
    g_print ("rtmp2rtsp: failed to attach\n");
    return -1;
  }

  g_signal_connect (server, "client-connected", (GCallback) callback_client_connected, NULL);

  g_timeout_add_seconds (timeout, (GSourceFunc) callback_timeout, server);

  g_print ("rtmp2rtsp: run from %s:%s at %s:%s\n", rtmp_host, rtmp_port, rtsp_host, rtsp_port);

  g_main_loop_run (loop);

  return 0;
}
