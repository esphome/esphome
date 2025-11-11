---
description: "Instructions for setting up binary fans."
title: "Binary Fan"
params:
  seo:
    description: Instructions for setting up binary fans.
    image: fan.svg
---

The `binary` fan platform lets you represent any binary [Output Component](/components/output#output) as a fan.

{{< img src="fan-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
fan:
  - platform: binary
    output: fan_output
    name: "Living Room Fan"
```

## Configuration variables

- **output** (**Required**, [ID](/guides/configuration-types#id)): The id of the
  binary output component to use for this fan.

- **oscillation_output** (*Optional*, [ID](/guides/configuration-types#id)): The id of the
  [output](/components/output#output) to use for the oscillation state of this fan. Default is empty.

- **direction_output** (*Optional*, [ID](/guides/configuration-types#id)): The id of the
  [output](/components/output#output) to use for the direction state of the fan. Default is empty.

- All other options from [Fan Component](/components/fan#config-fan).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/output/gpio" >}}
- {{< docref "/components/fan" >}}
- {{< apiref "fan/fan_state.h" "fan/fan_state.h" >}}
