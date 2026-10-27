# Injecting BLE GAP and GATTC events (#21698)

Some BLE paths (repeated connection parameter update failures, a disconnect in the middle of them) are hard to get
from a real peer on demand. `scripts/ble_event_inject` feeds made up events straight into the ESP32 BLE handlers.

- `ESP32BLE::gap_event_handler` and `gattc_event_handler` are protected statics and the class is `final`.
  `cp_inject.hpp` reaches them through explicit template instantiation, where access checks do not apply. Its
  `cpinject::run()` sends six labelled steps (`STEP1` to `STEP6`): four peers failing five times each, a success
  followed by more failures, and disconnects between failures.
- `cp-inject.yaml` is a package for a copy of a proxy config: it adds the include, a VERBOSE logger with the busy
  tags back at DEBUG, and an `Inject conn param events` button. Press it with `scripts/api_button.py` while a
  log capture (`esphome logs ... --device <ip>` under `timeout`) runs, then count the warning and verbose lines per
  step on the PR and on dev.
- Fake disconnects use `gattc_if` 0xFE: every client and proxy connection filters on its own `gattc_if_`, so the
  queued event reaches only the shared handler. Do not use 0xFF (`ESP_GATT_IF_NONE`), which every client accepts.
- The fake peers (`C0:FF:EE:00:00:0n`) are never connected, so a live proxy keeps serving its real connections;
  put it back on its own config afterwards.
