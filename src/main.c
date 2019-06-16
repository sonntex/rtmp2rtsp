#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>

#include "rtsp.h"
#include "http.h"

static gchar *rtmp_host = "127.0.0.1";
static gchar *rtmp_port = "1935";
static gint rtmp_timeout = 10;
static gchar *rtsp_host = "127.0.0.1";
static gchar *rtsp_port = "8554";
static gint rtsp_timeout = 10;
static gchar *http_host = "127.0.0.1";
static gchar *http_port = "8080";

static GOptionEntry options[] =
{
  { "rtmp-host", 0, 0, G_OPTION_ARG_STRING, &rtmp_host, "rtmp host", NULL },
  { "rtmp-port", 0, 0, G_OPTION_ARG_STRING, &rtmp_port, "rtmp port", NULL },
  { "rtmp-timeout", 0, 0, G_OPTION_ARG_INT, &rtmp_timeout, "rtmp timeout", NULL },
  { "rtsp-host", 0, 0, G_OPTION_ARG_STRING, &rtsp_host, "rtsp host", NULL },
  { "rtsp-port", 0, 0, G_OPTION_ARG_STRING, &rtsp_port, "rtsp port", NULL },
  { "rtsp-timeout", 0, 0, G_OPTION_ARG_INT, &rtsp_timeout, "rtsp timeout", NULL },
  { "http-host", 0, 0, G_OPTION_ARG_STRING, &http_host, "http host", NULL },
  { "http-port", 0, 0, G_OPTION_ARG_STRING, &http_port, "http port", NULL },
  { NULL }
};

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
  GstRTSPMediaTable *media_table;
  GMainLoop *loop;
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new ("");

  g_option_context_add_main_entries (context, options, "");

  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    g_print ("rtmp2rtsp: failed to parse arguments\n");
    return 1;
  }

  setup_backtrace ();
  setup_core ();

  gst_init (NULL, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  media_table = gst_rtsp_media_table_new ();

  rtsp_init (media_table, rtmp_host, rtmp_port, 10, rtsp_host, rtsp_port, 10);
  http_init (media_table, http_host, http_port);

  g_main_loop_run (loop);

  gst_rtsp_media_table_free (media_table);

  gst_deinit ();

  return 0;
}
