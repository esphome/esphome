# libretiny

## LibreTiny rtc preference emulation (#21007)

The PR keeps `storage: rtc` preferences in FlashDB and deletes them on the first load or save after a boot
whose `lt_get_reboot_reason()` is not software, watchdog, crash or sleep. Only the families listed in
`_libretiny_rtc_supported()` accept the config keys; on others force the backend in with
`platformio_options: build_flags: ["-DUSE_LIBRETINY_RTC_PREFERENCES"]` after confirming the rejection.

- Harness: `scripts/libretiny_prefs/ltprefs.hpp` plus `templates/bk72xx-rtc-prefs-dut.yaml`. `ltprefs_boot()`
  loads one rtc and one flash preference, a second pair that shares one key as rtc and as flash, and a third
  rtc entry (so a hard reset drops three entries in one pass), logs the reason and both results, saves `100 + step`, syncs; after
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

## BK72xx crash report on the next boot (#21012)

Harness: `scripts/libretiny_crash/bkprobe.hpp` plus `templates/bk72xx-crash-probe-dut.yaml`. A flash
preference holds the step; `on_boot` (priority -100) logs the reset reason, the SDK start type, two canaries
and hex dumps of the low SRAM, a set of marker words and the record area after `.bss`, waits 30 s on step 0
(so a capture can be opened after the flash) or 5 s, saves `step + 1`, syncs, then acts: 0 `__builtin_trap()`,
1 call into unmapped memory, 2 unmapped and unaligned reads and a store (no fault on the ARM968), 3
`lt_reboot()`. A `globals: restore_value` step counter does not work here: it is only saved from the global's
loop, which a crash 50 ms later never reaches; save an `ESPPreferenceObject` and `global_preferences->sync()`
first. Bump `run:` for fresh keys. Keep the `wifi:` block: the bug the fix addresses only shows with Wi-Fi up.

Flow: `flash_bk72xx.py` (retry up to three times), one `serial_capture.py ... --reset` capture of 150 to 230 s,
then `grep -a "libretiny.crash\|probe"`. Decode with
`arm-none-eabi-addr2line -pfiaC -e .pioenvs/<name>/raw_firmware.elf <addr...>` (the ELF is `raw_firmware.elf`
on LibreTiny), or run `scripts/libretiny_crash/decode_capture.py <yaml> <capture>` from the config directory
to feed the capture through the CLI's `LogLineProcessor`.

Expected: `*** CRASH DETECTED ON PREVIOUS BOOT ***`, `Reason: Undefined instruction`, PC on the `udf`,
LR = PC + 1, BT0..BT7 walking the lambda, `StatelessLambdaAction`, `Action::play_complex`, `DelayAction`,
`Scheduler::call`, `Application::loop`. The wild call reports where the core ended up (it executes zeros as
Thumb NOPs until it wraps into real code), not the called address. Without the fix a dev build with Wi-Fi
connected never comes back from the crash; see the BK72xx section in `boards.md`.

OTA-only board: copy the device's own config, add the probe include and the step machine (step 0 crashes
45 s after boot, later steps idle), keep its wifi, api, ota and safe_mode blocks and use the secrets file
next to that config. A LibreTiny OTA is a 1.5 MB UF2 and takes minutes on a slow link, so do not wrap the
upload in a short timeout. Read the replayed report with `esphome logs ... --device <name>.local` (an API
client gets it on subscribe) or on the board's serial log port if it has one.
