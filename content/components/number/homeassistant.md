---
description: "Instructions for setting up Home Assistant numbers with ESPHome."
title: "Home Assistant Number"
params:
  seo:
    description: Instructions for setting up Home Assistant numbers with ESPHome.
    image: description.svg
---

The `homeassistant` number platform allows you to create a number that is synchronized
with Home Assistant. Min, Max and Step are not configurable for this platform because they are taken from the Home Assistant entity.

> [!NOTE]
> Although you might not plan to *export* states from the node and you do not need an entity of the node
> in Home Assistant, this component still requires you to register the node under Home Assistant. See:
> [Connecting your device to Home Assistant](/guides/getting_started_hassio#connecting-your-device-to-home-assistant).

```yaml
# Example configuration entry
number:
  - platform: homeassistant
    id: my_ha_number
    entity_id: number.my_number
```

## Configuration variables

- **entity_id** (**Required**, string): The Home Assistant entity ID of the number to synchronize with.
- All other options from [Number](/components/number#config-number).

## `number.set` Action

You can also set the number for the Home Assistant number from elsewhere in your YAML file
with the [`number.set` Action](/components/number#number-set_action).

## See Also

- [Automation](/automations)
- {{< apiref "homeassistant/number/homeassistant_number.h" "homeassistant/number/homeassistant_number.h" >}}
