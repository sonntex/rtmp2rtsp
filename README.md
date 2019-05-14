Dependencies:

* start-stop-daemon
* gstreamer
* gst-plugins-good
* gst-plugins-bad
* gst-plugins-ugly
* gst-rtsp-server
* nginx
* nginx-rtmp-module

Sources:

* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_750.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_1100.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_1500.mp4

How to generate source?

    test/test-stream 2>/dev/null

How to play?

    test/test-player 2>/dev/null

How to stream?

    flv2rtsp "rtmp://127.0.0.1:1935/vod/stream" 127.0.0.1 8554

How to daemonize stream?

    flv2rtspd "rtmp://127.0.0.1:1935/vod/stream" 127.0.0.1 8554 start
    flv2rtspd "rtmp://127.0.0.1:1935/vod/stream" 127.0.0.1 8554 status
    flv2rtspd "rtmp://127.0.0.1:1935/vod/stream" 127.0.0.1 8554 info
    flv2rtspd "rtmp://127.0.0.1:1935/vod/stream" 127.0.0.1 8554 stop
