# Xiaomi BLE

## Xiaomi, ATC and PVVX parsing without the sensors (#21483)

No Xiaomi devices are needed, and one ESP32 cannot hear its own advertising, so the frames are injected into the
tracker as scan results: everything after the radio runs as normal.

- `scripts/xiaomi_ble/gen.py` writes `frames.hpp` and `expected.json`: plain MiBeacon (0x50), encrypted v5 23 byte
  with MAC and 19 byte without, real AES-CCM (`cryptography` AESCCM, tag 4, AAD 0x11, nonce = MAC reversed + pid +
  cnt + 3 byte ext counter), ATC (13 byte, big endian) and PVVX (15 byte, little endian) 0x181A frames, a corrupted
  tag every other cycle and a counter 0 boot frame. It models the old shared duplicate counter (`base`) and the per
  device one (`pr`) and prints the expected publish counts.
- `xbinject.yaml` (ESP-IDF devkit) includes `frames.hpp`; an `interval:` builds a `BLEScanResult` and calls
  `global_esp32_ble_tracker->gap_scan_event_handler(r)`. `count.py` counts the `Got Xiaomi` blocks (sensor
  publishes are not logged at DEBUG) and compares them with `expected.json`.
- On ESP32 the tracker's own triggers sit in `listeners_`, which runs before `neutral_listeners_` (the
  ble_device_base devices), so an `on_ble_service_data_advertise` never sees what a Xiaomi listener did to the
  shared data. Check that directly: `xb_rewrite_probe(id(xa))` builds an `ESPBTDevice` with `parse_scan_rst()`,
  calls the component's `parse_device()` and logs the service data before and after (`[probe` lines).
- `run.sh inject pr|base` builds, flashes, captures and prints PASS when the counts match that build's model and,
  on the PR build, the probe's service data is the same before and after `parse_device()`.
  Base dropped a second device's frame with the same counter, the frame after a bad tag and a first frame with
  counter 0, and rewrote the shared data in place; the PR fixed all of them.

## Over the air with a second ESP32

- Advertiser: an ESP32-S3 with Ethernet (OTA over Ethernet), `xbadv.yaml` with `adv_frames.hpp` from
  `gen.py --ota`. Per frame: stop advertising, `esp_ble_gap_set_rand_addr`, `config_adv_data_raw`, start
  non-connectable advertising with `own_addr_type` random, 20 ms interval, 250 ms dwell. Random static addresses
  need the top two bits set, so these use C4:C1:38 instead of A4. `esp32_ble:` alone brings up the stack. The
  web_server button starts a run: `curl -X POST -d "" http://<advertiser>/button/start/press` (no body gives 411).
  Its `failed sends` sensor counts only GAP calls rejected when queued; a frame the controller drops later shows
  up as missing publishes in `count.py`, the same as radio loss.
- Receiver: `xbrecv.yaml` on the devkit, passive scan with window = interval = 100 ms. `run.sh ota pr|base` (with
  `ADV` set) flashes it, resets it with a capture and presses start 18 s later, so the counter 0 boot frame is the
  first one it hears.
- Each frame arrives about 4 times; the counts matched the injection run exactly apart from one radio loss, so the
  over the air comparison allows two missing publishes per tag.

## Every Xiaomi platform at once (#22727)

- `gen_all.py` writes `frames_all.hpp`, `xball_entities.yaml` and `expected_all.json`: one device per platform
  (the 20 listener-only platforms, `xiaomi_mue4094rt`, `xiaomi_rtcgq02lm` hub, `xiaomi_body_scale`, ATC, PVVX),
  10 cycles, every frame sent twice. Each entity logs `PUB <dev>.<key> <value>` on publish (binary sensors with
  `trigger_on_initial_state: true`). Frame shapes that parse: long encrypted (fc 0x58) only up to 24 bytes, so
  a 7 byte `th` object needs the short 19 byte frame, which only lywsd02mmc, xmwsdj04mmc, mjyd02yla, cgpr1 and
  rtcgq02lm accept; hhccjcy10 is FD50 9 bytes and miscale v1 181D 10 bytes (no counter, the repeat publishes
  again); mue4094rt is a plain fc 0x40 frame; body_scale an S200 0x4E16 object in a 24 byte short frame.
  `xiaomi_mccgq02hl` publishes 9 of 10: the first frame is "closed", which equals its initial off state.
- `xball.yaml` is an ESP32-S3 (ESP-IDF, no network, USB_SERIAL_JTAG log) that injects one cycle per second from
  20 s and logs free heap before and after (`IDLE` lines). `run_all.sh TREE NAME PORT` builds, flashes, captures
  and counts; `count_all.py cap1 cap2 ...` also says whether the values are identical across builds.
- `xbsetup_priority.yaml`: `esphome config` with `setup_priority:` on a moved sensor, a moved binary sensor and
  mue4094rt (still a component); remove one at a time, since validation stops at the first platform error.
- On #22727: 441 publishes on the PR, its stacked base and dev, identical values; dump text identical, the PR prints
  the 20 listener dumps after `BLE Tracker:`.
