Sources:

* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_750.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_1100.mp4
* rtmp://184.72.239.149/vod/mp4:bigbuckbunny_1500.mp4

How to stream?

    flv2rtp "rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4" 127.0.0.1 5000

How to stream to multicast group?

    flv2rtp "rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4" 224.1.1.1 5000

How to stream from one multicast group to another multicast group or unicast address?

    rtp2rtp "rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4" 224.1.1.1 5000 10.8.0.1 5000

How to daemonize stream?

    flv2rtpd "rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4" 224.1.1.1 5000 start
    flv2rtpd "rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4" 224.1.1.1 5000 10.8.0.1 5000 start
    flv2rtpd "rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4" 224.1.1.1 5000 10.8.0.3 5000 start

How to play stream?

    vlc test/stream.sdp
    test/test-video
    test/test-audio
