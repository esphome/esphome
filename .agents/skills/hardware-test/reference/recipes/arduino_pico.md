# arduino-pico framework bumps (RP2040/RP2350)

Scripts: `scripts/rp2_web_ble/` (README there). Used for #22671 (6.1.1 to 6.3.2).

- Build PR and dev with a private `PLATFORMIO_CORE_DIR` (`scripts/pio_build.sh` does; see `reference/boards.md`).
  Check the compile log line `framework-arduinopico @ 1.60302.0` per build.
- Wired Ethernet (W5100S EVB): 6.3.2 defines `LWIP_ASSERT_CORE_LOCKED()` as `async_context_lock_check` on the
  Ethernet context (`cores/rp2040/lwip_wrap.cpp`), so unlocked lwIP calls panic there. Soak the API (200 ms template
  counter, 1 s uptime, logs subscribed), load pages, OTA and reboot; the rp2 crash handler shows a panic as
  `Crash (HardFault)` in the debug reset reason on the next boot.
- Fallback AP client count: `ap_clients_` is protected and `WiFiComponent` is final; read it from an
  `esphome: includes:` header through an explicitly instantiated member pointer (`appeek.hpp`). Join raises it to 1.
- bluetooth_proxy on the Pico W: `rp2_ble_tracker:` + `bluetooth_proxy: active: true`. Drive with `gatthold.py`
  against the ESP32-S3 server in `scripts/ble_notify/server.yaml`. UUID forms and the 0x3E connect failures are in
  `bluetooth_proxy.md`. A "Failed to send notify data response" warning at each disconnect is normal.
- web_server SSE on the W5100S (dev 2a6a1a050e4, 6.1.1): every `/events` stream stops after 0.3 to 11 s (alone, with
  the API, or with page loads alike), then the EVB closes it with a FIN 5.8 to 6.3 s after its last data segment, so
  curl exits 0. The pcap shows every byte ACKed by the host, window 65535, no retransmits, nothing sent in the gap:
  the device simply stops writing. The 5 s gap matches RPAsyncTCP `ASYNC_MAX_ACK_TIME` (likely `_pcb_busy` not
  cleared, then `_onTimeout` closes). A new `/events`, GET / and the API keep working. Pico W: 60 s steady. On macOS
  `tcpdump` runs without sudo once your account is in the bpf group. Tools: `sserun.sh HOST TAG SECS alone|api|pages
  N` (pcap, netstat poll, API VERBOSE log, probes) and `pcapsum.py PCAP DEVICE_IP` (per connection last data, FIN/RST,
  retx). Confirmed with counters patched into the build's `.piolibdeps` copy of RPAsyncTCP: every stall is one
  `_sent()` underflow with `_tx_unacked_len` 0 and the pcb idle after `send()`; ESPAsyncWebServer 3.12.1 does not
  help. The esphome-libs RPAsyncTCP fork (`esphome-1.3.2`, 8efb83c) streams 16 x 30 s with no stall. `stallctr.py PCAP
  IP APILOG` lines the counters up with each stall.
- `H=<device ip> churn.sh TAG SECS` (4 loops of 0.2 to 1.5 s `/events` connections) crashes the rp2 web_server: dev on
  the EVB, the fork build on the EVB and the Pico W. HardFault in `AsyncEventSource::send` from
  `deferrable_send_state`, about one reboot every 10 to 20 s. Ten quick reboots put the board in safe mode (ping and
  OTA port 2040 only); OTA a build to recover.
- **Never flash the W5100S EVB a build without `interrupt_pin`** unless you can reach BOOTSEL: arduino-pico then polls
  the W5100 from an async_context timer that every lwIP call pushes back, so the esphome OTA read loop starves the
  poller and OTA never completes (the device ACKs the hello, then never the features byte). Web and API still work.
  The board has no USB CDC port with ESPHome firmware, so the only way back is BOOTSEL and the UF2 drive.
- `churnflow.sh` (in `rp2_web_ble/`) repeats `churn.sh` with heap and main loop checks and reflashes by OTA when a
  round left the board in safe mode.
- `leakflow.sh` with `neverconn.py HOST N rst|head`: ESPAsyncWebServer 3.9.6 leaks about 175 B per `/events` client
  that resets before the response head is ACKed (52 KB per 300); 3.12.1 stays flat.
- `/events` churn also hangs the BK7238 main loop or drops it off the network on dev; recover with a serial reset.
- The API has 3 sockets on these builds: connstress at concurrency 4 always shows some "dropped after hello" failures;
  use 2 for a clean 500/500.
- IRQ context scheduler test for #22688: `scripts/rp2_sched_lock/`, described in `scheduler.md`.

## USB NCM Ethernet on RP2 (`ethernet: type: USB`, #23138, #23156)

Scripts: `scripts/rp2_usbncm/`. Results: https://github.com/esphome/esphome/pull/23138

- An RP2040 board with `type: USB` shows up on the computer as a new Ethernet interface by itself, next to the USB CDC
  log port (picotool upload still works). Give the device a static link-local `manual_ip` (169.254.x.y/16, any
  gateway): a macOS host falls back to a self-assigned link-local address about 17 s after enumeration, nothing to set.
