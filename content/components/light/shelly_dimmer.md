---
description: "Instructions for setting up a Shelly Dimmer 2."
title: "Shelly Dimmer"
params:
  seo:
    description: Instructions for setting up a Shelly Dimmer 2.
    image: shellydimmer2.jpg
---

The `shelly_dimmer` component adds support for the dimming and power-metering functionality that can be found the [Shelly Dimmer 2](https://shelly.cloud/knowledge-base/devices/shelly-dimmer-2/). The interaction with mains is done via an STM32 microcontroller that is automatically (when configured) flashed with an [open source firmware](https://github.com/jamesturton/shelly-dimmer-stm32).
A detailed analysis of the Shelly Dimmer 2 hardware is given on<https://github.com/arendst/Tasmota/issues/6914>.

Warning!!! At the time of writing there seems to be no way to revert back to the "stock firmware", because there seems to be no way to revert to firmware of the STM32 co-processor.

{{< img src="shellydimmer2.jpg" alt="Image" width="40.0%" class="align-center" >}}

An example of a configuration of this component:

```yaml
logger:
    baud_rate: 0

uart:
    tx_pin: 1
    rx_pin: 3
    baud_rate: 115200
sensor:

light:
    - platform: shelly_dimmer
      name: Shelly Dimmer 2 Light
      id: thislight
      power:
        name: Shelly Dimmer 2 Light Power
      voltage:
        name: Shelly Dimmer 2 Light Voltage
      current:
        name: Shelly Dimmer 2 Light Current
      max_brightness: 500
      firmware:
        version: "51.6"
        update: true
```

## Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the UART hub.

> [!NOTE]
> Currently, only the first hardware UART of the ESP is supported, which has to be configured like this:
>
> ```yaml
> uart:
>     tx_pin: 1
>     rx_pin: 3
>     baud_rate: 115200
> ```

- **leading_edge** (*Optional*, boolean): [Dimming mode](https://en.wikipedia.org/wiki/Dimmer#Solid-state_dimmer): `true` means leading edge, `false` is trailing edge. Defaults to `false`.
- **min_brightness** (*Optional*, int): Minimum brightness value on a scale from 0..1000, the default is 0.
- **max_brightness** (*Optional*, int): Maximum brightness value on a scale from 0..1000, the default is 1000.
- **warmup_brightness** (*Optional*, int): Brightness threshold below which the dimmer switches on later in mains current cycle. [This might help with dimming LEDs](https://github.com/jamesturton/shelly-dimmer-stm32/pull/23). The value is from 0..1000 with an default of 0.
- **nrst_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): Pin connected with "NRST" of STM32. The default is "GPIO5".
- **boot0_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): Pin connected with "BOOT0" of STM32. The default is "GPIO4".
- **current** (*Optional*): Sensor of the current in Amperes. All options from
  [Sensor](/components/sensor).

- **voltage** (*Optional*): Sensor of the voltage in Volts. Only accurate if neutral is connected. All options from [Sensor](/components/sensor).
- **power** (*Optional*): Sensor of the active power in Watts. Only accurate if neutral is connected. All options from [Sensor](/components/sensor).
- **firmware** (*Optional*):

  - **version** (*Optional*): Version string of the [firmware](https://github.com/jamesturton/shelly-dimmer-stm32) that will be expected on the microcontroller. The default is "51.6", another known-good firmware is "51.5".
  - **url** (*Optional*, string): An URL to download the firmware from. Defaults to github for known firmware versions.
  - **sha256** (*Optional*): A hash to compare the downloaded firmware against. Defaults a proper hash of known firmware versions.
  - **update** (*Optional*): Should the firmware of the STM be updated if necessary? The default is false.

> [!NOTE]
> When flashing Shelly Dimmer with esphome for the first time, automatic flashing the STM firmware is necessary too for the dimmer to work and enabled by the following configuration.:
>
> ```yaml
> firmware:
>   version: "51.6" #<-- set version here
>   update: true
> ```
>
> There is no action required by the user to flash the STM32. There is no way to revert to stock firmware on the STM32 at the time of writing.

- All other options from [Light](/components/light#config-light).

## See Also

- {{< docref "/components/light" >}}
- {{< apiref "shelly_dimmer/light/shelly_dimmer.h" "shelly_dimmer/light/shelly_dimmer.h" >}}
