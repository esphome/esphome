---
description: "Configuration for the LibreTiny platform for ESPHome."
title: "LibreTiny Platform"
params:
  seo:
    description: Configuration for the LibreTiny platform for ESPHome.
    image: libretiny.svg
---

This component contains platform-specific options for the [LibreTiny](https://docs.libretiny.eu/) platform.
It provides support for the following microcontrollers, commonly used in Tuya devices, amongst others:

- **BK72xx**: BK7231T, BK7231N
- **RTL87xx**: RTL8710BN, RTL8710BX
- **LN882x**: LN882HKI

Since different microcontrollers are supported, you need to include the appropriate ESPHome component,
depending on which processor your device has.

Refer to [LibreTiny/Boards](https://docs.libretiny.eu/link/boards) to find your board type.

```yaml
# Example configuration entry for BK72xx
bk72xx:
  board: generic-bk7231n-qfn32-tuya

# Example configuration entry for RTL87xx
rtl87xx:
  board: generic-rtl8710bn-2mb-788k

# Example configuration entry for LN882x
ln882x:
  board: generic-ln882hki
```

## Configuration variables

- **board** (**Required**, string): The PlatformIO board ID that should be used. Choose the appropriate board from
  [this list](https://registry.platformio.org/packages/platforms/kuba2k2/libretiny/boards)
  (the icon next to the name can be used to copy the board ID).

  **This affects CPU selection and some internal settings** - make sure to choose the right CPU.
  If unsure about the choice of a particular board, choose a generic board such as `generic-bk7231n-qfn32-tuya`.

- **framework** (*Optional*): Options for the underlying framework used by ESPHome.

  - **version** (*Optional*, string): The LibreTiny version number to use, from
    [LibreTiny platform releases](https://github.com/kuba2k2/libretiny/releases). Defaults to `recommended`. Additional values

    - `dev`  : Use the latest commit from <https://github.com/kuba2k2/libretiny>, note this may break at any time
    - `latest`  : Use the latest *release* from <https://github.com/kuba2k2/libretiny/releases>, even if it hasn't been recommended yet.
    - `recommended`  : Use the recommended framework version.

  - **source** (*Optional*, string): The PlatformIO package or repository to use for the framework. This can be used to use a custom or patched version of the framework.

  - [Advanced options](#advanced-options)

- **family** (*Optional*, string): The family of LibreTiny-supported microcontrollers that is used on this board.
  One of `bk7231n`, `bk7231t`, `rtl8710b`, `rtl8720c`, `bk7251`, `bk7231q`, `ln882hki`.
  Defaults to the variant that is detected from the board, if a board that's unknown to ESPHome is used,
  this option is mandatory. **It's recommended not to include this option**.

> [!NOTE]
> Support for the LibreTiny platform is still in development and there could be issues or missing components.
>
> Please report any issues on [LibreTiny GitHub](https://github.com/kuba2k2/libretiny).

## Getting Started

Since BK72xx and RTL87xx chips are relatively new on the IoT Open Source development stage,
there aren't many resources on flashing and configuring them.

Here are a few useful links:

- [tuya-cloudcutter](https://github.com/tuya-cloudcutter/tuya-cloudcutter) - flashing ESPHome Over-the-Air
  to some devices compatible with Tuya/SmartLife apps (BK72xx only!)

  - [Textual & video guide by digiblurDIY](https://docs.libretiny.eu/link/cloudcutter-digiblur)
  - [Video guide by LibreTiny](https://docs.libretiny.eu/link/cloudcutter-video)
  - [ESPHome-Kickstart](https://docs.libretiny.eu/link/kickstart) - starter firmware to upload OTA with Cloudcutter

- [Flashing BK72xx by UART](https://docs.libretiny.eu/link/flashing-beken-72xx)
- [Flashing RTL8710B by UART](https://docs.libretiny.eu/link/flashing-realtek-ambz)
- [Flashing LN882x by UART](https://docs.libretiny.eu/docs/platform/lightning-ln882x/#flashing)
- [UPK2ESPHome](https://upk.libretiny.eu/) - generating ESPHome YAML automatically, from Cloudcutter profiles or Kickstart firmware (also BK72xx only)

## GPIO Pin Numbering

Chips supported by LibreTiny use the internal GPIO pin numbering of the boards, this means that
you don't have to worry about other kinds of pin numberings, yay!

Additionally, you can use **pin function macros** to quickly access a GPIO tied to a particular peripheral,
such as UART1 TX/RX or PWM0.
See [LibreTiny/GPIO Access](https://docs.libretiny.eu/link/gpio-access) to learn more.

Most of the popular boards (often incorrectly called "chips"), that are usually shipped with Smart Home devices,
are *supported by LibreTiny*, which means that a pinout drawing is available, with all GPIOs described.
Visit [LibreTiny/Boards](https://docs.libretiny.eu/link/boards) to find all supported boards.

The `Pin functions` table outlines all GPIOs available on the chosen board.
*You can use any of the visible names* to access a particular GPIO.

Some notes about the pins on BK72xx:

- `TX2 (P0)` and `RX2 (P1)` are used for the default {{< docref "/components/logger" >}} UART port.
- `TX1 (P11)` and `RX1 (P10)` are used for flashing firmware, as well as for {{< docref "/components/tuya" >}}.
- `ADC3 (P23)` is the only {{< docref "/components/sensor/adc" >}} available on BK7231.

Some notes about the pins on RTL8710BN/BX:

- `TX2 (PA30)` and `RX2 (PA29)` are used for flashing the firmware,
  as well as the default {{< docref "/components/logger" >}} UART port.

- `TX2 (PA30)` is additionally used to determine the boot mode on startup (similar to ESP32).
  Pulling it LOW on startup will enter "download mode".

Some notes about the pins on LN882H:

- `TX0 (PA2)` and `RX0 (PA3)` are used for flashing the firmware,
  as well as the default {{< docref "/components/logger" >}} UART port.

- `BOOT1 (PA9)` is additionally used to determine the boot mode on startup (similar to ESP32).
  Pulling it LOW on startup will enter "download mode".

Example configuration entries using various naming styles:

```yaml
# GPIO switch on P26/GPIO26 (BK72xx example)
switch:
  - platform: gpio
    name: Relay 1
    pin: P26

# GPIO binary sensor on PA12 (RTL87xx example)
binary_sensor:
  - platform: gpio
    name: "Pin PA12"
    pin: PA12

# ADC reading (BK72xx example)
sensor:
  - platform: adc
    pin: ADC3
    name: "Battery Level"

# PWM component
output:
  - platform: libretiny_pwm
    pin: PWM2
    frequency: 1000 Hz
    id: pwm_output
# using light with the PWM
light:
  - platform: monochromatic
    output: pwm_output
    name: "Kitchen Light"

# Tuya MCU on UART1 (BK72xx example)
uart:
  rx_pin: RX1
  tx_pin: TX1
  baud_rate: 9600
tuya:
```

{{< anchor "advanced-options" >}}

## Advanced options

These are some advanced configuration options of LibreTiny platform.

```yaml
# Example configuration entry
bk72xx:
  board: cb2s
  framework:
    version: dev
    loglevel: debug
    debug:
      - wifi
      - ota
    sdk_silent: auto
    uart_port: 2
    gpio_recover: false
    options:
      LT_LOG_HEAP: 1
      LT_AUTO_DOWNLOAD_REBOOT: 1
```

- **loglevel** (*Optional*, string): Logging level for LibreTiny core. Controls the output of logging messages
  from the core (doesn't affect ESPHome logger!). *These messages are only visible on the physical UART*.
  One of `verbose`, `trace` (same as `verbose`  ), `debug`, `info`,
  `warn` (default), `error`, `fatal`, `none`.

- **debug** (*Optional*, string or string list): Modules to enable LibreTiny debugging for.
  Refer to [LibreTiny/Configuration](https://docs.libretiny.eu/link/config-debug)
  for more information - some modules are enabled by default.
  One or more of `wifi`, `client`, `server`, `ssl`, `ota`, `fdb`,
  `mdns`, `lwip`, `lwip_assert`.
  Specifying `none` will disable all modules. You can also combine `none` with one or more of the modules.

- **sdk_silent** (*Optional*, string): Define the SDK logging "silent mode".
  This disables messages from vendor SDKs, which makes UART output more readable, but can hide some error messages.
  *This affects the physical UART port only*.
  Refer to [LibreTiny/Configuration](https://docs.libretiny.eu/link/config-serial) for more information.

  - `all`  : Disable all messages (default).
  - `auto`  : Disable selectively, i.e. during Wi-Fi activation.
  - `none`  : Keep all logging messages, don't disable anything.

- **uart_port** (*Optional*, int): Choose the default UART port of the framework.
  This affects LibreTiny logging messages, **as well as the default port for**
  {{< docref "/components/logger" "ESPHome logger" >}} (e.g. if you don't specify any other).
  One of 0, 1, 2. The default value is chip-specific and is chosen by LibreTiny appropriately.

- **gpio_recover** (*Optional*, boolean): Disable JTAG/SWD debugging peripherals. This may be needed
  to free GPIOs that should be used for other functions. Defaults to `true`.

- **options** (*Optional*, mapping): Custom options passed to LibreTiny platform.
  Refer to [LibreTiny/Configuration](https://docs.libretiny.eu/link/config) to see all options.
  *This takes precedence (overrides) all options described above*.

## See Also

- {{< docref "esphome/" >}}
- {{< docref "/components/output/libretiny_pwm" >}}
- {{< docref "/components/text_sensor/libretiny" >}}
- [LibreTiny Documentation](https://docs.libretiny.eu/) (external)
- {{< docref "/components/tuya" >}}
