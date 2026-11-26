# ota

## OTA overlap and lock (#20588), OTA timeouts (#19041), deflate (#19037)

All overlap orders (a web upload during a native OTA and the reverse) must be refused; the lock release
after an interrupted web upload waits about 14 s (ESP8266) or 7 s (ESP-IDF) for disconnect detection. A
refused web client on `web_server_idf` never gets the busy reply; the connection drops at reboot.
`pkill -f "a\|b"` does not alternate; use two pkills.

## OTA upload timing A/B (#21639, #21641, #21642)

`scripts/ota_timing/run.sh N PHASE...` times native OTA uploads per build (phase) and per board. It builds every
board from `TREE_<PHASE>`, does one warm-up upload per board (it installs the phase's build and is left out of the
results), checks over the API that each board reports the compile time from that build's `build_info.json`
(`apiinfo.py`), then runs N rounds of every mode on every board. Boards are interleaved per round so they share the
same air; put a nearby ESP32 on the same Wi-Fi in `BOARDS` as a network control. `summ.py` prints medians: never
judge from single runs, slow Wi-Fi gave 33 to 235 s for the same upload in one night.

- `otatime.py` drives `espota2.run_ota()` from the tree on `PYTHONPATH` and prints one `RESULT` line with the
  `Update took` split, the compressed size, whether UDP was used and the time at each tenth of the upload.
  An upload that leaves no `RESULT` line (board gone, script crash, timeout) is recorded as `rc: -1`, and `summ.py`
  fails when a board and mode has fewer than N rows or an upload time cannot be read.
- Boards with an `api: encryption: key` also encrypt OTA: set `API_KEY` for `otatime.py` and `apiinfo.py`. LibreTiny
  LN882H listens for OTA on 8820, not 8892; BK72xx and RTL87xx use 8892.
- Raw mode leaves `CLIENT_FEATURE_SUPPORTS_DEFLATE` out of the client's offer. Never switch deflate off after the
  offer (`if deflate and 0:`): once the device accepted deflate it expects a deflate stream.
- `-tcp` modes make `espota2._loss_reason` return None, so the client asks to stay on TCP. Clearing
  `CLIENT_FEATURE_SUPPORTS_UDP` instead desyncs the handshake (`receiving UDP token response: timed out`).
- LibreTiny trimmed UF2 (#21642): the CLI sends what `esphome.components.libretiny.uf2.device_ota_uf2()` returns for
  `firmware.uf2`; that module only exists with #21642, so on older trees the full UF2 goes out. Compare the sizes
  in the `Compressed to` lines, and rely on the API compile time check to show that the trimmed image booted.

`receiving chunk result: Unknown error from ESP` is error 0xFF sent by the device, not a dropped connection. The
device sends it for a decode or flash write error, and also when no data arrives for 105 s, so on a weak link it
usually means a stall. A failure about 105 s after the last progress, or the same error on a raw upload, points to
the link; to be sure, capture the device log (`Inflate err`, `Inflate overrun`, `No data received`).

Results: https://github.com/esphome/esphome/issues/21639#issuecomment-6409628227 (dev and LibreTiny 1.14.0 on a
CB3S), https://github.com/esphome/esphome/pull/21642#issuecomment-6412972562 (trimmed UF2 on CB3S, BK7238 and BW15),
https://github.com/esphome/esphome/pull/21641#issuecomment-6412976859 (new decoder on the same three).

## On-device inflate benchmark (#21639, #21641)

`scripts/ota_timing/inflate_bench`: `gen_streams.py NAME=IMAGE...` cuts a 128 KB slice from each image and writes
`bench_streams.hpp` with raw deflate streams (level 9, 4 KB and 32 KB windows). `bench.hpp` inflates every stream with
each ring size at least the encoder window, three times, and logs the best time plus input and output rates, then
`BENCHDONE`. `bench.sh CONFIG DEVICE` builds and uploads `bench-esp32s3.yaml` or `bench-bk72xx.yaml`, collects one pass
over syslog and fails when a case did not reach the end of its stream.

- Keep the streams near 130 KB on a BK7231N: an image with about 330 KB of extra incompressible data failed its
  OTA with "Writing OTA data to flash memory failed". Use one image and `--windows 12` there.
- `bench.hpp` targets the decoder interface with `dest_limit`. #21641 drops it (the output runs into the window up
  to `dest`); after that lands, remove the `dest_limit` line.

Results: https://github.com/esphome/esphome/issues/21639#issuecomment-6409786224

## Upload without a configuration (#21738)

`esphome upload --file IMAGE --device IP` with no YAML picks the OTA port from the image header and takes the password
from `--ota-password` or `ESPHOME_OTA_PASSWORD` and the key from `ESPHOME_OTA_KEY`. Test each chip family with its own
current image (`build/firmware.ota.bin` for ESP-IDF, `.pioenvs/<name>/firmware.bin` for ESP8266, `firmware.uf2` for
LibreTiny) and check the build over the API; switch one board between two builds to show a new image lands.

