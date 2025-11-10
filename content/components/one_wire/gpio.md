---
description: "Instructions for setting up a Dallas (Analog Devices) 1-Wire bus via a GPIO pin to communicate with 1-wire devices in ESPHome"
title: "1-Wire Bus via GPIO"
params:
  seo:
    description: Instructions for setting up a Dallas (Analog Devices) 1-Wire bus via a GPIO pin to communicate with 1-wire devices in ESPHome
    image: one-wire.svg
---

The `gpio` platform uses the CPU to generate the bus signals on an on-board GPIO pin.

```yaml
# Example configuration entry
one_wire:
  - platform: gpio
    pin: GPIOXX
```

## Configuration variables

- **pin** (**Required**, number): The pin which will be use for bus communication. Note that 1-wire is a bi-directional
  bus so the selected GPIO pin must support both input and output. This must be a GPIO pin internal to the
  microcontroller and cannot be a pin located on an I/O expander or similar device.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation. Required if you have
  multiple busses.

### See Also

- {{< docref "index/" >}}
- {{< apiref "one_wire/one_wire_bus.h" "one_wire/one_wire_bus.h" >}}

- [Guidelines for Reliable Long Line 1-Wire Networks](https://www.analog.com/en/technical-articles/guidelines-for-reliable-long-line-1wire-networks.html)
