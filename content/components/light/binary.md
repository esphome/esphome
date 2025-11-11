---
description: "Instructions for setting up binary ON/OFF lights in ESPHome."
title: "Binary Light"
params:
  seo:
    description: Instructions for setting up binary ON/OFF lights in ESPHome.
    image: lightbulb.svg
---

The `binary` light platform creates a simple ON/OFF-only light from a
[binary output component](/components/output#output).

{{< img src="binary-ui.png" alt="Image" width="40.0%" class="align-center" >}}

```yaml
# Example configuration entry
light:
  - platform: binary
    name: "Desk Lamp"
    output: light_output
```

## Configuration variables

- **output** (**Required**, [ID](/guides/configuration-types#id)): The id of the binary [Output Component](/components/output#output) to use for this light.
- All other options from [Light](/components/light#config-light).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/light" >}}
- {{< docref "/components/output/gpio" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "binary/light/binary_light_output.h" "binary/light/binary_light_output.h" >}}
