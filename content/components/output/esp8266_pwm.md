---
description: "Instructions for setting up ESP8266 software-based PWMs."
title: "ESP8266 Software PWM Output"
params:
  seo:
    description: Instructions for setting up ESP8266 software-based PWMs.
    image: pwm.png
---

The ESP8266 Software PWM platform allows you to use a software PWM on
the pins GPIO0-GPIO16 on your ESP8266. Note that this is a software PWM,
so there can be some flickering during periods of high WiFi activity. Hardware PWMs
like the one on the ESP32 (see {{< docref "ledc/" >}}) are preferred.

```yaml
# Example configuration entry
output:
  - platform: esp8266_pwm
    pin: GPIOXX
    frequency: 1000 Hz
    id: pwm_output

# Example usage in a light
light:
  - platform: monochromatic
    output: pwm_output
    name: "Kitchen Light"
```

## Configuration variables

- **pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to use PWM on.
- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this output component.
- **frequency** (*Optional*, frequency): The frequency to run the PWM with. Lower frequencies
  have more visual artifacts, but can represent much more colors. Defaults to `1000 Hz`.

- All other options from [Output](/components/output#config-output).

> [!NOTE]
> If you previously had Tasmota installed on your device and have just flashed ESPHome onto it,
> you may encounter an issue where the PWM output is only fully on or off.
>
> A hard reset fixes the problem - if you have this issue please power cycle the device, that
> should fix it.

{{< anchor "output-esp8266_pwm-set_frequency_action" >}}

## `output.esp8266_pwm.set_frequency` Action

This [Action](/automations/actions#all-actions) allows you to manually change the frequency of an ESP8266 PWM
channel at runtime. Use cases include controlling a passive buzzer (for pitch control).

```yaml
on_...:
  - output.esp8266_pwm.set_frequency:
      id: pwm_output
      frequency: 100Hz
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the PWM output to change.
- **frequency** (**Required**, [templatable](/automations/templates), float): The frequency
  to set in hertz.

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< docref "/components/fan/speed" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "esp8266_pwm/esp8266_pwm.h" "esp8266_pwm/esp8266_pwm.h" >}}
