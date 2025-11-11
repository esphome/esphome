---
description: "Instructions for setting up generic output locks in ESPHome that control an output component."
title: "Generic Output Lock"
params:
  seo:
    description: Instructions for setting up generic output locks in ESPHome that control an output component.
    image: upload.svg
---

The `output` lock platform allows you to use any output component as a lock.

{{< img src="output-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
output:
  - platform: gpio
    pin: GPIOXX
    id: 'generic_out'
lock:
  - platform: output
    name: "Generic Output"
    output: 'generic_out'
```

## Configuration variables

- **output** (**Required**, [ID](/guides/configuration-types#id)): The ID of the output component to use.
- All other options from [Lock](/components/lock#config-lock).

## See Also

- {{< docref "/components/output" >}}
- {{< apiref "output/lock/output_lock.h" "output/lock/output_lock.h" >}}
