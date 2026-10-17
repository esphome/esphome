# libretiny

## LibreTiny rtc preference emulation (#21007)

The PR keeps `storage: rtc` preferences in FlashDB and deletes them on the first load or save after a boot
whose `lt_get_reboot_reason()` is not software, watchdog, crash or sleep. Only the families listed in
`_libretiny_rtc_supported()` accept the config keys; on others force the backend in with
`platformio_options: build_flags: ["-DUSE_LIBRETINY_RTC_PREFERENCES"]` after confirming the rejection.

- Harness: `scripts/libretiny_prefs/ltprefs.hpp` plus `templates/bk72xx-rtc-prefs-dut.yaml`. `ltprefs_boot()`
  loads one rtc and one flash preference, logs the reason and both results, saves `100 + step`, syncs; after
  a templated delay `ltprefs_exit()` bumps the flash-backed step and runs the step's action (`lt_reboot()`,
  `__builtin_trap()`, `lt_reboot_wdt()` or idle). The last 8 boot reports are kept in a flash preference and
  repeated in the 10 s alive line, so a boot missed by the capture (a power cut, an OTA-only board) is still
  readable. Bump `run:` for fresh keys.
- A store to an unmapped address does not fault on the ARM968, the board carries on: use `__builtin_trap()`
  for the crash row. The SDK crash handler reboots about 10 s later. Works on BK7238 and BK7231N.
- Hard resets: `serial_capture.py ... --reset` pulses RTS on CEN (reported as Power-On). A real power cut
  needs a hand or a smart plug; the history in the alive line covers the boot the host cannot see. On the
  Beken SDK a chip-enable reset and a power cut both report Power-On, there is no separate reset-pin source.
- Step 0 and step 8 wait 30 s before acting so the host can open the port (and pulse a reset) after a flash
  without losing the first boot; the flash tool's own reset starts the sequence immediately otherwise.
- OTA-only board: keep the device's wifi, api and ota blocks verbatim, keep safe_mode, read logs with
  `esphome logs <yaml> --device <name>.local` (the API attaches late, hence the history), and restore the
  device's own config afterwards.
- Plain-build comparison: `raw_firmware.bin` differs by the timestamp and by `__LINE__` in log calls of
  functions below the added code; map offsets through `objdump -h` load addresses and `addr2line` before
  calling a byte difference a code change.
