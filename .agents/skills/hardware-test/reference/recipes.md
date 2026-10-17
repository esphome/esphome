# Proven test flows

Each flow names the pull requests it was used on, so their result tables show what to post.

## Captive portal `/wifisave` hammer (#20802, #20784)

Reproduces a `/wifisave` arriving during the STA cooldown, and proves a fix on every board.
Device under test: `templates/example-dut.yaml` (bogus network, `ap:` with `ap_timeout: 15s`,
`captive_portal:`). Client: `templates/example-client.yaml` on an ESP32 or a D1 mini; it joins the
device's AP, posts 15 bogus saves 3 s apart, then the real credentials. With the portal up the cooldown
is 30 s, so nearly every save lands inside one. Driver: `scripts/wifisave_hammer.sh`. Settle 40 s on
ESP boards, 90 s on the BW15 (its first attempt on the real network fails, see boards.md).

Counts that matter: `Requested WiFi Settings Change` (saves received), `Exiting cooldown early`, crash
markers, boots (`Log initialized`), and the client's `bogus save status 200` and `real save status 200`.
A healthy run: every save answered 200, one cooldown exit per save (two on the BW15, whose retry enters
the adapter restart), no panic, and the real credentials connect the device within about 10 s.

Fault injection for AP start failures: wrap `this->ap_setup_ = this->wifi_start_ap_(this->ap_)` in
`setup_ap_config_()` with an `HWTEST_AP_FAIL_COUNT` build flag
(`esphome: platformio_options: build_flags: -DHWTEST_AP_FAIL_COUNT=2`), raise the portal's "Captive
portal started" from LOGV to LOGI, and after provisioning curl the device's STA address: connection
refused proves the portal's server is gone.

## `reboot_timeout` with a fallback AP (#9958)

Only one board runs the AP at a time, or stations join the wrong one. AP probe: bogus network,
`ap: {ssid, password, ap_timeout: 5s}`, `reboot_timeout: 60s`, `logger: level: VERBOSE` (ESP32 and
ESP8266 log `AP client connected MAC=` at verbose), `on_boot: logger.log: BOOT`. Station probes:
`reboot_timeout: 0s`, `interval: 150s` running `wifi.disable`, `delay: 20s`, `wifi.enable` (60 s cycles
are too short to cover the due moment). Pass pattern: "Can't connect; rebooting" exactly 60 s after boot
with no station joined, no reboot for minutes while one is, then a reboot within a second of the last
disconnect. The BW15 restarts its AP on every STA retry and cannot be tested this way.

## MQTT (#20433, #17935, #20762, #20767, #20768)

A local mosquitto with `allow_anonymous true`, `log_type all` and `log_dest file`: the broker log gives
each subscription's QoS, the device address (`New client connected from <ip> as <name>-<mac>`) and exact
subscribe counts. Kill and restart mosquitto to exercise `on_disconnect` (reason 0) and resubscription.
Keep `reboot_timeout: 0s` on `api:` and `mqtt:`. The ESP-IDF outbox swings free heap about 2 kB right
after bulk subscribe and unsubscribe; read the heap seconds later over several rounds before calling a
leak. Spawned subscriptions need about 2.5 s to reach the broker before the next publish.
`esphome logs --device MQTT` only uses the MQTT transport when the config has no `api:`. Driver pattern: a
paho script publishes and polls the serial capture file.

## UDP, multicast, sockets (#20556 to #20559, #20585, #20590)

A listener on port 9 needs no privileges on macOS. Test multicast device to device; host to device
multicast never arrives on some access points, and `239.255.x.x` groups are unreliable even to a stock
listener. Broadcast to boards is lossy under power save, unicast is not. Flood at a fixed packet rate
(the host hits `ENOBUFS` unthrottled); 1500 packets per second survived on every board. A forced Wi-Fi
reconnect on the Pico keeps IGMP membership. Set a long loop interval from `wifi.on_connect`
(`App.set_loop_interval(20000)`), not `on_boot`, or Wi-Fi connect crawls. The `logger.log` action
rejects `%.*s`; use a lambda.

## OTA overlap and lock (#20588), OTA timeouts (#19041), deflate (#19037)

All overlap orders (a web upload during a native OTA and the reverse) must be refused; the lock release
after an interrupted web upload waits about 14 s (ESP8266) or 7 s (ESP-IDF) for disconnect detection. A
refused web client on `web_server_idf` never gets the busy reply; the connection drops at reboot.
`pkill -f "a\|b"` does not alternate; use two pkills.

