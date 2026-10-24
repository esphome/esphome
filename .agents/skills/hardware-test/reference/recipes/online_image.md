# online_image

## online_image stalled downloads (#20594)

One classic ESP32, both frameworks (the ESP32 Arduino build also uses `http_request_idf.cpp`). The host
serves the images: `scripts/online_image/make_images.py <www>` writes 16x16 BMP/PNG/JPEG/QOI plus an 8 bit
palette BMP with a 1078 byte header; `scripts/online_image/oi_server.py <www> 8765 <log>` answers one
misbehaviour per path name (`ok`, `truncated`, `large_header`, `empty`, `stall` holds the socket 40 s,
`closed_early`, `slow` pauses 2.5 s, `chunked`, `chunked_cut`, `noclen`, `huge` claims 20 MB) with an ETag
and a 304 on If-None-Match. `templates/online-image-dut.yaml` has two AUTO-format images (default and
256 byte buffer), API actions `fetch`/`fetch_small` (set_url with `update: false`, then component.update)
and `refetch`/`refetch_small` (component.update only, keeps the cache headers), and a restart button.
`scripts/online_image/drive.py <device ip> <api key> <capture> http://<host ip>:8765 pr|base` runs the
case list over aioesphomeapi and judges each from the serial capture. ESP-IDF facts: a stalled server
ends with `Error downloading image: -28679` (EAGAIN) after the transport timeout, an early close with
`-1`, both before and after the PR; a body without Content-Length and without chunking reads as complete
(Size: 0) and finishes. A build without the fix hangs on the truncated, oversized header, empty, chunked PNG
and cut chunked cases and every later update logs `Image already being updated.`, so a baseline run needs
a reboot per group, the driver's 45 s cap per case, and an outer `timeout` that covers the whole list.
A case expected as `any` passes on `finished` or `error`, never on no outcome.
