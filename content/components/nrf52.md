---
description: "Configuration for the NRF52 platform for ESPHome."
title: "NRF52 Platform"
params:
  seo:
    description: Configuration for the NRF52 platform for ESPHome.
    image: nrf52.svg
---

This component contains platform-specific options for the NRF52 platform.

> [!NOTE]
> Support for all aspects of ESPHome on the NRF52 is still in development.

```yaml
# Example configuration entry
nrf52:
  board: adafruit_feather_nrf52840
```

## Configuration variables

- **board** (*Required*, string): The board type. Valid options are `adafruit_feather_nrf52840`, `adafruit_itsybitsy_nrf52840`, `xiao_ble`. Other boards should work with those configuration as well.
- **bootloader** (*Optional*, string): Bootloader type. Valid options are `mcuboot`, `adafruit`, `adafruit_nrf52_sd132`, `adafruit_nrf52_sd140_v6`, `adafruit_nrf52_sd140_v7`. Default value depends on board type.
- **dcdc** (*Optional*, boolean): Enable DC/DC converter for REG1 stage. Defaults to `true`.
External LC filters must be connected to the DC/DC regulator pins if it is being used.
The advantage of using a DC/DC regulator is that the overall power consumption is normally reduced
as the efficiency of such a regulator is higher than that of a LDO.
⚠️ Warning: Enabling DC/DC may cause the board to fail to boot if external LC filter is misconfigured or is poor quality.

## Getting Started

The nRF52840 requires a bootloader, with two supported options: `MCUboot` and `Adafruit nRF52 Bootloader`. It is recommended to use MCUboot as it supports OTA (Over-The-Air) updates. Your board most likely comes with a manufacturer-provided bootloader. ESPHome determines the bootloader type based on the board name.

Examples of low power [nRF52840 boards](https://github.com/joric/nrfmicro/wiki).

## Flashing with MCUboot

Flashing this bootloader requires an SWD connection, for which a programmer is necessary. A cheap ST-Link V2 can be utilized.

1. Connect the board to the PC via SWD.
1. Run `esphome upload yourfile.yaml --device PYOCD`.

```yaml
# Example configuration entry
nrf52:
  board: adafruit_feather_nrf52840
```

## Flashing with Adafruit nRF52 Bootloader

For flashing via a flash drive.

1. Connect the board to the PC via USB.
1. Quickly short the reset pin to ground twice.
1. Copy the UF2 package to the flash drive.

This bootloader supports updates over USB CDC.

1. Connect the board to the PC via USB.
1. Quickly short the reset pin to ground twice.
1. Run `esphome upload yourfile.yaml`.

```yaml
# Example configuration entry
nrf52:
  board: adafruit_itsybitsy_nrf52840
```

## GPIO Pin Numbering

There are two ways to reference GPIO pins:

1. By pin name, e.g., `P0.15` or `P1.11`.
1. By pin number, e.g., `15` or `43`.

## DFU (Device Firmware Update)

The `dfu` component enables automatic entry into **DFU (Device Firmware Update)** mode by monitoring
the USB CDC serial connection. When a host opens the port at **1200 baud**, the component triggers
a reset via a GPIO pin to put the device into DFU mode.

ESPHome uses this component internally when uploading firmware via:

```bash
esphome upload d.yaml
```

### Example Configuration

```yaml
nrf52:
  dfu:
    reset_pin:
      number: 14
      inverted: true
```

### Configuration variables

- **reset_pin** (*Required*, [Pin](/guides/configuration-types#pin)): The pin to use for trigger a hardware reset. This pin should be connected to the MCU's reset line or to a circuit that causes the bootloader to enter DFU mode after reset.

## REGOUT0

Output voltage from the REG0 regulator stage, which powers the GPIO pins when the board operates in high-voltage mode.
This setting can only be changed a limited number of times, unless uicr_erase is set to true.
Requires `mcuboot` or `adafruit` bootloader version 0.9.3 or higher.

### Example Configuration

```yaml
nrf52:
  reg0:
    voltage: 3.3V
    uicr_erase: true
```

### Configuration variables

- **voltage** (**Required**, voltage): The desired output voltage - must be one of
  1.8V, 2.1V, 2.4V, 2.7V, 3.0V, 3.3V.
- **uicr_erase** (**Optional**, bool): If set to true, the User Information Configuration Registers (UICR)
will be erased before writing the new voltage setting.
⚠️ Warning: Enabling this may cause the board to fail to boot if misconfigured. Default is false.

## Troubleshooting

### Flashing is unstable

If you are using the Adafruit bootloader, upgrade to the latest version:
[Adafruit nRF52 Bootloader Releases](https://github.com/adafruit/Adafruit_nRF52_Bootloader/releases)

### How to start

Try minimum LED blinking config for the board:

[supermini-nrf52840](https://github.com/joric/nrfmicro/wiki/Alternatives#supermini-nrf52840)

```yaml
nrf52:
  board: adafruit_itsybitsy_nrf52840

esphome:
  name: supermini-nrf52840

logger:
  level: DEBUG

output:
  - platform: gpio
    pin: P0.15
    id: red_led

interval:
  - interval: 1s
    then:
      - output.turn_on: red_led
      - delay: 0.5s
      - output.turn_off: red_led
```

[xiao-nrf52840](https://wiki.seeedstudio.com/XIAO_BLE/)

```yaml
nrf52:
  board: xiao_ble

esphome:
  name: xiao-nrf52840

logger:
  level: DEBUG

output:
  - platform: gpio
    pin: P0.26
    id: red_led

interval:
  - interval: 1s
    then:
      - output.turn_on: red_led
      - delay: 0.5s
      - output.turn_off: red_led
```

### Board does not boot

Disable DC/DC:

```yaml
nrf52:
  dcdc: false
```

## See Also

- {{< docref "esphome/" >}}

- [Guidelines for Adafruit Bootloader Memory Map](https://learn.adafruit.com/introducing-the-adafruit-nrf52840-feather?view=all#hathach-memory-map)
