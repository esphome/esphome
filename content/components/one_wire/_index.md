---
description: "Instructions for setting up a Dallas (Analog Devices) 1-Wire bus to communicate with 1-wire devices in ESPHome"
title: "1-Wire Bus"
params:
  seo:
    description: Instructions for setting up a Dallas (Analog Devices) 1-Wire bus to communicate with 1-wire devices in ESPHome
    image: one-wire.svg
---

The `one_wire` component allows you to use supported 1-Wire devices in ESPHome.

## Hardware

The 1-Wire bus the devices are connected to should have an external pull-up resistor of about 4.7KΩ. A resistor of
*about* 4.7KΩ connected between `3.3V` and the 1-Wire bus's GPIO/data pin should suffice. Values ± 1KΩ will generally
work fine as well, provided you don't have unusually long wires.

## Platforms

- [**GPIO**](/components/one_wire/gpio/) Create a 1-Wire bus on a GPIO pin
- [**DS2484**](/components/one_wire/ds2484/) An I2C-to-1-Wire bridge device

## Obtaining Sensor IDs

To find device addresses, simply start the firmware on your device with a `one_wire` hub configured and observe the
log output. Note that you don't need to define the individual sensors just yet, as scanning will occur even with no
sensors configured.

Here's an example log:

{{< img src="dallas-log.png" alt="Image" >}}

## See Also

- {{< apiref "one_wire/one_wire_bus.h" "one_wire/one_wire_bus.h" >}}

- [Guidelines for Reliable Long Line 1-Wire Networks](https://www.analog.com/en/technical-articles/guidelines-for-reliable-long-line-1wire-networks.html)
