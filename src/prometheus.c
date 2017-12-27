#include <stdlib.h>
#include <string.h>

#include "prometheus.h"
#include "rtsp.h"

#include <libsoup/soup.h>

typedef struct _SoupOpaque SoupOpaque;

struct _SoupOpaque
{
  GstRTSPMediaTable *media_table;
  gchar *host;
  gchar *port;
};

static SoupOpaque *
soup_opaque_new (GstRTSPMediaTable *media_table, const gchar* host, const gchar *port)
{
  SoupOpaque *opaque;

  opaque = g_new0 (SoupOpaque, 1);
  opaque->media_table = media_table;
  opaque->host = g_strdup (host);
  opaque->port = g_strdup (port);

  return opaque;
}

static void
soup_opaque_free (SoupOpaque *opaque)
{
  g_free (opaque->host);
  g_free (opaque->port);
  g_free (opaque);
}

static void prometheus_handle (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);
static void prometheus_handle_metrics (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);
static void prometheus_handle_metrics_get (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);

void
prometheus_init (GstRTSPMediaTable *media_table, const gchar *host, const gchar *port)
{
  SoupOpaque *opaque;
  SoupServer *server;
  GError *error = NULL;

  opaque = soup_opaque_new (media_table, host, port);

  server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-httpd ", NULL);

  g_object_set_data_full (G_OBJECT (server), "opaque", opaque, (GDestroyNotify) soup_opaque_free);

  soup_server_listen_all (server, atoi(port), 0, &error);

  soup_server_add_handler (server, NULL, prometheus_handle, NULL, NULL);

  g_print ("rtmp2rtsp: run prometheus at %s:%s\n", host, port);
}

static void
prometheus_handle (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  if (g_strcmp0 (path, "/metrics") == 0) {
    prometheus_handle_metrics (server, msg, path, query, context, data);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
  }
}

static void
prometheus_handle_metrics (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  if (g_strcmp0 (msg->method, "GET") == 0) {
    prometheus_handle_metrics_get (server, msg, path, query, context, data);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_METHOD_NOT_ALLOWED);
  }
}

static void
prometheus_handle_metrics_get (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  SoupOpaque *opaque = g_object_get_data (G_OBJECT (server), "opaque");
  guint streams_num, streams_bps, clients_num, clients_bps;
  gchar *body;

  rtsp_stat (opaque->media_table, &streams_num, &streams_bps, &clients_num, &clients_bps);

  body = g_strdup_printf (
      "rtmp2rtsp_streams_total %u\n"
      "rtmp2rtsp_streams_bitrate %u\n"
      "rtmp2rtsp_clients_total %u\n"
      "rtmp2rtsp_clients_bitrate %u\n",
      streams_num, streams_bps, clients_num, clients_bps);

  soup_message_set_response (msg, "text/plain", SOUP_MEMORY_TAKE, body, strlen(body));

  soup_message_set_status (msg, SOUP_STATUS_OK);
}
