# Board facts

Ports are not listed on purpose: they follow the USB hub port and change on every replug. Run
`scripts/ports.py`.

## ESP8266 (D1 mini, NodeMCU, CH340 or CP2102)

- Opening the port usually resets the board through DTR, but not always; `serial_capture.py --reset`
  pulses RTS to capture from boot. 74880 baud shows the ROM boot messages (`rst cause`, `boot mode`).
- UART0 can be looped back onto itself (`USC0(0) |= (1 << UCLBE)` in an `on_boot` lambda) so a UART
  component is testable without its device: frames written with `uart.write` arrive on RX. Log over the
  API with `logger: baud_rate: 0`, since the serial port carries the component's bytes.
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
- `Arduino.h` already declares `_heap_start`; a lambda can use `&_heap_start` directly, and a second `extern` fails.
- `GPI` bits 16 to 31 are strapping bits (mask with 0xFFFF). GPIO16 has no interrupt; read `GP16O` for an output.

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
  refuses unsigned images afterwards, so restore with a signed plain build. The full accept and reject case set
  (several keys, CRC-only edits) is in `recipes/ota.md`.
- An ESP32 whose bootloader has no app rollback is protected only by `safe_mode` after a bad OTA. Before sending a
  change to such a device that nobody can reach by hand, boot the same code on a devkit first and keep a fallback
  image of the merge base built.

## ESP32-S3, C3, C6, H2 with native USB

- `/dev/cu.usbmodem*`, esptool works, `logger: hardware_uart: USB_SERIAL_JTAG`.
- The USB console can die at the moment the app takes over the console during a reboot (log stops after
  `Disabling RNG early entropy source`, then 0 bytes, esptool reports `No serial data received`). Only a
  USB replug fixes it. Fallback: `esphome logs --device <name>.local` (API log stream, which drops lines
  while Wi-Fi is off).
- The USB console drops a line now and then even when it stays up; do not count events by log lines alone.
- The ESP32-P4 before silicon revision 3 is bricked by rev3-only firmware (boot loop at bootloader entry). The
  Waveshare ESP32-P4-WIFI6 rev1.3 board is such pre-rev3 silicon.
