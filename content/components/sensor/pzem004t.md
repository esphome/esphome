---
description: "Instructions for setting up PZEM-004 and PZEM-004T V1 power monitors."
title: "Peacefair PZEM-004T Energy Monitor"
params:
  seo:
    description: Instructions for setting up PZEM-004 and PZEM-004T V1 power monitors.
    image: pzem004t.svg
---

> [!NOTE]
> This page is incomplete and could use some work. If you want to contribute, please see our
> [developer site](https://developers.esphome.io). This page is missing:
>
> - An image for the front page.
> - Images/screenshots/example configs of this device being used in action.

The `pzem004t` sensor platform allows you to use PZEM-004 (version with 8-segment screens, [website](https://www.instructables.com/Power-Peacefair-PZEM-004-ESP8266-Arduino-Nano/)) and PZEM-004T V1 energy monitors
([website](https://innovatorsguru.com/ac-digital-multifunction-meter-using-pzem-004t/)) sensors with
ESPHome.

> [!WARNING]
> This page refers to version V1 of the PZEM-004T, which has been out of stock for a while.
> The PZEM-004, however, is still working (and selling) with this protocol and does not use modbus.
> For using the newer V3 variant of this sensor please see {{< docref "pzemac" "pzemac" >}}.

The communication with this component is done via a [UART](/components/uart).
You must therefore have a `uart:` entry in your configuration with both the TX and RX pins set
to some pins on your board and the baud rate set to 9600.

```yaml
# Example configuration entry
uart:
  tx_pin: D1
  rx_pin: D2
  baud_rate: 9600

sensor:
  - platform: pzem004t
    current:
      name: "PZEM-004T Current"
    voltage:
      name: "PZEM-004T Voltage"
    power:
      name: "PZEM-004T Power"
    energy:
      name: "PZEM-004T Energy"
    update_interval: 60s
```

## Configuration variables

- **current** (*Optional*): Use the current value of the sensor in amperes. All options from
  [Sensor](/components/sensor).

- **power** (*Optional*): Use the (active) power value of the sensor in watts. All options from
  [Sensor](/components/sensor).

- **voltage** (*Optional*): Use the voltage value of the sensor in volts.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

## Hardware considerations

These devices have optocouplers on the UART port and the resistors mounted on the board have been designed to work with 5V devices.
ESPs need a [Level Shifter](https://www.adafruit.com/product/1875) to be compatible with TTL levels.

> [!NOTE]
> You need a IC level shifter and not a [Mosfet-based](https://www.sparkfun.com/products/12009) level shifter, because you need
> to power the optocoupler's LEDs without an additional resistor in the path.

If prefer, you could change the value of the optocoupler's resistors by following [this](https://gregwareblog.wordpress.com/2020/12/13/home-power-monitoring-pzem-004/) or [this](https://www.instructables.com/Power-Peacefair-PZEM-004-ESP8266-Arduino-Nano/) guide.

Your ESP shall be powered by an external power supply and cannot be connected to the PZEM for power.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "pzemac/" >}}
- {{< docref "pzemdc/" >}}
- {{< apiref "pzem004t/pzem004t.h" "pzem004t/pzem004t.h" >}}
