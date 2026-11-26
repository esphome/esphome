# esp32_hosted

## Wi-Fi throughput A/B on the ESP32-P4-WIFI6 (#23122)

`scripts/tcpbench/` measures what a Wi-Fi tuning change does on a real link. The `tcpbench` external component
(ESP-IDF) asks for high performance networking, logs the Wi-Fi buffer sdkconfig values at boot and the internal heap
every 10 s, and runs three lwIP socket tasks: a TCP sink on 5001 (answers the byte count and time at EOF), a TCP
source on 5002 (sends for the requested seconds) and a UDP counter on 5003. `bench.py HOST rx|tx|tcpburst|udpburst`
drives one test and prints a RESULT line.

- `bench-nopsram.yaml` and `bench-psram.yaml` build the board with and without PSRAM; `round.sh CFG TAG ROUND`
  flashes one build over serial, checks the BUILDTAG line, runs the set (TCP receive 20 s twice, send 20 s, 10 x
  512 KB, 10 UDP bursts of 300) and appends to `results.txt`. Alternate dev and PR rounds (four per build at least):
  run to run spread on a 3 to 6 Mbit/s link is wider than most differences. `summ.py DIR` prints medians, ranges,
  esp_hosted warnings, allocation failures and the internal heap low point per build.
- With guaranteed PSRAM, TCP receive with the 512 KB lwIP window drains the P4's internal heap to about 13 KB on dev
  too (`mempool OOM` or `bus_rx: payload copy alloc failed`); some receive runs then time out. Leave those out of the
  medians and say so.

## Updating the C6 co-processor firmware from the P4 (#23122)

The C6 on early boards reports `coprocessor=0.0.0` ("major version mismatch", streaming mode, no SDIO aggregation).
`scripts/tcpbench/c6-update.yaml` updates it over SDIO with ESPHome's own `update: platform: esp32_hosted`, `type:
embedded` and the ESPHome-published image (manifest https://esphome.github.io/esp-hosted-firmware/manifest/esp32c6.json).
It needs `flash_size: 16MB` (the 1.47 MB image is embedded) and `watchdog_timeout: 60s` (the old firmware never
answers the version RPC, each call blocks 5 s), and starts the update from `on_update_available` after 20 s.

- The write takes about 11 s. The old firmware has no activate RPC, so `Failed to activate OTA` is logged, but the
  reboot runs the new image: `host=3.1.0 coprocessor=3.1.0 (match)`, `RPC version negotiated: V2`, `SDIO SW_AGGR
  negotiated`.
- Going back to a build without the embedded image keeps the C6 firmware. SDIO aggregation takes about 44 KB more
  internal heap at boot (two 15,872 B RX staging slots), and the new firmware alone raised dev throughput, so update
  before comparing builds.