- S3, C3 and C6 all enumerate as `303a:1001`; confirm the chip with `esptool --port <port> chip_id` before flashing.
- ESP32-C6 test configs: set `wifi: power_save_mode: none`. With the default light sleep, API commands and state
  updates arrived seconds late and a 1.2 s step check failed at random on base and PR alike (esphome#22339).
- An S3 on Arduino with `logger: hardware_uart: USB_CDC` enumerates as TinyUSB CDC (`303a:0009`). If that build
  crash loops (a FastLED plus NeoPixelBus build aborts in `check_rmt_legacy_driver_conflict`), esptool cannot catch
  the port: hold BOOT, tap RST, keep BOOT held about 2 s, then flash with `--before no-reset`. A plain RTS hard reset
  leaves it in the loader on USB JTAG; `--after watchdog-reset` boots the app. Keep Arduino LED tests on the classic
  ESP32.
- A board left in the ROM bootloader after an upload (BOOT held, or the USB JTAG reset landing in download mode
  again) starts its app without a replug through `esptool --port <port> --after watchdog-reset chip-id`; flash a
  minimal Wi-Fi plus API image first when recovering from a crash-looping one.

## RP2040 / RP2350 (Pico W, Pico 2 W)

- The CDC port re-enumerates after every reboot and after `esphome upload` (1200 baud touch). Re-detect
  with `ports.py --wait 2e8a` before capturing; `serial_capture.py` reopens on its own if the port
  vanishes mid-capture.
- First flash ever: BOOTSEL, then `esphome upload --device /Volumes/RPI-RP2` (or `RP2350`).
- arduino-pico creates the lwIP netif in `WiFi.begin()`, so any IGMP join during component setup fails;
  joins must wait for `network::is_connected()`.
- Parallel Pico builds race on package extraction after a PlatformIO package wipe; one at a time.
- An RTS pulse from pyserial does not reset the board; reboot with `App.safe_reboot()` or the touch.
- LEAmDNS re-enters `update()` from the receive handler on the Pico too (measured), so an ESP8266-only guard does
  not cover it.
- An image with only `ota: platform: web_server`, built from a tree without the fix for uploads of unknown size
  (before 2026-10-19), cannot be updated again over the network; see `recipes/ota.md` before flashing one.
- RP2350 `panic()` goes through `_exit()` and `__breakpoint()` to a HardFault, and ESPHome's rp2 crash handler
  reboots within about a second (USB back 1 to 2 s after the panic). A panic during component setup therefore
  boot-loops before USB CDC ever enumerates, which looks like a dead board: no port and no BOOTSEL drive. Recover
  with BOOTSEL held on replug, then flash to the `RP2350` volume. To see the crash, trigger it after boot.
- A Pico 2 W can sample any of its own GPIOs from a spare PIO block (PIO2) at 25 MHz while another peripheral
  drives the pin, since PIO reads the pad without taking it. That decodes WS2812-style pulses with no wires or
  LEDs. Turn a light on at full brightness before probing, or color correction writes zeros. The sample FIFO
  overruns now and then; count only full captures.
- A Pico 2 W also makes a byte-exact fake I2C sensor: a raw RP2350 I2C0 interrupt handler (GP4/GP5) that answers
  each RD_REQ with one byte from a 256-byte register file, so any read length works (used for BME280/BMP280 in
  esphome#22236). The ESP32-S3 I2C slave v2 driver is not usable for register emulation: answering from a task or
  straight from the interrupt both gave shifted multi-byte reads.

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
- The `rtw_is_adapter_up` bus fault after `Disabling AP` happens on LibreTiny 1.13.0 and 1.14.0 alike
  (2 of 4 and 3 of 3 runs in #21531); run at least four rounds per build before blaming a PR for it.
- On a fault the ROM handler (ram_vector_table entries 3 to 7) prints a dump of about 4 KB in 0.4 s and
  then spins; only the watchdog resets the chip (10 s after `arch_init`, 1 s once the ESPHome crash
  handler re-arms it). `.ram.bss.noinit*` (`.system_restore_data`) survives that reset. The core has no
  FPU. For a crash that fires once, guard an `interval` lambda with
  `!libretiny::crash_handler_has_data()`. For a handler mode (MSP) fault, point vector 16+11 (SDIOD,
  unused) at a trapping function and set bit 11 in NVIC ISER and ISPR (0xE000E100, 0xE000E200).
- OTA slots (esphome#22012): a serial `ltchiptool flash write` fills both OTA1 and OTA2, and the slot flag decides
  which one boots (`OTA1 USE` or `OTA2 USE` in the ROM banner). Each OTA writes the other slot, so a test that needs
  the OTA1 to OTA2 direction runs two OTAs in a row. On dev before #22111 a deflate OTA into OTA2 corrupted the
  image, and the next restart hung at `System_Init1` until a serial reflash.
- If the chip still waits in the ROM after a serial flash, pulse CEN to GND once (TX2 floating) to boot the app.
  `expected ACK` failures at random blocks mid-transfer can come from one USB hub port; another port fixed them.
- Its Wi-Fi is slow and drops (esphome#21742): a 300 KB deflate OTA takes 3 to 10 minutes, so give the uploader 30
  minutes, not 10.
- Wrong-MD5 OTA test without changing firmware: monkeypatch `espota2.hashlib.md5` so only the image checksum (data
  over 64 KB) returns a wrong hexdigest. Give each build its own `name:` and read `device_info().name` after a
  restart to see which image booted.

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
- Both flash wrappers pass ltchiptool `-L` with the newest installed LibreTiny platform (`scripts/lt_platform.py`):
  its default `~/.platformio/platforms/libretiny` can be an older copy that rejects BK7238 UF2s as unsupported.
- Crashes: `__builtin_trap()` and a call into unmapped memory both end as an undefined-instruction trap
  (`bk_misc_get_start_type()` 6, `lt_get_reboot_reason()` Crash); reads, unaligned loads and stores to
  unmapped addresses never fault on the ARM968. Without Wi-Fi the SDK trap handler spins with interrupts off
  for 10 s and the hardware watchdog resets the chip. With Wi-Fi connected a dev build hangs forever after a
  crash, and never resets from a plain `portDISABLE_INTERRUPTS(); while (1) {}` either: the boot-time
  watchdog is dead once Wi-Fi is up, for any lockup. The crash handler fix (#21012) re-arms the watchdog
  from its wrapper, after which the reboot comes within a second (BK7238 and BK7231N).
- The SDK prints nothing on UART1 under `sdk_silent: all`; the bootloader then prints the banked
  `cpsr/spsr/r13/r14` of SVC, IRQ, FIQ and SYS plus `ST:` (the retention register) before `go os_addr`.
  A CEN or power reset wipes ITCM (reads `0xaaaaaaaa`), so only a watchdog reset keeps a record there. What
  survives that reboot on the BK7238: ITCM (`0x3F0000..0x3F4000`) and the top 96 KB of SRAM
  (`0x428000..0x440000`, the heap); the bootloader zeroes SRAM up to about `0x420000` and rewrites
  `0x400024..0x4000FF` with its own register dump every boot, so neither a `.noinit` after `.bss` nor the
  SDK's saved register banks are usable. Code is compiled Thumb, the SDK assembly is ARM; in an exception
  handler only r0 to r3 are shared by all modes. BK7231N ITCM is `0x3FEE00..0x400000` (4.5 KB); a `.noinit`
  placed there after `.itcm.code` works for a crash record.
- Pick the board that matches the module's flash layout. A NiceMCU XH-WB3S (BK7238) needs `board: xh-wb3s` (plain
  Beken layout, RF calibration at 0x1E0000). Built as `generic-bk7238-tuya` (calibration at 0x1E3000, download
  partition over 0x1E0000) the firmware boots and may even join a known network, but scans come back empty and its
  AP is invisible to clients, with no warning in the log. If scans are empty, check the layout first.
- The BK7238 bootloader prints `App verify failed! Need to recovery factory firmware.` on every boot with any
  build; it is harmless. Even with the right layout an ESP32 client did not see the BK7238 fallback AP on
  LibreTiny 1.13.0 or 1.14.0, so the wifisave hammer cannot run on it.
- Moving a BK7231N device (CB3S) that sits on its fallback AP to another network: join the host's Wi-Fi to
  `<name>-fallback`, OTA to 192.168.4.1, then rejoin the host's own network. Moving it with `/wifisave` instead can
  leave it in the captive portal's BLIND_RETRY state (no scan, `Beacon Timeout` every 46 s) until it reboots, and a
  log-only serial adapter cannot reset it. Rejoining may change the host's address; update a syslog target that
  points at the old one.
- Callbacks run on the `async_tcp` task and log lines can lose a byte. Wi-Fi connect takes tens of ms;
  radio enable 160 to 278 ms, disable about 200 ms.
- OTA install: the BK7238 bootloader installs any complete download whose own CRC passes at the next restart, even
  when ESPHome refused the update; an incomplete download fails `rt_ota_part_fw_verify` (`Verify firmware CRC32
  ... !=`) and the old image stays.
- Power save: on the BK7238, `power_save_mode: light` with LibreTiny 1.14.0 gives ping times of 3 to 6 s because
  the BDK sets a listen interval of 30 beacons when `CFG_LOW_VOLTAGE_PS` is 1 (esphome#19491).

## BW12 and other AmebaZ boards (RTL8710BX, LibreTiny rtl87xx)

- On a bare module wired to a plain USB-UART (the bench's BW12), download mode is a hand strap: CEN to
  GND, TX2 to GND, release CEN, release TX2. Such an adapter does not drive CEN, so neither ltchiptool nor
  `serial_capture.py --reset` can reset the chip. A development board that wires DTR and RTS to CEN and the
  download pin enters download mode from the tools, like the BW15 kit; the notes below on baud rates still apply.
- ltchiptool links at 1.5 Mbaud and reads the system data at 0x9000 at that rate before it writes at
  115200. A CH340 drops bytes there (`Data read error at 0x9000`, `Timed out waiting for Quiet-Handshake`)
  and the run gives up without writing. `-B 115200 -b 115200` moves the link to 115200 first and works.
  Start the command with `-t 120` so it is already polling while the board is strapped.
- After a failed run the ROM may sit at another baud rate and no longer link; strap again.
- The device builder frontend's AmebaZ flasher follows ltchiptool (link and system read at 1.5 Mbaud),
  so it can hit the same read failure on the same adapters.
- Power the module from the adapter's 5 V pin into its own regulator; the adapter's 3.3 V pin cannot
  carry the Wi-Fi transmit peaks.
- Capture with `serial_capture.py --released`. With DTR and RTS asserted on the open port, a software
  reboot (`App.safe_reboot()`, the restart button) lands in the ROM downloader (`UARTIMG_Download 2`)
  and stays there until CEN is pulsed. After a flash the chip also waits in the ROM until CEN is pulsed.
- A full flash takes 50 to 220 s at 115200. While the board is on Wi-Fi, OTA (port 8892) saves the strap,
  but the board's Wi-Fi latency (one to two seconds a ping) can make the upload time out.
- `sdk_silent: none` under `framework:` keeps the Wi-Fi driver's join log (`start auth to`, `auth success,
  start assoc`, `association success`, `sta recv deauth reason code(N)`); LibreTiny mutes it during the
  connect otherwise. `wifi_get_last_error()` after a failed `WiFi.begin()` gives the SDK's reason
  (1 no network, 2 connect failed, 3 handshake failed, 6 auth failed); LibreTiny only logs `ret=-1`.
- Some modules ship with no RF calibration in efuse, only the chip ID and MAC. Dump the logical map with
  the ROM's `EFUSE_LogicalMap_Read(buf)` (512 bytes): TX power indexes are at 0x10 to 0x1A, crystal trim
  at 0xC9, MAC at 0x11A; 0xFF means the driver uses its default (trim 0x20). Linking with
  `-Wl,-wrap,EFUSE_LogicalMap_Read_ACUT` lets a wrapper rewrite blank bytes before the driver parses
  the map on each radio start. See `recipes/libretiny.md` for the per-boot trial harness used to A/B them.
- Scan results report WPA2-AES access points as `WPA PSK`: LibreTiny truncates `rtw_security_t` to
  `uint8_t` before mapping it. Cosmetic. Open networks come out as `WIFI_AUTH_INVALID` (255) on both
  AmebaZ and AmebaZ2, so `auth != WIFI_AUTH_OPEN` marks them encrypted.
- After a BSSID plus channel join, LibreTiny's `set_pscan_channel(ch, false)` leaves the driver with no
  channel to scan, so the next SSID only join fails at once (no `start auth`), and the first scan after a BSSID
  join comes back empty (no results in about 62 ms on the BW15). A `wifi_set_pscan_chan` list of channels 1 to 13
  returns 0 but leaves only the last channel enabled, and re-applying the channel plan does not help either
  (AmebaZ2 reports plan 0xFF; AmebaZ's `rtw_set_chplan_cmd` rejects plans above 0x58 except 0x7F). What works
  (esphome#21749, BW15): set the one-channel pscan list yourself and pass channel 0, so the join's own scan
  rebuilds the full list; setting a placeholder SSID `-` before narrowing (esphome#21754) kept joins as fast as
  dev with the first scan full on every boot. Not yet confirmed on AmebaZ.
- A 4-way handshake that never finishes after association is reported as `RTW_WRONG_PASSWORD` on AmebaZ
  even with the right password; AmebaZ2 reports a real wrong password as a 4-way handshake timeout.
- The driver also posts a SCAN_DONE (number 0) inside every join, on AmebaZ2 too.
- RTL8710B GPIO interrupts fire on one edge only, and LibreTiny maps CHANGE to rising. The Arduino `digitalRead` called
  from an ISR switches the pin back to GPIO and frees its interrupt (esphome#22400, #22403).
- The FreeRTOS tick is 1/1024 s, so `millis()` on the tick ran 2.4 % fast (esphome#22404).

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
- The ESPHome OTA port on LN882x is 8820, not LibreTiny's 8892 (BK72xx, RTL87xx); helper scripts that hardcode
  8892 get `Connection refused`.
- WL2H-U modules with the Tuya flash layout: app at 0x007000 (0x113000 long), OTA at 0x11A000 (0xA8000 long). A
  full OTA upload over a slow link takes 2 to 14 minutes.
- UF2 block order differs per family: LN882H puts the device image first and the flasher image after it, BK72xx
  puts the flasher image first and the device image last. The LN882H bootloader installs a complete, valid OTA
  partition even when the upgrade state is still `UPG_STATE_DOWNLOAD_ING` (esphome#22111).
- Without serial access to recover, never send a corrupted image; wrong-MD5 and cut-upload tests with valid
  images are safe.

## nRF52840

- Nordic PCA10059 dongle: the factory Open DFU bootloader enumerates as `1915:521F` on its own port
  name; the app as `2FE3:0100` with two CDC ports, ESPHome log on the second. Only Nordic
  `nrfutil nrf5sdk-tools` works (`pkg generate --hw-version 52 --sd-req 0x00 --application-version 1
  --application zephyr.hex app.zip`, then `dfu usb-serial -pkg app.zip -p <port>`); ESPHome downloads
  it on the first build. GPREGRET does not enter the bootloader; a pin reset through P0.19 does, which
  `dfu: true` wires up on this board. A pulsing red LED means bootloader; the side RESET button
  re-enters it.
- Makerdiary MDK USB dongle: UF2 bootloader without SoftDevice, app at 0x1000, `UF2BOOT` volume,
  adafruit-nrfutil, GPREGRET 0x57 works. The running app enumerates as `1915:CAFE` or `239A:0029`. RGB LED
  active low (red P0.23, green P0.22, blue P0.24), button P0.18.
- Both boards reflash without a button once they run a `dfu: true` build: `esphome upload --device <any app CDC
  port>` on the PCA10059, a 1200 bps open of the log port plus a copy of `zephyr.uf2` to the volume on the ItsyBitsy.
- Adafruit ItsyBitsy nRF52840 Express (UF2 bootloader 0.9.0 with S140 6.1.1, bootloader port `239a:0051` plus an
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
- Thread: nRF52 `openthread:` accepts only `tlv:`; the individual keys (network_key, channel, ...) are rejected at
  validation. To list nearby networks with channel and PAN ID, call
  `otThreadDiscover(InstanceLock::acquire().get_instance(), 0, 0xFFFF, false, false, cb, nullptr)` from an
  `interval:` lambda in an included header; results arrive on the OpenThread thread and log fine.
- A wrong network key on a real network's channel and PAN ID makes the device form its own partition as leader on
  that PAN within about 30 s; take it off Thread at once. Never try guessed datasets against a network in use.

## Waveshare ESP32-S3-ETH with the OV5640 camera module

- Native USB console (`303a:1001`), ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM, W5500 on SPI (MOSI 11,
  MISO 12, SCLK 13, CS 14, INT 10, RST 9), TF card on 4..7, WS2812 on 21.
- Camera header (vendor schematic): XCLK 3, SIOD 48, SIOC 47, VSYNC 1 (a 0 ohm link to GPIO1, alternative
  GPIO16), HREF 2, PCLK 39, D0..D7 = 41, 45, 46, 42, 40, 38, 15, 18. PWDN is tied to GND and RESET is an RC to
  3.3 V on the board, so no reset or power-down GPIO exists.
- It can run from PoE and USB-C at once, so the USB-C can move from the host to a USB device for `usb_uart` or USB
  host tests while the board stays up; flash and read logs over Ethernet while it is unplugged. In USB host mode
  the USB-C is the host port after the first flash: log over the API with `logger: hardware_uart: UART0`.
- A crash-looping image can wedge its USB console until a BOOT replug. Never force allocation failures on it by
  hogging RAM: Wi-Fi then aborts in `operator new` or the loop task trips the task watchdog.
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

## Building against a patched LibreTiny

- Clone LibreTiny at the tag ESPHome pins, commit the patch, and set `framework: version: <tag version>` plus
  `source: file://<clone>` under `bk72xx:` or `rtl87xx:`. PlatformIO installs it as
  `~/.platformio/platforms/libretiny` (the stock copy is `libretiny@src-<hash>` and is not touched).
- A later commit in the clone is not picked up by the next build; patch the installed copy or remove it. Remove
  `~/.platformio/platforms/libretiny` when done.
- BDK headers such as `power_save_pub.h` have no `extern "C"`; wrap them when including them from the Arduino
  core's C++ files.

## LibreTiny crash and abort() facts (esphome#21596, #22288)

- `abort()` on dev: the BK7238 hangs for good (the main loop stops and the watchdog never fires once Wi-Fi is up;
  recover with an RTS pulse on CEN, `serial_capture.py --reset`). The BW15 (RTL8720CF) restarts with no crash
  record. The LN882H reboots only after about 18 s through the watchdog (`WDT Reset`), with no record: the SDK's
  `abort()` and `_exit()` (`components/libc/newlib_stub.c`) and its FreeRTOS `configASSERT` are spin loops. With
  #22288 all three reboot in 1 to 2 s and log `Reason: Abort`, with the PC at the call site and no `LR:` line.
- `assert()`, `std::terminate` and a failed `new` all reach `abort()` (nano libsupc++). `raise(SIGABRT)` returns,
  because `_kill` is a nosys stub.
- `__builtin_trap()` faults on every board. A store to `nullptr` or 0xDEADBEE0 does not fault on the BK7238. On
  the BW15 a `nullptr` store does not fault, and loads and stores at 0xDEADBEE0 give only imprecise bus faults
  (CFSR 0x400, no BFARVALID), so testing the fault address path needs another Cortex-M chip.
- BK7238 safe mode: each quick crash reboot counts toward it (`Last reset too quick; invoke in N restarts`), so
  back-to-back crash tests can drop the board into safe mode.
- LN882H: the previous crash is replayed to every new API log client until the next crash; that is expected, not a
  double record. Once, the first `__builtin_trap()` after a fresh OTA took about 12 s to reboot; retries took 2 to
  3 s and it did not reproduce.
- `~/.platformio/packages/toolchain-gccarmnoneeabi` can vanish between a build and the decode (it comes back with
  the next LibreTiny build). Until then the `arm-none-eabi-addr2line` and objdump of
  `toolchain-rp2040-earlephilhower` read any ARM ELF.
- RTL8720C: a noreturn `bl abort` can be the last instruction of a function, so the raw return address points at
  padding addr2line cannot place. Decode `(addr & ~1) - 2` to get the call line; #22288 records it that way.
- Abort reports (#21596): early heads put LR 0x00000000 first in the address list; later heads drop a zero LR, so
  an abort report starts with the backtrace.

## Waveshare ESP32-C6-GEEK

- Native USB JTAG serial (`303a:1001`), 16 MB flash, 1.14" ST7789 LCD and TF card; the vendor docs give no GPIO
  numbers. Pins that read a fixed level against the internal pulls: 1, 2, 7, 20, 23 high, 6 low. Free for pad
  tests: 0, 3, 4, 5, 18, 19, 21, 22 (see `recipes/gpio_hold.md`).

## Waveshare ESP32-P4-Module-DEV-KIT (USB host)

- ESP32-P4 rev v1.3, 16 MB flash, console on a CH343 (`1a86:55d3`). Other boards use the same chip for their
  console, so check with `esptool chip_id`.
- USB host for `usb_host`: set the Host/Device jumper to HOST and use the bottom USB-A port nearest the Ethernet
  jack (the high-speed OTG controller); the other USB-A ports and the USB-C marked "USB" do not reach it. The ports
  are unlabelled. The port powers the device.
- ESP-IDF programs the FIFO split only when a device connects: read `GRXFSIZ`, `GNPTXFSIZ` and `HPTXFSIZ` at
  `DR_REG_USB2_BASE` + 0x24, 0x28 and 0x100 after enumeration. GHWCFG3 (0x4C, high 16 bits) reads 896 lines. The
  default split is RX 512, NPTX 256, PTX 128.
- Not every P4 board has a USB host port; check before planning a `usb_host` test on one.

## Gelidus Research PS1 v2.3 (LD2420 presence sensor)

- ESP32-D0WD-V3 rev 3.1 on a CH340 (`1a86:7523`); `esphome upload --device <port>` works directly.
- LD2420 UART: TX GPIO17, RX GPIO16, 115200 (radar firmware v1.6.1). Status LED GPIO23, night light PWM on GPIO25.
- The vendor package is at github.com/GelidusResearch/grps (`packages/grps-ld2420.yaml`); copy it locally for tests.
- The radar needs a person in its field. Energy frames are `F4 F3 F2 F1 23 00 <presence> <range cm LE16> ...`; a
  UART `debug:` block on RX shows them.

## ESP32-C3 Super Mini (native USB, 4 MB XMC flash)

- Shows up as `303a:1001`; confirm with `esptool --port <port> chip_id`. Board `esp32-c3-devkitm-1`,
  `logger: hardware_uart: USB_SERIAL_JTAG`. Avoid GPIO2, 8 and 9 (strapping; 8 is the LED, 9 is BOOT).
- A 6-pin BME680 breakout (CS, ADDR/MISO, SCL, SDA, GND, VCC) on SDA GPIO6 and SCL GPIO7 with CS and ADDR left
  open answers at 0x77 through the breakout's pull-ups, and the `bme680` platform reads fine.
- BME688 parallel mode never reads in `bme68x_bsec2` on dev: the read timer is re-armed by each 30 ms run before
  its 41.6 ms delay ends. Entering parallel mode at all needs gas estimates subscribed at `BSEC_SAMPLE_RATE_SCAN`
  and no IAQ outputs.

## mDNS responder tests (#22267)

- When the host's multicast does not reach the device, query its port 5353 directly as legacy unicast (a
  python-zeroconf packet sent from a normal UDP socket) to dump records or hammer the responder.
- On a crowded Wi-Fi channel, back-to-back query loss swung from 0 to 14% between builds for no code reason. Judge
  responder timing on an Ethernet board.

## nRF54LM20 DK (PCA10184, on-board J-Link)

- The power switch must be ON or nothing enumerates. The ESPHome log (uart20) is on the second J-Link VCOM port;
  the first is unused.
- Flash with `esphome upload <yaml> --device JLINK` (sdk-nrf toolchain); the LM20 needs no west `--device` override.
  The J-Link re-enumerates after the flash, so start the capture after the port comes back.
- Reset without halting: JLinkExe `-device NRF54LM20A_M33 -if SWD -autoconnect 1` with `w4 0xE000ED0C 0x05FA0004`
  (plain `r` and `g` halt and resume, no reboot).
- LED0 P1.22, LED1 P1.25; BUTTON0 P1.26, BUTTON1 P1.09, BUTTON2 P1.08, BUTTON3 P0.05 (active low, pull-up). GPIO
  P1 registers at 0x500D8200 (IN +0x00C, PIN_CNF +0x080), P0 at 0x5010A200.
- Binary sensor state changes are not logged at DEBUG; log the states or count `on_press` to see button presses.
