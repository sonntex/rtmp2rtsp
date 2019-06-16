#include <stdlib.h>
#include <string.h>

#include "rtsp.h"
#include "http.h"

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

static void http_handle (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);
static void http_handle_streams (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);
static void http_handle_streams_get (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);

void
http_init (GstRTSPMediaTable *media_table, const gchar *host, const gchar *port)
{
  SoupOpaque *opaque;
  SoupServer *server;
  GError *error = NULL;

  opaque = soup_opaque_new (media_table, host, port);

  server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-httpd ", NULL);

  g_object_set_data_full (G_OBJECT (server), "opaque", opaque, (GDestroyNotify) soup_opaque_free);

  soup_server_listen_all (server, atoi(port), 0, &error);

  soup_server_add_handler (server, NULL, http_handle, NULL, NULL);

  g_print ("rtmp2rtsp: run soup at %s:%s\n", host, port);
}

static void
http_handle (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  if (strcmp (path, "/api/v1/streams") == 0) {
    http_handle_streams (server, msg, path, query, context, data);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
  }
}

static void
http_handle_streams (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  if (strcmp (msg->method, "GET") == 0) {
    http_handle_streams_get (server, msg, path, query, context, data);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_METHOD_NOT_ALLOWED);
  }
}

static void
http_handle_streams_get (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  SoupOpaque *opaque = g_object_get_data (G_OBJECT (server), "opaque");
  JsonBuilder *builder;
  gchar *body;

  builder = json_builder_new ();
  json_builder_data_list (builder, opaque->media_table);
  body = json_builder_to_body (builder);
  g_object_unref (builder);

  soup_message_set_response (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen(body));

  soup_message_set_status (msg, SOUP_STATUS_OK);
}
