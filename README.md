Dependencies:

* start-stop-daemon
* gstreamer
* gst-plugins-good
* gst-plugins-bad
* gst-plugins-ugly
* nginx
* nginx-rtmp-module

Sources:

* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_750.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_1100.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_1500.mp4

How to generate rtmp?

    sudo cp test/nginx.conf /etc/nginx/nginx.conf
    sudo systemctl restart nginx.service
    test/test-stream

How to stream?

    flv2rtp "rtmp://127.0.0.1:1935/vod/stream" 127.0.0.1 5000

How to stream to multicast group?

    flv2rtp "rtmp://127.0.0.1:1935/vod/stream" 224.1.1.1 5000

How to stream from one multicast group to another multicast group or unicast address?

    rtp2rtp "rtmp://127.0.0.1:1935/vod/stream" 224.1.1.1 5000 10.8.0.1 5000

How to daemonize stream?

    flv2rtpd "rtmp://127.0.0.1:1935/vod/stream" 224.1.1.1 5000 start
    rtp2rtpd "rtmp://127.0.0.1:1935/vod/stream" 224.1.1.1 5000 10.8.0.1 5000 start
    rtp2rtpd "rtmp://127.0.0.1:1935/vod/stream" 224.1.1.1 5000 10.8.0.3 5000 start

How to play stream?

    test/test-player
