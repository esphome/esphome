---
description: "Instructions for setting up status LEDs in ESPHome to monitor the status of an ESP."
title: "Status LED"
params:
  seo:
    description: Instructions for setting up status LEDs in ESPHome to monitor the status of an ESP.
    image: led-on.svg
---

The `status_led` hooks into all ESPHome components and can indicate the status of
the device. Specifically, it will:

- Blink slowly (about every second) when a **warning** is active. Warnings are active when for
  example reading a sensor value fails temporarily, the WiFi/MQTT connections are disrupted, or
  if the native API component is included but no client is connected.

- Blink quickly (multiple times per second) when an **error** is active. Errors indicate that
  ESPHome has found an error while setting up. In most cases, ESPHome will still try to
  recover from the error and continue with all other operations.

- Stay off otherwise.

```yaml
# Example configuration entry
status_led:
  pin: GPIOXX
```

> [!NOTE]
> If your device has a single LED that needs to be shared use
> {{< docref "/components/light/status_led" "status_led light platform" >}} instead.

## Configuration variables

- **pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The
  GPIO pin to operate the status LED on.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

> [!NOTE]
> If your LED is in an active-LOW mode (when it's on if the output is enabled), use the
> `inverted` option of the [Pin Schema](/guides/configuration-types#pin-schema):
>
> ```yaml
> status_led:
>   pin:
>     number: GPIOXX
>     inverted: true
> ```

## See Also

- {{< docref "/components/light/status_led" >}}
- {{< apiref "status_led/status_led.h" "status_led/status_led.h" >}}
