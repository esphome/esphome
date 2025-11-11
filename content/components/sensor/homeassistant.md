---
description: "Instructions for setting up Home Assistant sensors with ESPHome that import states from your Home Assistant instance."
title: "Home Assistant Sensor"
params:
  seo:
    description: Instructions for setting up Home Assistant sensors with ESPHome that import states from your Home Assistant instance.
    image: home-assistant.svg
---

The `homeassistant` sensor platform allows you to create sensors that import
states from your Home Assistant instance using the {{< docref "/components/api" "native API" >}}.

> [!NOTE]
> Although you might not plan to *export* states from the node and you do not need an entity of the node
> in Home Assistant, this component still requires you to register the node under Home Assistant. See:
> [Connecting your device to Home Assistant](/guides/getting_started_hassio#connecting-your-device-to-home-assistant).

```yaml
# Example configuration entry
sensor:
  - platform: homeassistant
    name: "Temperature Sensor From Home Assistant"
    entity_id: sensor.temperature_sensor
```

Entity state attributes can also be imported:

```yaml
# Example configuration entry
sensor:
  - platform: homeassistant
    id: current_temperature
    entity_id: climate.living_room
    attribute: current_temperature
```

> [!NOTE]
> This component is only for numeral states. If you want to import arbitrary text states
> from Home Assistant, use the {{< docref "/components/text_sensor/homeassistant" "Home Assistant Text Sensor" >}}.

## Configuration variables

- **entity_id** (**Required**, string): The entity ID to import from Home Assistant.
- **attribute** (*Optional*, string): The name of the state attribute to import from the
  specified entity. The entity state is used when this option is omitted.

- All other options from [Sensor](/components/sensor).

> [!NOTE]
> The sensors implemented by this component are by default `internal`, to avoid exporting them back to
> Home Assistant. Should you still want to do that (eg. because you use ESPHome's very efficient filters
> on them) you need to specifically configure `internal: false`. Also, `state_class`, `unit_of_measurement`
> are not inherited from the imported sensor so you need to set them manually.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- [Automation](/automations)
- {{< apiref "homeassistant/sensor/homeassistant_sensor.h" "homeassistant/sensor/homeassistant_sensor.h" >}}
