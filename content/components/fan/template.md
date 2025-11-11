---
description: "Instructions for setting up template fans."
title: "Template Fan"
params:
  seo:
    description: Instructions for setting up template fans.
    image: fan.svg
---

The `template` fan platform lets you create a fan interface using only triggers.

{{< img src="fan-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
fan:
  - platform: template
    name: "Virtual Fan"
    on_state:
      - do something
    on_speed_set:
      - do something
```

## Configuration variables

- **has_direction** (*Optional*, boolean): Indicates if there should be a control for direction. Default is `false`.
- **has_oscillating** (*Optional*, boolean): Indicates if there should be a control for oscillating. Default is `false`.
- **speed_count** (*Optional*, int): Set the number of supported discrete speed levels. Default is only on/off.
- **preset_modes** (*Optional*): A list of preset modes for this fan. Preset modes can be used in automations (i.e. `on_preset_set`  ).
- All other options from [Fan Component](/components/fan#config-fan).

## See Also

- {{< docref "/components/fan" >}}
- {{< apiref "template/fan/template_fan.h" "template/fan/template_fan.h" >}}
