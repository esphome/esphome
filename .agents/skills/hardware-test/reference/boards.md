# Board facts

Ports are not listed on purpose: they follow the USB hub port and change on every replug. Run
`scripts/ports.py`.

## ESP8266 (D1 mini, NodeMCU, CH340 or CP2102)

- Opening the port usually resets the board through DTR, but not always; `serial_capture.py --reset`
  pulses RTS to capture from boot. 74880 baud shows the ROM boot messages (`rst cause`, `boot mode`).
- `esphome logs` on some CH340s emits thousands of duplicated garbage lines right after the RTS reset;
  a plain pyserial capture at 115200 is reliable.
- A failed esptool write can wedge the adapter (`termios` error 22, or an open that hangs) until replug.
- The preference sector survives a reflash: a provisioned API key or saved Wi-Fi credentials stay
  active. `esptool erase_flash` gives a clean device.
- `Panic core_esp8266_main.cpp:191 __yield` with `ctx: sys` means a yield from lwIP context (a web
  handler, an lwIP callback). Decode stacks with
  `xtensa-lx106-elf-addr2line -e <build>/.pioenvs/<name>/firmware.elf -f -C -i <addr...>` from the
  toolchain under `~/.platformio/packages/toolchain-xtensa/bin/` (PlatformIO builds) or the ESPHome
  cache directory (native toolchain builds).
- Constant data lives in RAM unless marked `PROGMEM`; with 80 KB of RAM a few hundred bytes are worth a
  row in the PR.
- AsyncMqttClient drops inbound topics over 128 characters; topics of 255 or more wrap a counter and
  stall the link until the broker drops TCP.

## ESP32 classic devkit

- Takes ESP-IDF and Arduino. Default Wi-Fi light sleep delays API driven actions by up to 6 s; judge
  timings from device logs, not from the host. `power_save_mode: none` for OTA overlap tests (an IDF web
  upload of 739 KB took 107 s with power save and 45 s without).
- Receives only a few percent of IPv6 multicast on some access points with power save on; a raw lwIP
  socket loses the same, so that is the AP or driver, not a PR.
- Decode with `xtensa-esp32-elf-addr2line` from the IDF tools directory against
  `<build>/.pioenvs/<name>/firmware.elf` (PlatformIO) or `<build>/build/<name>.elf` (IDF native).
- Clean reboot between runs with the capture stopped: `esptool --after hard_reset chip_id`.
- Trigger a forced crash only after the app is marked valid (45 s after boot is plenty), never on an
  image still pending OTA rollback.
- Signed OTA testing: build with `esp32.framework.advanced.signed_ota_verification.verification_keys`
  under a different name, sign with `python -m espsecure sign_data --version 2 --keyfile
  tests/components/esp32/dummy_signing_key.pem`, upload with `esphome upload --file`. The verifier
  refuses unsigned images afterwards, so restore with a signed plain build.

## ESP32-S3, C3, C6, H2 with native USB

- `/dev/cu.usbmodem*`, esptool works, `logger: hardware_uart: USB_SERIAL_JTAG`.
- The USB console can die at the moment the app takes over the console during a reboot (log stops after
  `Disabling RNG early entropy source`, then 0 bytes, esptool reports `No serial data received`). Only a
  USB replug fixes it. Fallback: `esphome logs --device <name>.local` (API log stream, which drops lines
  while Wi-Fi is off).
- The ESP32-P4 before silicon revision 3 is bricked by rev3-only firmware (boot loop at bootloader entry).

## RP2040 / RP2350 (Pico W, Pico 2 W)

- The CDC port re-enumerates after every reboot and after `esphome upload` (1200 baud touch). Re-detect
  with `ports.py --wait 2e8a` before capturing; `serial_capture.py` reopens on its own if the port
  vanishes mid-capture.
- First flash ever: BOOTSEL, then `esphome upload --device /Volumes/RPI-RP2` (or `RP2350`).
- arduino-pico creates the lwIP netif in `WiFi.begin()`, so any IGMP join during component setup fails;
  joins must wait for `network::is_connected()`.
- Parallel Pico builds race on package extraction after a PlatformIO package wipe; one at a time.
- An RTS pulse from pyserial does not reset the board; reboot with `App.safe_reboot()` or the touch.

## BW15 kit (RTL8720CF, LibreTiny rtl87xx)

- The kit's CH340 wires RTS to CEN and DTR to PA00 (boot strap). A plain pyserial open asserts both and
  holds the chip in reset, which is why `esphome upload`, plain `ltchiptool` and `esphome logs` all fail
  ("Timeout while linking", or silence). `scripts/flash_rtl87xx_kit.py` sets DTR high and RTS released
  on every open, pulses RTS once so the ROM answers `ping`, and after the command drops DTR and pulses
  RTS to boot the app. Run it with the PlatformIO LibreTiny penv Python.
