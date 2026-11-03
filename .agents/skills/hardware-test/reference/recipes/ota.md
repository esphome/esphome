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

- LibreTiny defines `micros` as a macro, so the header calls `(esphome::micros)()`.
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
