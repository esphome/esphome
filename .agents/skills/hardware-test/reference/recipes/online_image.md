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

## PNG decode time limit (#21364, #21391)

The same server also serves any plain file in its www directory by name. `scripts/online_image/make_big_pngs.py
<www>` (after `make_images.py`) writes a 320x240 gradient (618 B, also as `ok.png` so the misbehaviour cases carry a
real multi-row PNG), a flat 640x480 PNG (1.6 KB for 307200 pixels), 231 KB of noise and a JPEG. The device is
`templates/online-image-dut.yaml` with `type: BINARY` on the main image (a 640x480 RGB565 does not fit a classic
ESP32 without PSRAM) and `debug` loop time and heap sensors at 2 s; the 256 byte buffer image stays RGB565, so do
not point big images at it (out of memory on every build). `scripts/online_image/pngdrive.py HOST KEY LOGFILE
BASEURL LABEL` (`CASES=short` for the six case list) calls the actions, waits for the finished or error line and
reports the case time and the highest loop time. The per pass `online_image.image took a long time` values come from
the serial capture and only print on a new maximum, so read the first fetch of each image. The pass that decodes a
whole buffer is the decoder cost (before the change 510 ms gradient and 1980 ms flat, after it 77 and 140 ms); for
the noisy PNG the longest pass is the blocking 64 KB read on both builds (0.7 to 3.7 s). Alternate PR, base, PR
for a timing claim.

For a replacement PR, compare against the parent PR head, not dev. `CASES=noise` fetches the noisy PNG three times
(run it right after a boot, alternating builds); `CASES=end` runs flat and gradient as plain, `noclen_`, `chunked_`,
`half_` and `stall_` through the prefixed server cases below.

## JPEG decoded as it downloads (#21342)

`oi_server.py` serves prefixed cases on any file: `half_<file>` (first half, matching Content-Length),
`noclen_<file>` (close delimited), `chunked_<file>` (1 KB chunks), `stall_<file>` (a third, then the socket held
40 s). `scripts/online_image/make_jpegs.py <www>` writes noise, gradient, 12 KB EXIF and progressive JPEGs. Device:
`templates/online-image-dut.yaml` with `type: BINARY` on both entities and `buffer_size: 4096` on the second;
`scripts/online_image/jpgdrive.py HOST KEY LOGFILE BASEURL LABEL` runs the list and prints case time, highest loop
time and lowest free heap. Before the change a JPEG needs a contiguous block of its file size (233 KB never
allocates on a classic ESP32, and after a few failures 69 KB does not either until a reboot); after it the 64 KB
buffer stays 64 KB and 4 KB is enough for a baseline JPEG. The first pass still reads up to `buffer_size` in one
blocking read, so the 64 KB entity blocks 1.7 to 4 s and the 4 KB one 272 ms at most; judge the decode budget on
the small buffer entity. Already on dev: a JPEG without Content-Length (plain or chunked) ends with `Could not open
image for decoding: 4` then `Invalid type`.

## BMP and QOI decode time limit (#21392)

Use a wired board (`templates/online-image-s3-eth-dut.yaml`: Waveshare ESP32-S3-ETH, W5500, PSRAM, `type: RGB565`)
rather than a classic ESP32 on Wi-Fi: with a weak link the reads block up to the 4.5 s timeout and the per case loop
numbers are noise. `scripts/online_image/make_bmp_qoi.py <www>` writes 1 bit BMP 640x480, 8 and 24 bit BMP 320x240,
QOI 640x480 one colour (all runs) and checkerboard, QOI 320x240 gradient and noise. `pngdrive.py` with
`CASES=bmpqoi` runs each as plain, `noclen_`, `chunked_`, `half_` plus stalls. The `loop_time` sensor sits at 75
to 160 ms even for a 16x16 image, so for decode passes use `scripts/online_image/firstpass.py HOST KEY BASEURL
LABEL IMAGE...`: it reboots the device before each fetch and reports that fetch's `took a long time` maximum. A
`half_` file with matching Content-Length reports `download finished` with "not caching it" on dev too.