## DHCP lease cache (#17767)

Cycle 20 s awake, `deep_sleep.enter 30s`, wake, 20 s, `App.safe_reboot()`, logging `millis()` at the
`wifi.connected` condition. An RTS hardware reset must fall back to DHCP; a software reboot and a deep
sleep wake must apply the cache. `esphome run --no-logs` silently skipped the upload on one board; use
`upload` then `logs`.

## HDMI-CEC two-board wire (#9189)

Two boards on one wire: promiscuous mode, a 0x8F request answered with 0x90 0x00, a `destination:`
filter on the reply handler, template buttons for each target plus lambdas calling
`send(source, dest, span)` as another initiator. Healthy numbers: device-side turnaround 15 to 22 ms,
round trip 144 to 156 ms. Press buttons over the API; judge timings from device logs (ESP32 light sleep
lags the press).

## LD2450 polygon zones (#20154)

To make a target "leave" without anyone moving, set the radar's Zone-1 numbers over the target and
switch the zone type to `Filter` (the count drops to 0 within 0.5 s); set it back to `Disabled` after.
Writing those numbers blocks the ld2450 loop about 50 ms.

## Scheduler and loop wake A/B (#20356)

An external component whose template button arms an SDK `os_timer` (50 ms, one shot) on the ESP8266;
the callback runs in SYS context, records `millis()` and calls `set_timeout` or `defer`; the timeout
logs the elapsed time and loop count delta. Drive with an aioesphomeapi script (`subscribe_logs` plus
`button_command`). Build the same yaml twice with different names, one per tree.

## Noise handshake and OTA encryption on the ESP8266 (#19062, #19233)

Host side timing is useless on a weak link; use an on-device bench with the minimum of five runs over
two boots. Provision a key with `noise_encryption_set_key` through aioesphomeapi and upload with
`espota2.run_ota(host, 8266, None, bin, noise_psk=key)` when the key is not in the yaml.

## ESP-NOW on the ESP8266 (#14407)

The NONOS SDK only delivers broadcast ESP-NOW frames while the station is associated or a soft-AP is
up; a hidden, open, client-less soft-AP (60 s beacon, DHCP stopped) makes broadcasts arrive. In AP-only
mode the device transmits from the soft-AP MAC.

## Wi-Fi Easy Connect (DPP) enrollee (#17934)

Only an Android phone configures: Settings, network details, Add device, Share Wi-Fi (the Wi-Fi list's
QR scanner makes the phone an enrollee). Render the URI with the `qrcode` module and open the PNG.
`esp32: framework: log_level: DEBUG` plus `sdkconfig_options: CONFIG_WPA_DEBUG_PRINT: y` prints the
supplicant state machine.

## Thread border router (#18104)

See boards.md for the ESP Thread BR board. Check that the H2 answers Spinel before blaming a PR.

## nRF52840 dongle boards (#19009, #20814)

Test config: the two LEDs as inverted GPIO outputs, the button as an inverted pull-up binary sensor, a
10 s alive interval. To retest the nrfutil download path, move the cached `sdk-nrf/nrfutil` aside first.

## online_image stalled downloads (#20594)

One classic ESP32, both frameworks (the ESP32 Arduino build also uses `http_request_idf.cpp`). The host
serves the images: `scripts/online_image/make_images.py <www>` writes 16x16 BMP/PNG/JPEG/QOI plus an 8 bit
palette BMP with a 1078 byte header; `scripts/online_image/oi_server.py <www> 8765 <log>` answers one
misbehaviour per path name (`ok`, `truncated`, `large_header`, `empty`, `stall` holds the socket 40 s,
`closed_early`, `slow` pauses 2.5 s, `chunked`, `chunked_cut`, `noclen`, `huge` claims 20 MB) with an ETag
and a 304 on If-None-Match. `templates/online-image-dut.yaml` has two AUTO-format images (default and
256 byte buffer), API actions `fetch`/`fetch_small` (set_url with `update: false`, then component.update)
and `refetch`/`refetch_small` (component.update only, keeps the cache headers), and a restart button.
`scripts/online_image/drive.py <device ip> <api key> <capture> http://<host ip>:8765 pr|base|base2` runs the
case list over aioesphomeapi and judges each from the serial capture. ESP-IDF facts: a stalled server
ends with `Error downloading image: -28679` (EAGAIN) after the transport timeout, an early close with
`-1`, both before and after the PR; a body without Content-Length and without chunking reads as complete
(Size: 0) and finishes. A build without the fix hangs on the truncated, oversized header, empty, chunked PNG
and cut chunked cases and every later update logs `Image already being updated.`, so a baseline run needs
a reboot per group, a 25 s cap per case, and a driver `timeout` that covers the whole list.

