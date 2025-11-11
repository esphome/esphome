---
description: "Instructions for setting up speed-controllable fans."
title: "Speed Fan"
params:
  seo:
    description: Instructions for setting up speed-controllable fans.
    image: fan.svg
---

The `speed` fan platform lets you represent any float [Output Component](/components/output#output) as a fan that
supports speed settings.

{{< img src="fan-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
fan:
  - platform: speed
    output: my_output_1
    name: "Living Room Fan"
```

## Configuration variables

- **output** (**Required**, [ID](/guides/configuration-types#id)): The id of the [float output](/components/output#output) to use for this fan.
- **oscillation_output** (*Optional*, [ID](/guides/configuration-types#id)): The id of the
  [output](/components/output#output) to use for the oscillation state of this fan. Default is empty.

- **direction_output** (*Optional*, [ID](/guides/configuration-types#id)): The id of the
  [output](/components/output#output) to use for the direction state of the fan. Default is empty.

- **speed_count** (*Optional*, int): Set the number of supported discrete speed levels. The value is used
  to calculate the percentages for each speed. E.g. `2` means that you have 50% and 100% while `100`
  will allow 1% increments in the output. Defaults to `100`.

- **preset_modes** (*Optional*): A list of preset modes for this fan. Preset modes can be used in automations (i.e. `on_preset_set`  ).
- All other options from [Fan Component](/components/fan#config-fan).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/fan" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/pca9685" >}}
- {{< apiref "fan/fan_state.h" "fan/fan_state.h" >}}
