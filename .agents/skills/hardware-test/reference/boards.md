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
  channel to scan, so the next SSID only join fails at once (no `start auth`). Calling
  `wifi_set_pscan_chan` with channels 1 to 13 and `PSCAN_ENABLE` each before such a join fixes it and
  keeps the BSSID fast path (BW15 BSSID join median 3.05 s with it, 4.68 s when joining with channel 0).
- A 4-way handshake that never finishes after association is reported as `RTW_WRONG_PASSWORD` on AmebaZ
  even with the right password; AmebaZ2 reports a real wrong password as a 4-way handshake timeout.
- The driver also posts a SCAN_DONE (number 0) inside every join, on AmebaZ2 too.

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
- Thread: nRF52 `openthread:` accepts only `tlv:`; the individual keys (network_key, channel, ...) are rejected at
  validation. To list nearby networks with channel and PAN ID, call
  `otThreadDiscover(InstanceLock::acquire().get_instance(), 0, 0xFFFF, false, false, cb, nullptr)` from an
  `interval:` lambda in an included header; results arrive on the OpenThread thread and log fine.
- A wrong network key on a real network's channel and PAN ID makes the device form its own partition as leader on
  that PAN within about 30 s; take it off Thread at once. Never try guessed datasets against a network in use.

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
