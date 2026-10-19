# rtsp_server

## RTSP camera streaming on the Waveshare ESP32-S3-ETH (#17990)

`templates/rtsp-waveshare-s3-eth-dut.yaml` (OV5640 pins, GPIO8 camera power switch with the 1.2 s boot delay,
`rtsp_server`, logger VERBOSE for `rtsp_server`); swap the `wifi:` block for the W5500 `ethernet:` block when
the cable is in, and add `network: enable_high_performance: true` for the big TCP buffers. Flash over native
USB, capture the log with `serial_capture.py`, take the IP from the last `IP Address` line that is not
0.0.0.0 (or `dns-sd -Q <name>.local`). `scripts/rtsp_server/rtsp_client.py IP [cases]` covers play, udp,
pipeline, badreq, replay, getparam, teardown, third, stall, timeout; `rawrate2.py IP quiet|chatty|rtcp|bigbuf`
measures the raw receive rate; `measure.sh IP LABEL LOGFILE [OUTDIR]` runs the fixed set (ping, ffmpeg 12 s
twice, two ffmpeg clients, raw reader) and appends to `results.txt`; `cam_api.py IP` pulls one image over the
API; `ffmpeg -rtsp_transport tcp -i rtsp://IP:554/ -frames:v 30 -f image2 f%03d.jpg` plus Pillow checks
decoding. The 5760 byte default TCP send buffer makes the stream stall over Wi-Fi (about 1 fps, dozens of
stall drops per run); `enable_high_performance` gives several fps on Wi-Fi and removes the stalls on Ethernet.
Frame counts vary a lot between runs (auto exposure changes the JPEG size), so always measure twice. Home
Assistant's stream pipeline does not accept MJPEG over RTSP; its ffmpeg camera opens one RTSP session per
snapshot, so `max_connections` needs headroom, and go2rtc transcoding to H.264 is the way to a live view.
