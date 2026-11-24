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

## ESP8266 flash preferences: Wi-Fi scan heap collapse and interrupt load (#21046, #23221)

Files in `scripts/esp8266_flash_prefs/`, a NodeMCU (ESP8266, Arduino).

- `scan-repro.yaml` reproduces [#21046](https://github.com/esphome/esphome/issues/21046): `restore_from_flash: true`,
  `flash_write_interval: 1s`, a global bumped every second and a raw `wifi_station_scan` every 3 s, gated on
  `is_connected()` (ungated it pre-empts ESPHome's own scan and the board never joins). The crash needs network load
  too: `run.sh TAG SECONDS HOST` (env `PORT`) runs the serial capture, `ping -i 0.05` and a held API connection
  together. Without the ping nothing crashed in 1 h; with it dev reset 14 times in 1 h. The heap stays flat at about
  46 KB, then collapses to under 5 KB within 60 to 90 s before a StoreProhibit in `ieee80211_add_xrates`, a
  `memset(NULL)` from `esf_buf_alloc` or a fault in `ieee80211_deliver_data`. A control build with
  `-s restore_from_flash false` (or a build without the `InterruptLock` in `sync()`) shows 0 resets and a flat heap.
  `crashes.py --trend CAPTURE` prints the heap per boot, `crashes.py CAPTURE ELF` decodes each exception PC with the
  last heap line before it (it finds `xtensa-lx106-elf-addr2line` under `~/.platformio`, or set `ADDR2LINE`).
- `isr-load.yaml` checks interrupts during the flash sync for [#23221](https://github.com/esphome/esphome/pull/23221):
  jumpers GPIO12 to GPIO14 (2 kHz `esp8266_pwm` into `pulse_counter`, which counts exactly 2000/s when nothing masks
  interrupts) and GPIO13 to GPIO4 (5 Hz into an interrupt driven binary sensor), a 5 s interval that times one
  `sync()`. `israna.py` prints the pulse rate per second window, edges per second, sync times and crashes. With
  interrupts locked during the erase (dev) the PWM and the counter lose about 3 % (about 31 ms per second); a whole
  `sync()` takes about 35 ms on both builds.
- `pers.py PORT HOST OUT KINDS N` resets the `isr-load` board N times per kind (`api` restart button, `pin` reset,
  `pinrand`, `pinsave` 0 to 30 ms after a "Saving" line, `hwdt` and `swdt` hang buttons) and compares the restored
  globals with the last tick printed before the reset. Disable `safe_mode` (the config does), or 10 quick resets enter
  safe mode. Opening the CP2102 port resets the board, so the driver waits 8 s before the first trial.
- Each restoring global polls at 1 s and the sync runs at 1 s, so the restored counter is 0 to 2 behind the last value
  printed, and two globals written together may come back 1 to 2 apart. A reset inside the erase to write window loses
  the whole preference sector, on dev too (about 3 of 45 random pin resets).
- A background `ping` ignores the SIGINT from `timeout` and loses its summary; give it a deadline instead (`-t SECONDS`
  on macOS, `-w SECONDS` on Linux).
- Results: https://github.com/esphome/esphome/pull/23221#issuecomment-7119953311