- macOS trap: with Wi-Fi also holding a 169.254 route, the host sends the device's address out Wi-Fi after each device
  reboot (ARP incomplete on the Wi-Fi interface) for 60 to 110 s. One `ping -b <usb interface> <address>` puts the ARP
  entry on the USB interface and everything works at once; `cycle.sh` does that before each check. Do not touch routes.
- Baseline (#23138, arduino-pico 6.3.2): a 10 min API stream at 200 ms with no gaps, 1200/1200 pings at 1.3 ms, OTA of
  240 KB in 2.6 s. 6 of 7 OTA reboots panicked in TinyUSB (`ep 00 was already available`, rp2040_usb.c:108) on
  shutdown, 0 of 8 button restarts. 6.4.0 (#23156) still panics on 9 of 11 OTAs, so the bump does not fix it.
- Panic message capture: `-Wl,--wrap=panic` plus a `__wrap_panic` that formats into
  `__attribute__((section(".uninitialized_data.x")))` buffers, which survive the watchdog reboot, and logs them next
  boot (`boot_recorder.hpp` does this); the rp2 crash handler alone only gives PC `_exit`, LR `panic`.
  Uninitialised RAM also survives a picotool USB flash.
- About 1 OTA in 10 leaves USB (NCM and CDC) silent for about 5 min until a watchdog reset. That is ESPHome safe mode,
  not a USB fault: OTAs less than 60 s apart each count as a failed boot, after 10 the device runs safe mode for 300 s
  (ping and OTA port up, API closed), then `App.reboot` (watchdog reset reason, no `on_shutdown`). Space OTAs 65 s apart
  in loops (`cycle.sh ... spaced`).
- The 2.5 s boot hang (about 1 boot in 10) is the host sending SET_INTERFACE alt 1 then alt 0 to the NCM data
  interface: TinyUSB never starts IN, 8 frames fill the one NTB and `NCMEthernet::_try_process_xmit_queue` spins until
  the watchdog. A 20 ms bounded wait plus drop leaves the device offline (host interface inactive) instead.
  `boot_recorder.hpp` with `usbncm-rec.yaml` records an event ring, USB counters and loop snapshots across the reboot,
  including safe mode (`on_safe_mode` hook); `cycle.sh NAME usbncm-rec.yaml N TAG ota fast|spaced` collects boots and
  `evring.py CAPTURE alt0|notif` reads the ring.
- The macOS log names the culprit, no sudo needed: `hostlog.sh OUT SECONDS` streams the kernel and configd lines. The
  NCM driver logs `AppleUSBNCMData::setupDataTransfers` (alt 1) and `updateLinkStatus: linkStatus 1` (connection
  notification). A bad enumeration shows `IOUSBHostPipe::abortGated ... endpoint 0x83: aborting 16 requests` 0 to 5 ms
  after setupDataTransfers while the device is still attached, no linkStatus 1 and an inactive interface; the device
  sees alt 0 at the same moment. `hostorder.py HOSTLOG` lists every enumeration with ABORT and NO-LINK, so the host log
  alone counts alt 0 events. Not caused by notification timing nor by the LowDataMode order. Rate seen: about 1 in 11
  boot enumerations, higher when the device re-enumerates every 12 s.
- Proposed arduino-pico fix in `ncm-alt0.patch`: bounded transmit wait plus drop, `isLinked()` follows the data
  interface alt setting through a wrapper around `netd_control_xfer_cb` in `libraries/tusb-ncm`, and the device
  re-enumerates after 3 s at alt 0 with the wait doubling up to 60 s (a 3 try cap was not enough: the hammer hit 4 alt 0
  enumerations in a row). Test it as patched copies of lwIP_USB_NCM and tusb-ncm under `esphome: platformio_options:
  lib_extra_dirs:` (`usbncm-patched.yaml -s ncm_libs DIR/libraries`, steps at the top of the patch, logs NCMLINK on each alt
  change); check with `nm` that
  `__sdk_netd_control_xfer_cb` and `tud_network_ncm_data_alt_cb` are in the image.
- `usbncm-hammer.yaml` (`tud_disconnect`, 500 ms, `tud_connect` every 12 s) collects alt 0 events fast; the host needs
  about 17 s per enumeration to self-assign link-local, so ping only works with a slower hammer.
- OTA interrupt rounds (#23156): build each variant twice with `-s tag A|B` (`esphome: comment: "img ${tag}"`), OTA
  with `upload --file`, check the running image with `scripts/ota_timing/apiinfo.py HOST[=build_info.json]`, and kill
  the client with SIGKILL once the device log shows `Progress: N%` past a target. The device logs "Timeout reading 1040 bytes" and keeps the
  old image. Never run two USB uploads at once: picotool refuses with "requires a single RP2040 device" and leaves both
  boards in BOOTSEL (copy firmware.uf2 to the UF2 drive to recover).
- BLE GATT connect from the rp2 proxy to an ESP32-C6 `esp32_ble_server` fails on 6.3.2 and 6.4.0 (status 0x02 or no
  answer) while a computer connects; use the S3 `ble_notify/server.yaml` peer for connects. `network: enable_ipv6`
  plus `web_server` does not build on rp2.
- Linux host facts (from the source, not tested here): `cdc_ncm_bind_common` toggles the data interface alt 1, 0, then
  1 again after 10 to 20 ms; usbnet submits RX URBs only while the netdev is up, so an interface left down never reads
  IN (the same transmit hang on the device); unbinding returns the interface to alt 0.
