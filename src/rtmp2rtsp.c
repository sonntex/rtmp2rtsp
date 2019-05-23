#include <gst/gst.h>

#include <gst/rtsp-server/rtsp-server.h>

const guint timeout = 10;

static void
callback_options_request (GstRTSPClient * client, GstRTSPContext * ctx, GstRTSPServer * server, gpointer user_data)
{
  GstRTSPMountPoints *mounts;
  GstRTSPMediaFactory *factory;

  mounts = gst_rtsp_server_get_mount_points (server);

  factory = gst_rtsp_mount_points_match (mounts, ctx->uri->abspath, NULL);

  if (!factory)
  {
    gchar *launch;

    factory = gst_rtsp_media_factory_new ();

    launch = g_strdup_printf (
        "( rtmpsrc location=rtmp://localhost:1935/rtmp2rtsp%s timeout=%u ! flvdemux name=demux "
        "  demux.video ! queue ! h264parse ! rtph264pay name=pay0 pt=96 "
        "  demux.audio ! queue !  aacparse ! rtpmp4apay name=pay1 pt=97 )",
        ctx->uri->abspath, timeout);

    gst_rtsp_media_factory_set_launch (factory, launch);
    gst_rtsp_media_factory_set_shared (factory, TRUE);

    gst_rtsp_mount_points_add_factory (mounts, ctx->uri->abspath, factory);

    g_print ("rtmp2rtsp: launch %s\n", launch);

    g_free (launch);
  }
  else
  {
    g_object_unref (factory);
  }

  g_object_unref (mounts);
}

static void
callback_client_connected (GstRTSPServer * server, GstRTSPClient * client, gpointer user_data)
{
  g_signal_connect_object (client, "options-request", (GCallback) callback_options_request, server, G_CONNECT_AFTER);
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

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  GstRTSPServer *server;
  gchar *host;
  gchar *port;

  if (argc != 3) {
    g_print ("rtmp2rtsp: failed to parse arguments\n");
    return -1;
  }

  host = argv[1];
  port = argv[2];

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  server = gst_rtsp_server_new ();

  gst_rtsp_server_set_address (server, host);
  gst_rtsp_server_set_service (server, port);

  if (gst_rtsp_server_attach (server, NULL) == 0) {
    g_print ("rtmp2rtsp: failed to attach\n");
    return -1;
  }

  g_signal_connect (server, "client-connected", (GCallback) callback_client_connected, NULL);

  g_timeout_add_seconds (timeout, (GSourceFunc) callback_timeout, server);

  g_print ("rtmp2rtsp: run at %s:%s\n", host, port);

  g_main_loop_run (loop);

  return 0;
}
