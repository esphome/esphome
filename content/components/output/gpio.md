---
description: "Instructions for setting up binary outputs for GPIO pins."
title: "GPIO Output"
params:
  seo:
    description: Instructions for setting up binary outputs for GPIO pins.
    image: gpio.svg
---

The GPIO output component is quite simple: It exposes a single GPIO pin
as an output component. Note that output components are **not** switches and
will not show up in Home Assistant. See {{< docref "/components/switch/gpio" >}}.

```yaml
# Example configuration entry
output:
  - platform: gpio
    pin: GPIOXX
    id: gpio_d1
```

## Configuration variables

- **pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to turn on and off.
- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this output component.
- All other options from [Output](/components/output#config-output).

> [!WARNING]
> This is an **output component** and will not be visible from the frontend. Output components are intermediary
> components that can be attached to for example lights. To have a GPIO pin in the Home Assistant frontend, please
> see the {{< docref "/components/switch/gpio" >}}.

## See Also

- {{< docref "/components/switch/gpio" >}}
- {{< docref "/components/output" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/light/binary" >}}
- {{< docref "/components/fan/binary" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "gpio/output/gpio_binary_output.h" "gpio/output/gpio_binary_output.h" >}}