- `flash write -d <port> <uf2>` takes no family argument; `flash read -d <port> -s 0 -l <len>
  realtek-ambz2 <out>` does.
- Capture with `serial_capture.py --released`. The ESPHome log is on the flashing UART already.
- The macOS CH340 driver only pushes DTR/RTS to the wire once a session has driven them, so the first
  plain open after a replug streams logs and the second is silent; always open with the lines released.
- Driver quirks seen on `dev`: `WiFi.begin` can refuse the real network on the first try after bogus
  credentials (it connects after two cooldowns); right after connecting while the AP is still up the
  driver can bus-fault in `rtw_is_adapter_up` (`R0 = 0xdeadbeef`) and reboot; the AP restarts on every
  STA retry, so stations cannot stay joined; `WiFi.begin` fails synchronously for an absent network and
  lands in a cooldown at once; connecting blocks the loop 2.5 to 4.1 s.
- Decode faults with `arm-none-eabi-addr2line -e <build>/.pioenvs/<name>/raw_firmware.elf -f -C -i <PC>`
  from `~/.platformio/packages/toolchain-gccarmnoneeabi/bin/` (`raw_firmware.elf`, not `firmware.elf`).
  Fault dumps print PC, LR and R0 to R3.
- The AmebaZ2 SDK sets `LWIP_TCPIP_CORE_LOCKING 0`.

## BK7238, WB3S, CB3S (BK7231N) (LibreTiny bk72xx, CH340, RTS on CEN)

- Stock ltchiptool closes and reopens the port after its reset, and pyserial raises both lines on open,
  holding CEN low through the BootROM window. `scripts/flash_bk72xx.py` opens with both released and
  pulses the reset right before the link checks. A write takes about 75 s. Only a CEN or power reset
  reaches the downloader; no software reboot does.
- ESPHome logs on UART2 by default; `logger: hardware_uart: UART1` puts them on the flashing adapter.
- Opening the port may reset the chip (bootloader banner on open) or may not, depending on the adapter and
  what pyserial does with DTR and RTS; `serial_capture.py --reset` pulses RTS on CEN and always does, and
  `lt_get_reboot_reason()` reports that as Power-On. Check for the banner at the start of a capture before
  assuming either.
- `flash_bk72xx.py` misses the link now and then (`Timed out attempting to link with chip`); retry up to 3
  times before suspecting the board.
- A store to an unmapped address does not fault on the ARM968; use `__builtin_trap()` to force a crash. The
  SDK crash handler prints the registers and reboots about 10 s later, reported as Crash.
- Callbacks run on the `async_tcp` task and log lines can lose a byte. Wi-Fi connect takes tens of ms;
  radio enable 160 to 278 ms, disable about 200 ms.

## BW16 kit (RTL8720DN, AmebaD)

- The kit's own USB-C (CH340) is on LOG_UART and flashes fine. Its two-transistor circuit resets while
  RTS alone is asserted and straps BURN while DTR alone is; both asserted does nothing. ROM entry is
  esptool's classic sequence (`dtr:false,rts:true` 100 ms, `dtr:true,rts:false` 400 ms, both false); the
  first pulse after a replug can be lost, retry up to three times.
- Any reopen resets the chip, 460800 is flaky, 115200 is solid: one open per session, in-place baud
  changes only. The ROM and the loader NAK continuously while idle, so ACK scans must drain first.
- The AmebaD core has no mDNS responder (`mdns: disabled: true`).

## LN882H

- ltchiptool forces 1 Mbaud for writes; adapters that cannot hold it fail with "Timeout while linking"
  or die mid-write. Patch `LN882H_YM_BAUDRATE` in the penv's `ltchiptool/soc/ln882h/util/ln882htool.py`
  to 115200 (lost on reprovision). Weak 3.3 V from the adapter: feed the module's 5 V pad instead. Short
  BOOT to GND while the tool connects. OTA afterwards.
- `USE_WIFI_AP_EXCLUSIVE` (one radio mode at a time) exists only on LN882x; compile-check changes there.

## nRF52840

- Nordic PCA10059 dongle: the factory Open DFU bootloader enumerates as `1915:521F` on its own port
  name; the app as `2FE3:0100` with two CDC ports, ESPHome log on the second. Only Nordic
  `nrfutil nrf5sdk-tools` works (`pkg generate --hw-version 52 --sd-req 0x00 --application-version 1
  --application zephyr.hex app.zip`, then `dfu usb-serial -pkg app.zip -p <port>`); ESPHome downloads
  it on the first build. GPREGRET does not enter the bootloader; a pin reset through P0.19 does, which
  `dfu: true` wires up on this board. A pulsing red LED means bootloader; the side RESET button
  re-enters it.
