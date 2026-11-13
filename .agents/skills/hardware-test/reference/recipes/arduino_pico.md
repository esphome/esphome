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
