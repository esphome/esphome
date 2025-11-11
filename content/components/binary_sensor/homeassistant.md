---
description: "Instructions for setting up Home Assistant binary sensors with ESPHome that import states from your Home Assistant instance."
title: "Home Assistant Binary Sensor"
params:
  seo:
    description: Instructions for setting up Home Assistant binary sensors with ESPHome that import states from your Home Assistant instance.
    image: home-assistant.svg
---

The `homeassistant` binary sensor platform allows you to create binary sensors that **import**
states from your Home Assistant instance using the {{< docref "/components/api" "native API" >}}.

> [!NOTE]
> Although you might not plan to *export* states from the node and you do not need an entity of the node
> in Home Assistant, this component still requires you to register the node under Home Assistant. See:
> [Connecting your device to Home Assistant](/guides/getting_started_hassio#connecting-your-device-to-home-assistant).

```yaml
# Example configuration entry
binary_sensor:
  - platform: homeassistant
    name: "Input Boolean From Home Assistant"
    entity_id: input_boolean.state_home
```

With Home Assistant 2021.6 or newer, entity state attributes can also be imported.

```yaml
# Example configuration entry
binary_sensor:
  - platform: homeassistant
    id: muted
    entity_id: media_player.mega_speakers
    attribute: is_volume_muted
```

## Configuration variables

- **entity_id** (**Required**, string): The entity ID to import from Home Assistant.
- **attribute** (*Optional*, string): The name of the state attribute to import from the
  specified entity. The entity state is used when this option is omitted.

- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## See Also

- [Automation](/automations)
- {{< apiref "homeassistant/binary_sensor/homeassistant_binary_sensor.h" "homeassistant/binary_sensor/homeassistant_binary_sensor.h" >}}
