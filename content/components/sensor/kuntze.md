---
description: "Instructions for setting up Kuntze pool monitor in ESPHome."
title: "Kuntze pool monitor"
params:
  seo:
    description: Instructions for setting up Kuntze pool monitor in ESPHome.
    image: kuntze.jpg
---

The `kuntze` component allows you to integrate the Kuntze water measurement
instrument in ESPHome. It uses [UART](/components/uart) (ModBUS) for communication.

Once configured you can use sensors as described below for your projects.

{{< img src="kuntze.jpg" alt="Image" caption="Kuntze Neon® Multi instrument" class="align-center" >}}

## Overview

Kuntze devices have an RS485 (ModBUS RTU) communication port. Please see the
Kuntze papers for the pinout of the RS485 connector on your unit. ModBUS line
has to be terminated properly (with a `120Ω` resistor), and since this is likely
your only unit connected to ESPHome, you should activate bus termination in the
Network menu (this component doesn't support multiple Kuntze devices on the same
bus). ModBUS address should remain at factory default value.

The device communicates at `19200` baud `8E1`. To connect to ESPHome, an RS485
transceiver is needed. Choose a type which does not need a trigger to send and
receive data, for example:

{{< img src="rs485.jpg" alt="Image" >}}

The controller connects to the UART of the MCU. For ESP32 GPIO `16` to `TXD` and `17`
to RXD are the default ones but any other pins can be used as well. 3.3V to VCC and GND to GND.

> [!WARNING]
> If you are using the [Logger Component](/components/logger#logger) make sure you are not using the same pins for it or otherwise disable the UART
> logging with the `baud_rate: 0` option.

## Component

A configured modbus component is optional. It will be automatically created.

```yaml
# Example configuration entry

sensor:
  - platform: kuntze
    id: my_kuntze
    ph:
      id: ph
    temperature:
      id: temperature
```

Configuration variables:

- **ph** (*Optional*): Measured pH value.
- **temperature** (*Optional*): Measured temperature value.
- **dis1** (*Optional*): Measured DIS 1 value.
- **dis2** (*Optional*): Measured DIS 2 value.
- **redox** (*Optional*): Measured Redox value.
- **ec** (*Optional*): Measured EC value.
- **oci** (*Optional*): Measured OCI value.

All sensors are *Optional* and support all other options from [Sensor](/components/sensor).

## See Also

- [UART Bus](/components/uart)
- [Logger Component](/components/logger#logger)
- [Sensor](/components/sensor)
- [Kuntze manuals](https://www.kuntze.com/en/downloads-2/)
- [Communication protocol](https://www.kuntze.com/wp-content/uploads/2021/05/2019_Manual_Modbus-RTU_ENG.pdf)
