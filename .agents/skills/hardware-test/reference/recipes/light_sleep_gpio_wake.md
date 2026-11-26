# GPIO wakeup from auto light sleep (#21074)

`scripts/light_sleep_wake` needs no wiring. LEDC on `LEDC_USE_RC_FAST_CLK` with
`sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE` keeps toggling GPIO10 at 2 Hz through light sleep, and an
`ESP32InternalGPIOPin` interrupt (`ANY_EDGE`) on the same pad timestamps every edge in an IRAM handler.
`wakeprobe::report()` logs every 30 s: edges seen and expected, the shortest and longest gap, how many edges came
more than 20 ms off the half period, and the SLEEP share from `esp_pm_dump_locks()` (needs `profiling: true`).

- Native ESP-IDF builds need `include_builtin_idf_components: [esp_driver_ledc]`.
- After `ledc_channel_config` call `gpio_input_enable(pin)`, never `gpio_set_direction(INPUT_OUTPUT)`: that routes the
  pad back to a plain GPIO output, the wave stops and the pad reads a constant 1.
- The C3 shares one LEDC clock across timers, so a board whose own config drives LEDs through `ledc` needs a test
  config without them. Logging over the API works with `logger: baud_rate: 0`.
- Read the reports over the API with `esphome logs ... --device <ip>` under `timeout`. Run PR, dev, then the PR
  again. On a C3 bulb dev caught every edge but handled 8 of 845 more than 20 ms late
  (up to 45 ms), the PR none; both slept 57 to 58% of the time.

## Peripheral power-down needs an external signal (ESP32-P4)

LEDC in `LEDC_SLEEP_MODE_KEEP_ALIVE` keeps the TOP power domain on, so the self-generated wave above never tests
`power_down_peripherals`. With that option the PR's ext1 path also hands an LP pin to the LP IO block, and an LEDC
wave on the same pad no longer reaches the input (the pad reads 0).

- `wake-stim.yaml` puts a 2 Hz square wave on GPIO25 of a classic ESP32 (LEDC on REF_TICK). Wire it to GPIO4 of the
  board under test; boards on the same USB host share ground.
- `wake_detail.hpp` with `wake-detail-esp32p4.yaml` (substitution `power_down`): GPIO4 is the any-edge interrupt pin
  and GPIO22, jumpered to it, is read as the true level. The first 40 s hold a `NO_LIGHT_SLEEP` lock, then sleep is
  allowed. Each report counts interrupt calls, real edges (the GPIO22 level changed) and calls with no change, per
  phase, and two windows (from 9.5 s and from 69.5 s) log every call with its gap and both levels. A light-sleep
  exit callback counts wakes when `CONFIG_PM_LIGHT_SLEEP_CALLBACKS` is set.
- Waveshare ESP32-P4-WIFI6 (pre-rev3 silicon, CH343 console): `engineering_sample: true`, `flash_size: 32MB` and
  `logger: hardware_uart: UART0`, or the app logs nothing on that port. No Wi-Fi needed. GPIO4 is the left header,
  12th pin from the USB end; GPIO22 the right header, 9th.
- Result on #21074 at 2ef417bd81 with peripherals powered down: the PR caught all 644 real edges (dev 66) but added
  4642 calls while the line was high, one per sleep cycle (about 16 ms), with the level read correctly. Peripherals
  on: both builds caught every edge. https://github.com/esphome/esphome/pull/21074#issuecomment-6503939850

## Power lock check

`power_management: esp32_power_management` with `profiling: true` and an `interval` lambda that calls
`esp_pm_dump_locks(stdout);` prints the lock and mode tables on the USB console (not through the ESPHome logger).
Compare lock lists between builds: `usb_serial_jtag` holds `NO_LIGHT_SLEEP` by itself. For a soak that the USB console
must not keep awake, set `logger: hardware_uart: UART0`, dump the locks into `open_memstream()` and log the SLEEP,
CPU_MAX and light sleep count lines, read over the API.

## ESP32-S3 on native USB: no sleep, or no port (#21074)

- `esp32_power_management` sets `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y` (a user `sdkconfig_options: n` loses), so
  while the USB Serial/JTAG sees the host, the `usb_serial_jtag` NO_LIGHT_SLEEP lock is held: 240 s with sleep allowed
  gave 4 light sleeps and SLEEP 0 %. Any S3, C3 or C6 on USB Serial/JTAG measures nothing unless that is turned off.
- With it turned off (the line edited in both trees) the S3 sleeps and the port vanishes, and it does not come back
  when the firmware takes a permanent NO_LIGHT_SLEEP lock afterwards: a macOS host disables the root port and the
  device never re-enumerates. A port on the computer itself cannot be power cycled with uhubctl; only a replug
  recovers it.
- Also tried: 20 s awake, 20 s sleep allowed, totals saved in RTC_NOINIT memory, then the D+/D- pull override
  (pull-ups off, pull-downs on) for 300 ms through the `usb_serial_jtag_ll_phy_*` / `usb_fsls_phy_ll` calls and
  `esp_restart()`. The port stayed listed through the window and vanished at the reconnect, never to return.
- So for a light sleep measurement on these chips: put the board on a uhubctl-capable hub, or log over UART0 through
  a USB-UART adapter (`logger: hardware_uart: UART0`) and read the totals over the API (Power lock check above).
  Prove any reconnect trick on a 20 s window before the real run.
- Recovery after a replug: `esptool --after no_reset chip-id` within the first 20 s (while the lock is still held)
  parks the chip in the ROM loader; then flash.