- Makerdiary MDK USB dongle: UF2 bootloader without SoftDevice, app at 0x1000, `UF2BOOT` volume,
  adafruit-nrfutil, GPREGRET 0x57 works. RGB LED active low (red P0.23, green P0.22, blue P0.24),
  button P0.18.
- Adafruit ItsyBitsy nRF52840 Express (UF2 bootloader with S140, bootloader port `239a:0051` plus an
  `ITSY840BOOT` volume): the board DTS already has `cdc_acm_uart0`, so the logger port is the first CDC
  port and a `usb_cdc_acm` interface 1 the second. A `dfu: true` build enters the bootloader on the
  1200 bps touch (an app without it needs a double tap on RESET); `adafruit-nrfutil` over the bootloader
  serial port can fail, copying `.pioenvs/<name>/zephyr/zephyr.uf2` to the volume works (the copy reports
  an I/O error as the board reboots, which is normal). Reboot a running `dfu: true` app with a 2001 bps touch.
- The PCA10059 board DTS already defines a `cdc_acm_uart` node, so every ESPHome CDC port is one USB port
  later than its number (log on `cdc_acm_uart0` is the second port). A third CDC instance needs
  `CONFIG_USB_MAX_NUM_TRANSFERS` above 4 or it never moves data (see `recipes/usb_cdc_acm.md`).
- USB CDC boot output: the Zephyr legacy stack's `cdc_acm` TX ring is 1 KB and `uart_poll_out` drops when it
  is full, so anything printed in one burst beyond about 1 KB (a coredump, a long table) is lost even with
  the port open, and the logger warns `logger took a long time`. Print one line per `loop()` instead.
- Zephyr settings survive a reflash: key a `restore_value` global by a `run` substitution when a test needs
  a never-saved value.

## Waveshare ESP32-S3-ETH with the OV5640 camera module

- Native USB console (`303a:1001`), ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM, W5500 on SPI (MOSI 11,
  MISO 12, SCLK 13, CS 14, INT 10, RST 9), TF card on 4..7, WS2812 on 21.
- Camera header (vendor schematic): XCLK 3, SIOD 48, SIOC 47, VSYNC 1, HREF 2, PCLK 39, D0..D7 = 41, 45,
  46, 42, 40, 38, 15, 18. PWDN is tied to GND and RESET is an RC to 3.3 V on the board, so no reset or
  power-down GPIO exists.
- GPIO8 switches the camera 3.3 V rail through a P-channel MOSFET: LOW = camera powered, gate pulled up so
  the camera is off by default. `power_down_pin: GPIO8` only pulls it low 10 ms before the probe, which is
  not enough; use `switch: gpio GPIO8 restore_mode: ALWAYS_OFF setup_priority: 800` plus `on_boot:
  priority: 700` with `delay(1200)` and `esp32_camera: setup_priority: 200`
  (`templates/rtsp-waveshare-s3-eth-dut.yaml`). `esphome config` rejects a config that uses GPIO8 for both the switch
  and `power_down_pin` (pin reuse), so it is one or the other.
- The sensor at SCCB 0x3C answers only with XCLK running, so the ESPHome i2c scan at boot (before XCLK)
  reporting no devices on 48/47 is expected. If the SCCB ID register (0x3100 = 0x78) reads fine but every
  0x30xx register reads 0x00 (`Detected camera not supported`, 0x106), reseat the ribbon before blaming
  software; `templates/waveshare-s3-eth-camdiag.yaml` runs that probe with LEDC XCLK and raw SCCB reads.

## ESP Thread Border Router board v1.2 (S3 host + H2 RCP)

- S3 host on USB JTAG serial, H2 RCP on its own USB. Spinel UART1 from the host: `rx_pin: GPIO17`,
  `tx_pin: GPIO18`, RCP reset `GPIO7` active low, 460800. The config needs `network: enable_ipv6: true`.
- The shipped H2 firmware fails `CheckRadioCapabilities` with IDF 5.5; build `examples/openthread/ot_rcp`
  for esp32h2 with ESPHome's own IDF (`IDF_PATH` at the cached framework, its penv and tools,
  `IDF_PYTHON_CHECK_CONSTRAINTS=no`) and flash with esptool at 0x0, 0x8000 and 0x10000.
- A crash-looping S3 pulses the H2 reset every boot, so the H2 cannot be flashed and the S3 will not
  enter its bootloader over USB. Hold S3 BOOT, tap S3 RST, flash the H2, release, then replug both.
- `dns-sd -L <name> _meshcop._udp local.` shows the `omr` prefix; it disappears when routing is dead.
  Keep one capture per chip; never reset the H2 over USB mid-test unless that is the test (the host
  logs `Unexpected RCP reset: RESET_OTHER` and recovers).