## zwave_proxy frame checksums on a live proxy (#20922)

A Z-Wave proxy (ESP32-S3 with the module on `usb_uart`) is a live device: flash it over OTA and read logs
over the API, there is no serial console because the native USB is the host port for the module, so the
boot-time `GET_NETWORK_IDS` exchange is never captured. Home Assistant's Z-Wave JS holds the single API
subscription (it polls `0x3B` every 30 s and resubscribes after every reboot), so do not subscribe from a
script, and keep the device `name:` so it stays on Z-Wave JS; give each build its own scratch directory
instead.

1. `logger: level: VERY_VERBOSE` with `logs: zwave_proxy: VERY_VERBOSE` and DEBUG for the rest: the
   `Sending:`, `Received frame:` and `CHECKSUM Received ... Calculated` lines are `ESP_LOGVV`, which needs
   the compile-time level.
2. `md5` the two `firmware.ota.bin` files to be sure the A and B builds differ, then `esphome upload
   --device <name>.local` and a `timeout 330 esphome logs --device <name>.local` capture started right after
   the OTA to catch the resubscribe traffic.
3. `scripts/zwave_proxy/verify_checksums.py <captures>` recomputes the XOR (init 0xFF, LENGTH..payload) over
   every logged frame and counts bad checksums and ACK/NAK sent and received; `scripts/zwave_proxy/devinfo.py
   <name>.local` prints the firmware version, compile time and the Home ID from the API (a Home ID proves the
   device-built `GET_NETWORK_IDS` frame was accepted at boot).
4. Pre-existing: one `zwave_proxy took a long time for an operation` warning on the first frame after the
   log client connects, caused by VERY_VERBOSE logging; it appears on dev too.

## usb_cdc_acm on the nRF52840 dongle (#11634)

`templates/usb-cdc-acm-dongle-dut.yaml`: logger on USB_CDC port 0, `usb_cdc_acm` interface 0 disabled,
interface 1 echoing from a 20 ms lambda, 0x01-prefixed commands for burst, stream, flush, peek, line coding
and counters. Build and flash with `dfu: true` through the app port; the dongle then shows three CDC ports:
the board DTS node `cdc_acm_uart` (unused), `cdc_acm_uart0` (log) and `cdc_acm_uart1` (data), in that order.
Capture the log port with `serial_capture.py`, drive the data port with `scripts/usb_cdc_acm/drive.py
DATA_PORT` (11 cases, prints PASS/FAIL and a summary) and grep the log for `Opened`, `Closed`, `buffer full`,
`flush result`, `line coding`. Boot lines are held in the driver's 1 KB ring until the host opens the port, so
a capture started later still shows the first 1 KB of the boot and then a truncated line.

With three CDC instances the Zephyr legacy stack's `CONFIG_USB_MAX_NUM_TRANSFERS=4` starves the last instance
(each instance holds an OUT read and an IN ZLP), so interface 1 shows DTR changes but moves no data.
`scripts/usb_cdc_acm/cdc_probe.hpp` (include it, call `cdc_probe()` from an interval) reads the Zephyr driver
directly to tell a dead endpoint from a component bug. Raising the transfer pool to 8, or disabling the
board's `cdc_acm_uart` node, fixes it. Design limit of the driver: `fifo_fill` holds `tx_ready` until the
write callback and `write_array` drops when the ring is full, so a 64 KiB full-duplex echo loses one 64 byte
tail on every buffer size.

## Zephyr preferences on the nRF52840 dongle (#20810)

`templates/nrf52-preferences-dut.yaml`: logger on USB_CDC at VERBOSE, `flash_write_interval: 5s`, one
`globals` with `restore_value` incremented every 15 s. Give each build its own global id so each first boot
sees a never-saved key (settings survive a reflash). Flow per build: flash through the log port, 40 s capture
of the first boot, then start a second capture and reboot with a 2001 baud touch on the dongle's other CDC
port (the dfu rate callback is global, and the capture holds the log port); `serial_capture.py` reopens the
log port after the USB re-enumeration and gets the buffered boot lines (`load setting`, `Loaded N settings`).

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

## cse7766 on a Sonoff S31 (#20948)

