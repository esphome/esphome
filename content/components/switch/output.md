---
description: "Instructions for setting up generic output switches in ESPHome that control an output component."
title: "Generic Output Switch"
params:
  seo:
    description: Instructions for setting up generic output switches in ESPHome that control an output component.
    image: upload.svg
---

The `output` switch platform allows you to use any output component as a switch.

{{< img src="output-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
output:
  - platform: gpio
    pin: GPIOXX
    id: 'generic_out'
switch:
  - platform: output
    name: "Generic Output"
    output: 'generic_out'
```

## Configuration variables

- **output** (**Required**, [ID](/guides/configuration-types#id)): The ID of the output component to use.
- All other options from [Switch](/components/switch#config-switch).

## See Also

- {{< docref "/components/output" >}}
- {{< apiref "output/switch/output_switch.h" "output/switch/output_switch.h" >}}
