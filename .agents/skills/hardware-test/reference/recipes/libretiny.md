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

## RTL8720C crash report on the next boot (#21648)

`templates/rtl8720c-crash-buttons-dut.yaml` has one API button per case: `trap` (`__builtin_trap()`, usage fault),
`bus fault` (store to 0x70000000, imprecise, CFSR 0x400), `wild call` (call to 0xFFFFFFF1, MemManage fault with PC
0xFFFFFFF0 and LR in the caller), `isr trap` (a trap from a spare interrupt pended by software, hard fault), `hang`
(interrupts off, the watchdog resets it and nothing must be reported) and `restart`. Every boot logs
`boot reason N, crash record yes|no|n/a`; `n/a` means the handler is not built, as on a dev control build.

Flow per build: flash with `flash_rtl87xx_kit.py`, start one `serial_capture.py PORT 600 OUT --released --ms`, then
`scripts/libretiny_crash/rtl_drive.py HOST OUTDIR TREE YAML trap restart "bus fault" "wild call" "isr trap" hang restart`.
The `restart` after the first crash checks the report shows only once. Time each reset from the serial capture
(the ROM fault dump to the next `ROM Version` line), not from the API port: the board is back before an API client
notices it went away. Compare the reported PC with the `PC  =` line of the ROM dump, and check the decoded lines in the
`caseN-*.txt` files. Pass the UF2 by name (`firmware.uf2`): the build directory holds a second identically sized copy.

Regression rerun of the same flow (#21728): `scripts/libretiny_crash/rtl_rom_vs_report.py CAPTURE` lists, for each ROM
fault dump in a `--ms` capture, the ROM's CFSR and PC, the ms to the next `ROM Version` banner and the Reason, CFSR, PC
and SP the next boot reported, so two builds compare line by line. The ROM lines start with a bare CR; split the
capture on newlines only. The imprecise bus fault's PC moves between builds (it is wherever the core was when the
fault was raised); compare it with that run's ROM dump, not with another build. SP stays the same per fault (8 word
frame, no FPU). On the BK7238 time the reboot from the button press to the next `BK7238_1.0.14` banner (0.6 to 0.7 s).

## BK7238 BLE and flash sector safety (#21735)

The Beken BLE bond store reads its partition at boot and erases that sector when the CRC does not match; before
LibreTiny 1.14 the partition was unmapped and pointed at sector 0, erasing the bootloader (#18646). To prove a change
like this safe on a board that cannot be recovered without a dump:

1. Take a full backup first: `scripts/bk72xx_flash/readback.sh full-before` (ltchiptool family `beken-7238`, about
   190 s for 2 MB). Keep it until the board is back on a known build.
2. Serial flash a build without the feature, then deliver the feature build by OTA (the path users take). Use
   `bk72xx: framework: sdk_silent: none` with `logger: hardware_uart: UART1`, so the SDK's own lines (bootloader
   banner, `CRC fail, remove bonded info`) show up in the capture.
3. Soak with Wi-Fi up, OTA again from the new build, pulse CEN a few times (`serial_capture.py --reset`), and check
   each boot comes back through the bootloader.
4. Read the whole chip again and run `scripts/bk72xx_flash/partdiff.py full-before.bin full-after.bin
   <libretiny>/boards/_base/beken-72xx.json`: bootloader, calibration and userdata must be unchanged; app, download
   and one KVS sector change normally.

Check what already sits in the target partition before the first boot: on an XH-WB3S that shipped with Tuya firmware,
0x1E1000 (the `ble_bonding` partition of the generic Beken layout) held the Tuya `diff2ya` record (magic `ba dc cd
ab`), which the bond store erased on its first boot. Nothing depended on it there, but say so in the results.
