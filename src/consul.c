#include <stdlib.h>
#include <string.h>

#include "consul.h"
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

static void consul_handle (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);
static void consul_handle_health (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);
static void consul_handle_health_get (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data);

void
consul_init (GstRTSPMediaTable *media_table, const gchar *host, const gchar *port)
{
  SoupOpaque *opaque;
  SoupServer *server;
  GError *error = NULL;

  opaque = soup_opaque_new (media_table, host, port);

  server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-httpd ", NULL);

  g_object_set_data_full (G_OBJECT (server), "opaque", opaque, (GDestroyNotify) soup_opaque_free);

  soup_server_listen_all (server, atoi(port), 0, &error);

  soup_server_add_handler (server, NULL, consul_handle, NULL, NULL);

  g_print ("rtmp2rtsp: run consul at %s:%s\n", host, port);
}

static void
consul_handle (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  if (g_strcmp0 (path, "/health") == 0) {
    consul_handle_health (server, msg, path, query, context, data);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
  }
}

static void
consul_handle_health_get (
    SoupServer *server, SoupMessage *msg, const gchar *path, GHashTable *query,
    SoupClientContext *context, gpointer data)
{
  soup_message_set_status (msg, SOUP_STATUS_OK);
}

void
consul_add (const gchar *local_port, const gchar *agent_port)
{
  SoupSession *session;
  SoupMessage *msg;
  JsonBuilder *builder;
  gchar *body, *url;

  json_builder_service (builder);
  body = json_builder_to_body (builder);
  g_object_unref (builder);

  session = soup_session_new_with_options (SOUP_SESSION_ADD_FEATURE_BY_TYPE,
      SOUP_TYPE_CONTENT_SNIFFER, NULL);

  url = g_strdup_printf ("http://localhost:%s/v1/agent/service/register",
      agent_port);

  msg = soup_message_new ("GET", url);

  soup_message_set_request (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen (body));

  soup_session_send_message (session, msg);

  g_free (url);
}

void
consul_del (const gchar *local_port, const gchar *agent_port)
{
}

void
json_builder_service (JsonBuilder *builder, const gchar *local_port, const gchar *agent_port)
{
  json_builder_begin_object (builder);
  json_builder_service_value (builder, local_port, agent_port);
  json_builder_end_object (builder);
}

void
json_builder_service_value (JsonBuilder *builder, const gchar *local_port, const gchar *agent_port)
{
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, "rtmp2rtsp");
  json_builder_set_member_name (builder, "port");
  json_builder_add_string_value (builder, local_port);
}
