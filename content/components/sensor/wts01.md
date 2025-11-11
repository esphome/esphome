---
description: "Instructions for setting up WTS01 temperature sensors in ESPHome."
title: "WTS01 Temperature Sensor"
params:
  seo:
    description: Instructions for setting up WTS01 temperature sensors in ESPHome.
    image: wts01.png
---



The `wts01` platform allows you to use WTS01 temperature sensors with ESPHome.
This is the sensor used in Sonoff TH Origin (THR316, THR320) and TH Elite (THR316D, THR320D) devices.

For this component to work you need to have set up a UART bus in your configuration - only the RX pin should be necessary.

The sensor communicates with the microcontroller via {{< docref "/components/uart" "UART" >}}.

{{< img src="wts01-full.png" alt="Image" width="80.0%" class="align-center" >}}

## Basic configuration

```yaml
# You need to have a UART bus setup in your configuration
uart:
  rx_pin: GPIOXX
  baud_rate: 9600

# Then you can add the WTS01 sensor
sensor:
  - platform: wts01
    name: "WTS01 Temperature"

```

## Configuration variables

- All options from [Sensor](/components/sensor).

> [!NOTE]
> The WTS01 sensor is used in Sonoff TH Origin (THR316, THR320) and TH Elite (THR316D, THR320D) devices and connects to the main device using a RJ9 4C4P connector.
> This sensor provides temperature readings with 0.1°C resolution.

## See Also

- [Sensor Filters](/components/sensor/#sensor-filters)
- {{< docref "/components/uart" >}}
- {{< apiref "wts01/wts01.h" "wts01/wts01.h" >}}
