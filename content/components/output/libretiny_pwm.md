---
description: "Instructions for setting up LibreTiny hardware PWMs."
title: "LibreTiny PWM Output"
params:
  seo:
    description: Instructions for setting up LibreTiny hardware PWMs.
    image: pwm.png
---

The LibreTiny PWM platform allows you to use a hardware PWM on BK72xx and RTL87xx chips.
Refer to [LibreTiny/Boards](https://docs.libretiny.eu/link/boards) to find your board
and which PWM pins it supports.

```yaml
# Example configuration entry
output:
  - platform: libretiny_pwm
    pin: P8
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

{{< anchor "output-libretiny_pwm-set_frequency_action" >}}

## `output.libretiny_pwm.set_frequency` Action

This [Action](/automations/actions#all-actions) allows you to manually change the frequency of a LibreTiny PWM
channel at runtime. Use cases include controlling a passive buzzer (for pitch control).

```yaml
on_...:
  - output.libretiny_pwm.set_frequency:
      id: pwm_output
      frequency: 100Hz
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the PWM output to change.
- **frequency** (**Required**, [templatable](/automations/templates), float): The frequency
  to set in hertz.

## See Also

- {{< docref "/components/libretiny" >}}
- {{< docref "/components/output" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< docref "/components/fan/speed" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "libretiny_pwm/libretiny_pwm.h" "libretiny_pwm/libretiny_pwm.h" >}}
