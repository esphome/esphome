# globals

## Restoring globals with storage: rtc and flash (#13954)

`templates/globals-storage-dut.yaml` is the shared package: five restoring globals (rtc int, flash int, default int,
rtc `std::string`, rtc `uint8_t[2]`), bumped and logged with the reset reason on every boot, saved and synced after
2 s, odd boots `App.safe_reboot()`. Platform files add the board (`packages: dut: !include globals-storage-dut.yaml`), the `debug` reset-reason text sensor and, on the
ESP32, `deep_sleep: run_duration: 12s, sleep_duration: 8s` (even boots sleep). No network needed: flash by USB,
`serial_capture.py PORT 110 OUT --reset` gives the reset pulse plus a run of soft restarts and sleeps; the ESP32's
EN pulse is a `POWERON_RESET` that empties RTC memory, the D1 mini's RST pulse (`rst cause:2`) keeps it, so the
ESP8266 needs a real power cut by hand; arm long `--released` captures on both ports first (the capture
reopens the port when it comes back, the very first boot lines are still lost, read the next boot's restored
values). Unchanged-build check: the config with every `storage:` removed, built on the PR and on the dev merge
commit under the PR's own commits (not the merge base, which is older dev), `cmp -l` plus an `nm -S` symbol diff;
only the template names differ. Results: https://github.com/esphome/esphome/pull/13954#issuecomment-6254794712