`templates/s31-cse7766-dut.yaml`: `logger: baud_rate: 0` because the CSE7766 owns the UART, VERBOSE logger
with everything but cse7766 quieted, 2 s throttles, and a `globals` frame counter fed by the voltage sensor's
`on_raw_value` and logged every 10 s (about 96 frames per 10 s). OTA only, every reflash reboots the relay.
`scripts/cse7766/run_obs.sh IP SECONDS LABEL [OUTDIR] [NAME_FILTER]` subscribes to states and logs over the
API with a concurrent ping and prints the frame counter, API gaps and warnings. API state gaps of 10 to 20 s
can appear on a weak Wi-Fi path while the device counter stays flat, so judge a UART parser by the
device-side counter, not by the API cadence. With nothing plugged in, current and power read 0.

## zephyr_coredump crash and decode on nRF52 boards (#13122)

`templates/nrf52-coredump-crash-dut.yaml` (dongle; change `board:` to `adafruit_itsybitsy_nrf52840` for the
ItsyBitsy): `zephyr_coredump:`, `dfu: true`, a `restore_value` global keyed by the `run` substitution
(settings survive a reflash, so bump `run` for every new crash), and an `on_boot` sequence that sets the
flag, yields 3 s so the globals loop saves it (a blocking lambda never gets there and the board crash-loops),
syncs preferences, then writes to 0xDEADBEEF. Flash, keep every port closed for 80 s, then
`scripts/zephyr_coredump/flow.sh LABEL PORT ELF [OUTDIR]` does the three opens (3 s cut, 100 s full print,
20 s reopen) and calls `decode.sh`, which keeps the first complete `#CD:BEGIN#..END#` block, runs Zephyr's
`coredump_serial_log_parser.py` and `coredump_gdbserver.py` from the sdk-nrf ESPHome installed and prints
`bt` and `info threads` with its `arm-zephyr-eabi-gdb`. The ELF is `.pioenvs/<name>/zephyr/zephyr/zephyr.elf`.
Expect about 9 KB dumps, 140 to 150 `#CD:` lines, a 673 ms loop warning for the erase, and `Has stored
dump: 0` on the reopen. Check `compiled on` in every capture: another session flashing the same board
mid-test looks exactly like a failed test otherwise. Boards without a network cannot exercise the API and
MQTT delivery paths of the component; say so in the results.

## LibreTiny rtc preference emulation (#21007)

The PR keeps `storage: rtc` preferences in FlashDB and deletes them on the first load or save after a boot
whose `lt_get_reboot_reason()` is not software, watchdog, crash or sleep. Only the families listed in
`_libretiny_rtc_supported()` accept the config keys; on others force the backend in with
`platformio_options: build_flags: ["-DUSE_LIBRETINY_RTC_PREFERENCES"]` after confirming the rejection.

- Harness: `scripts/libretiny_prefs/ltprefs.hpp` plus `templates/bk72xx-rtc-prefs-dut.yaml`. `ltprefs_boot()`
  loads one rtc and one flash preference, logs the reason and both results, saves `100 + step`, syncs; after
  a templated delay `ltprefs_exit()` bumps the flash-backed step and runs the step's action (`lt_reboot()`,
  `__builtin_trap()`, `lt_reboot_wdt()` or idle). The last 8 boot reports are kept in a flash preference and
  repeated in the 10 s alive line, so a boot missed by the capture (a power cut, an OTA-only board) is still
  readable. Bump `run:` for fresh keys.
- A store to an unmapped address does not fault on the ARM968, the board carries on: use `__builtin_trap()`
  for the crash row. The SDK crash handler reboots about 10 s later. Works on BK7238 and BK7231N.
- Hard resets: `serial_capture.py ... --reset` pulses RTS on CEN (reported as Power-On). A real power cut
  needs a hand or a smart plug; the history in the alive line covers the boot the host cannot see. On the
  Beken SDK a chip-enable reset and a power cut both report Power-On, there is no separate reset-pin source.
- Step 0 and step 8 wait 30 s before acting so the host can open the port (and pulse a reset) after a flash
  without losing the first boot; the flash tool's own reset starts the sequence immediately otherwise.
- OTA-only board: keep the device's wifi, api and ota blocks verbatim, keep safe_mode, read logs with
  `esphome logs <yaml> --device <name>.local` (the API attaches late, hence the history), and restore the
  device's own config afterwards.
- Plain-build comparison: `raw_firmware.bin` differs by the timestamp and by `__LINE__` in log calls of
  functions below the added code; map offsets through `objdump -h` load addresses and `addr2line` before
  calling a byte difference a code change.
