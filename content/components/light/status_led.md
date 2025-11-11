---
description: "Instructions for setting up a Status LED shared also as binary ON/OFF light in ESPHome."
title: "Status LED Light"
params:
  seo:
    description: Instructions for setting up a Status LED shared also as binary ON/OFF light in ESPHome.
    image: led-on.svg
---

The `status_led` light platform allows to share a single LED for indicating the status of
the device (when on error/warning state) or as binary light (when on OK state).
This is useful for devices with only one LED available.
You can also use a binary [Output Component](/components/output#output).

It provides the combined functionality of {{< docref "/components/status_led" "status_led component" >}} and a
{{< docref "/components/light/binary" "binary light component" >}} over a single shared GPIO led.

When the device is on error/warning state, the function of `status_led` will take precedence and control the blinking of the LED.
When the device is in OK state, the LED will be restored to the state of the `binary light` function and can be controlled as such.

```yaml
# Example configuration entry
light:
  - platform: status_led
    name: "Switch state"
    pin: GPIOXX
```

> [!NOTE]
> When using this platform the high level `status_led` component should not be included (at least over the same pin),
> as its functionality is directly provided by this platform.
>
> The only difference is that the platform won't be loaded in OTA safe mode, while the component would be.

## Configuration variables

- **pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin to operate the LED on.
- **output** (*Optional*, [ID](/guides/configuration-types#id)): The id of the binary [Output Component](/components/output#output) to use for this light.
- All other options from [Light](/components/light#config-light).

> [!NOTE]
> If your Status LED is in an active-LOW mode (such as with the D1 Mini ESP8266 boards), use the
> `inverted` option of the [Pin Schema](/guides/configuration-types#pin-schema):
>
> ```yaml
> pin:
>   number: GPIOXX
>   inverted: true
> ```

## See Also

- {{< docref "/components/status_led" >}}
- {{< docref "/components/light/binary" >}}
- {{< docref "/components/light" >}}
- {{< apiref "status_led/light/status_led_light.h" "status_led/light/status_led_light.h" >}}