Point every case that should be refused before connecting at an unroutable address (192.0.2.1), never at a board: a
refusal prints no `Connecting to` line, and an image that slips through cannot reach a device. An ESP32-S3
`firmware.factory.bin` slipped through on 2026-10-27 (its bootloader header at offset 0 looks like an app), the device
reported `OTA successful` and its bootloader rejected the slot at boot; send the board its proper app image afterwards.
A factory image has the partition table magic `AA 50` at 0x8000 and the app at 0x10000.

## OTA automations on two OTA platforms (#21903)

`scripts/ota_automations/ota-automations-common.yaml` (a package) puts `esphome` and `web_server` OTA in one config,
each with all six automations (`on_begin`, `on_progress`, `on_end`, `on_abort`, `on_error`, `on_state_change`)
logging a platform tag and the argument. `run.sh esp32|esp8266 LABEL` builds, flashes by serial, and runs four cases
under one capture: a native upload, a native upload whose uploader is SIGKILLed after the first progress fire, a
web_server upload of a random 64 KB file and a web_server upload of the real image. `summ.py OUT_DIR` prints each
case's fires in order with progress collapsed to a count; diff dev against the PR.

- `web_server: ota: false` removes `/update` (404), so leave it out when the web platform is under test.
- A killed native uploader ends in `state 4` and `error 255`, not `abort`. `on_abort` only comes from a web upload
  cut on the network. On ESP8266 those happen by themselves (1 in 4 uploads on dev), so count them on both builds with
  `webrep.sh IP IMAGE COUNT` before blaming the PR.
- A random file through web_server gives `error 128` on the ESP32 and `error 132` on the ESP8266.
- A SIGTERM to the uploader lets it finish the transfer; use SIGKILL for the native error path.
- Results: https://github.com/esphome/esphome/pull/21903#issuecomment-6516529409

## Signed OTA with several trusted keys (`verification_keys`, #22398)

`scripts/signed_ota/` on an ESP32-C3 (`signed-ota-c3.yaml`, `signed_ota_verification: signing_scheme: rsa3072,
verification_keys: [keyA.pem]`):
1. `python -m espsecure generate-signing-key --version 2 --scheme rsa3072 keyA.pem` (and keyB).
2. Build four tagged images with `esphome -s tag <name> compile`, serial-flash the first factory image (no on-boot
   signature check without secure boot), copy each `firmware.ota.bin`.
3. `sign_variants.py <image> keyA.pem keyB.pem <prefix>` writes key A, key B, B then A, stored CRC flipped, byte 2
   changed (covered only by the CRC), the same with the CRC recomputed by zlib, and B then A with block 1's CRC flipped.
4. `otaseq.sh <ip> FILE=reject... FILE=<tag>...` sends them with `esphome upload --file` and checks the first verdict
   in the device log and the upload result against the expectation (a rejection, or the `BUILDTAG` of that build);
   send the rejects first so every check runs in the same build, then the accepted ones.
Expected: unsigned, B only and every CRC edit rejected; A, B+A and the zlib-fixed CRC accepted. The byte 2 pair isolates
the CRC from the signature and key digest.

Gotchas: an ESP-IDF image can contain `E7 02` at a 4 KiB boundary by chance; find the signature sector by the CRC, as
`sign_variants.py` does. An unsigned image sent after a signed one can be rejected with "trusted key failed to verify"
instead of "no signature block": the partition still holds the old signature sector past the new image's end.

Results: https://github.com/esphome/esphome/pull/22398#issuecomment-6716330334

## ESP8266 web_server OTA interruption and SSE (#22682)

`scripts/web_server/async_bump/otacases.py` (README there). Facts from a D1 mini on dev and on ESPAsyncWebServer 3.12.1:
- A cut upload logs `OTA upload interrupted; aborting`, keeps the old image and serves again within a second; the
  next full upload works without waiting for a lock release. No heap lost per cut.
- Uploads stop by themselves on the network about 1 in 6 to 1 in 15 (curl `000`, old image kept), and about 1 in 25
  boots the new image while curl never gets the reply (it waits 30 to 60 s). Both builds; classify, do not fail on it.
- At OTA reboot the device does not close `/events`: a held client sees silence, rarely a FIN. Detect by idle time.
- Max free block drops from about 45 kB to about 37 kB after the first OTA and stays (both builds).

## Rollback self-test and ESP-IDF 6 TLS (#22902, #22930, #22931)

- App rollback can be tested without a bad build: a `crash_major` global with `restore_value: true`, set over the API,
  and an `on_boot` priority 700 lambda `if (id(crash_major) == ESP_IDF_VERSION_MAJOR) abort();` (priority 800 runs
  before globals are restored). Arm it, OTA a build of the other IDF major, and the new image aborts before it is
  marked valid, so the bootloader must go back to the old one. `esp_ota_get_bootloader_description()` reports which
  IDF built the flashed bootloader; an OTA never replaces it.
