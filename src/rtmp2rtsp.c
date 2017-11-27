#include <stdlib.h>

#include <gst/gst.h>

#include <gst/rtsp-server/rtsp-server.h>

gchar *rtmp_host;
gchar *rtmp_port;
gchar *rtsp_host;
gchar *rtsp_port;

const guint timeout = 10;

static void
callback_options_request (GstRTSPClient * client, GstRTSPContext * ctx, GstRTSPServer * server)
{
  GstRTSPMountPoints *mounts;
  GstRTSPMediaFactory *factory;

  g_print ("rtmp2rtsp: options request\n");

  mounts = gst_rtsp_server_get_mount_points (server);

  factory = gst_rtsp_mount_points_match (mounts, ctx->uri->abspath, NULL);

  if (!factory)
  {
    gchar *launch;

    factory = gst_rtsp_media_factory_new ();

    launch = g_strdup_printf (
        "( rtmpsrc location=rtmp://%s:%s%s timeout=%u ! flvdemux name=demux "
        "  demux.video ! queue ! h264parse ! rtph264pay name=pay0 pt=96 "
        "  demux.audio ! queue !  aacparse ! rtpmp4apay name=pay1 pt=97 )",
        rtmp_host, rtmp_port, ctx->uri->abspath, timeout);

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
callback_client_connected (GstRTSPServer * server, GstRTSPClient * client)
{
  g_print ("rtmp2rtsp: client connected\n");

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
