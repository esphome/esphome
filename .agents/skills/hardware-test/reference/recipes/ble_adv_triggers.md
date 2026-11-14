# BLE service and manufacturer data advertise triggers (#21822, #21823, #21827)

`scripts/ble_adv_triggers` checks `on_ble_service_data_advertise` and `on_ble_manufacturer_data_advertise` over the
air on the ESP32 and BK72xx trackers.

- Advertiser: `advtrig.yaml` on any second ESP32 with BLE. Every 250 ms it switches to the next of six raw
  non-connectable advertisements (`adv_frames.hpp`, random static addresses `C4:C1:38:00:00:21` to `26`):

  | Address | Contents |
  |---|---|
  | `...:21` | service data 181C `AA01` and manufacturer data 02E5 `BB01` |
  | `...:22` | service data 32-bit 12345678 `AA02` |
  | `...:23` | service data 128-bit 12345678-9ABC-DEF0-1122-334455667788 (wire order) `AA03` |
  | `...:24` | manufacturer data 181C `BB04` (the service UUID of `...:21`, other data kind) |
  | `...:25` | two service data 181C entries, `AA05` then `AA06` (only the first may fire) |
  | `...:26` | the 128-bit UUID in printed byte order, `AA07` (must not match) |

- Receivers: `rx-esp32.yaml` (classic ESP32, passive scan) and `rx-bk72xx.yaml` include the same eleven triggers
  (`triggers.yaml`). `rx.hpp` counts each trigger's hits by payload (`AAnn`, `BBnn`, anything else as `other`) and
  logs the table every 20 s, plus all adverts and those from the advertiser.
- `run.sh esp32|bk72xx LABEL` builds, flashes, captures 150 s and runs `check.py`, which prints PASS or FAIL. Run it
  once per build (dev merge base and each PR head). About 550 hits per trigger on ESP32 and 170 on a BK7238 in that
  time.
- `mac_address: 00:00:00:00:00:00` meant no filter on the ESP32 tracker before #21823 and filters on that address
  after it (BK72xx always filtered). Pass `ZERO_MAC=any` for an ESP32 build from before #21823.
- BK7238: with no `wifi:` in the config the tracker logs `Scan started` but receives nothing at all, on dev too. The
  BK config therefore joins Wi-Fi.
- Results: https://github.com/esphome/esphome/pull/21827#issuecomment-6489703755