- ESP-IDF 6 trims SHA-384 from mbedTLS by default, so https to any server whose chain is signed with SHA-384 fails
  verification (-0x3000 or -0x2700) unless `CONFIG_MBEDTLS_SHA384_C: y` is in `sdkconfig_options`.
- A local TLS 1.3 test server: a threaded Python `ssl` server with `minimum_version = TLSv1_3`; for a server that
  offers only FFDHE groups, point `OPENSSL_CONF` at a config with `Groups = ffdhe2048`.
- Classic ESP32 boards on a busy network can lose 0 to 98 % of pings from run to run with either build while C3, C6
  and S3 boards answer every ping; compare builds there only with both runs at the same time on two boards.

## RP2 preference write after a successful OTA (#23179)

Config with a raw `ESPPreferenceObject` in a global (made in `on_boot`), saved plus `global_preferences->sync()` in
`on_end` (runs in the main loop before `safe_reboot()`), a probe save at the first `on_progress` (`on_begin` fires
before the backend `begin()`, so it is not blocked yet), and a rewrite in `on_error`. Pico CDC misses the boot lines:
store the boot values in globals and print them from a 10 s interval with a per-image `-s marker` substitution.
Staging is LittleFS (1 MB, ends at `_EEPROM_start`, the last 4 KB sector); a 500 KB random `PAD_BLOB` header gives a
~970 KB image, ~820 KB compressed. Dev: `on_end` sync false every time; PR true. A killed upload on rp2 can wait
105 s for `No data received` before `on_error`. Result: https://github.com/esphome/esphome/pull/23179#issuecomment-7108892214

## Damaged web_server OTA uploads (#23225)

`scripts/web_ota/shortpost.py HOST IMAGE MODE [CUT] [OFFSET]` sends a curl-shaped multipart POST to `/update`. MODE
`full`, `close`/`hold` (stop CUT bytes early), `dropmid` (drop CUT bytes at OFFSET, still send the closing boundary,
which is what a lossy relay produces), `shortcl` (Content-Length matches the short body), `flip` (one bit at OFFSET).
`runcase.sh LOG HOST IMAGE LABEL MODE...` runs one case while `serial_capture.py` writes LOG, retries uploads that
Wi-Fi cut before the final chunk, and greps the serial log.

- ESPAsyncWebServer fires `final` on the closing boundary even with bytes missing, so only the backend check stops it.
  On dev before #23225 an ESP8266 flashes the short image, eboot prints `@cp:B0` `e:3`, and the board is silent until a
  serial reflash. ESP-IDF rejects the same uploads in `esp_ota_end` (0x1503). A gzip image still skips the ESP8266 check.
- A close is seen by the device a few seconds late, so wait about 30 s before the next upload or it is refused as busy.
- `cp.yaml` starts the captive portal on the station network (`global_captive_portal->start()`), so its upload path
  can be tested without an AP client.

## OTA over a relay with network drops (#23218)

`scripts/ota_relay/` puts the host on a network only a board can reach: `relay.hpp` relays TCP from a serial link
(USB Serial/JTAG, or UART0 at 921600 with `-DRELAY_UART`) to one address, two channels, and `relay_host.py PORT
LPORT:TPORT:RATE [SPEC]` serves each channel on `127.0.0.1:LPORT`, paced to RATE bytes per second (8 to 15 KB/s makes
an upload last about a minute).

- Upload through a fallback AP: `relay-sta.yaml` (classic ESP32) joins the AP of `dut-ap.yaml` (an ESP8266 that never
  finds its network) and relays to 192.168.4.1. `webrun.sh` starts each upload a few seconds after the DUT's retry
  line, so the next retry falls due mid upload; `windows.py` counts retry and warning lines per upload.
- Network drop under an upload: `relay-ap.yaml` (S3) is the open test network and relays to `dut-sta.yaml` (C3,
  static address, own AP always on). Writing a number of ms to `drop-ms` next to `relay_host.py` renames the AP for
  that long (stations are kicked and cannot find it, the relay side stays up); `droprun.sh` drops it mid upload,
  `dropana.py` prints the time from each drop to the upload end, disconnect, first scan and reconnect.
- On ESP32 a station drop ends an upload at once: the web upload aborts (or after the 5 s receive timeout) and native
  OTA fails its next write with errno 113, so the OTA is over before Wi-Fi handles the disconnect.
- A WPA2 test AP with protected management frames delays drop detection by 6 to 11 s (`SA query timeout`); use an
  open AP for crisp drops.
- Native OTA blocks the main loop for its whole data phase, so Wi-Fi cannot retry during a native upload either way.
- The relay once lost a 1 KB frame and the ESP8266 flashed the short image as a good web OTA (boot loader `e:3`,
  serial reflash), which is the bug #23225 fixes, so every DATA frame now carries a sequence number and checksum and a
  lost frame closes the upload.
- Result: https://github.com/esphome/esphome/pull/23218
