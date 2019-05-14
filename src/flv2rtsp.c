#include <gst/gst.h>

#include <gst/rtsp-server/rtsp-server.h>

/* this timeout is periodically run to clean up the expired sessions from the
 * pool. This needs to be run explicitly currently but might be done
 * automatically as part of the mainloop. */
static gboolean
timeout (GstRTSPServer * server)
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
  GstRTSPMountPoints *mounts;
  GstRTSPMediaFactory *factory;
  gchar *location;
  gchar *host;
  gchar *port;
  gchar *description;

  if (argc != 4) {
    g_print ("failed to parse arguments\n");
    return -1;
  }

  location = argv[1];
  host = argv[2];
  port = argv[3];

  description = g_strdup_printf ("( "
      "rtmpsrc location=%s timeout=10 ! flvdemux name=demux "
      "demux.video ! queue ! h264parse ! rtph264pay name=pay0 pt=96 config-interval=1 "
      "demux.audio ! queue ! aacparse  ! rtpmp4apay name=pay1 pt=97 " ")",
      location);

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  /* create a server instance */
  server = gst_rtsp_server_new ();
  gst_rtsp_server_set_address (server, host);
  gst_rtsp_server_set_service (server, port);

  /* get the mount points for this server, every server has a default object
   * that be used to map uri mount points to media factories */
  mounts = gst_rtsp_server_get_mount_points (server);

  /* make a media factory for a test stream. The default media factory can use
   * gst-launch syntax to create pipelines.
   * any launch line works as long as it contains elements named pay%d. Each
   * element with pay%d names will be a stream */
  factory = gst_rtsp_media_factory_new ();
  gst_rtsp_media_factory_set_launch (factory, description);

  /* attach the test factory to the /test url */
  gst_rtsp_mount_points_add_factory (mounts, "/stream", factory);

  /* don't need the ref to the mapper anymore */
  g_object_unref (mounts);

  /* attach the server to the default maincontext */
  if (gst_rtsp_server_attach (server, NULL) == 0)
    goto failed;

  /* add a timeout for the session cleanup */
  g_timeout_add_seconds (2, (GSourceFunc) timeout, server);

  /* start serving, this never stops */
  g_print ("stream ready at rtsp://%s:%s/stream\n", host, port);
  g_main_loop_run (loop);

  g_free (description);

  return 0;

  /* ERRORS */
failed:
  {
    g_print ("failed to attach the server\n");
    return -1;
  }
}
