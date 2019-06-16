#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>

#include "rtsp.h"
#include "http.h"

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
  GstRTSPMediaTable *media_table;
  gchar *rtmp_host;
  gchar *rtmp_port;
  gchar *rtsp_host;
  gchar *rtsp_port;
  gchar *http_host;
  gchar *http_port;

  rtmp_host = getenv ("RTMP_HOST");
  rtmp_port = getenv ("RTMP_PORT");
  rtsp_host = getenv ("RTSP_HOST");
  rtsp_port = getenv ("RTSP_PORT");
  http_host = getenv ("HTTP_HOST");
  http_port = getenv ("HTTP_PORT");

  if (!rtmp_host ||
      !rtmp_port ||
      !rtsp_host ||
      !rtsp_port ||
      !http_host ||
      !http_port) {
    g_print ("rtmp2rtsp: failed to parse arguments\n");
    return -1;
  }

  setup_backtrace ();
  setup_core ();

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  media_table = gst_rtsp_media_table_new ();

  rtsp_init (media_table, rtmp_host, rtmp_port, 10, rtsp_host, rtsp_port, 10);
  http_init (media_table, http_host, http_port);

  g_main_loop_run (loop);

  gst_rtsp_media_table_free (media_table);

  return 0;
}
