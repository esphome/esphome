---
description: "Instructions for setting up switch binary sensors with ESPHome."
title: "Switch Binary Sensor"
params:
  seo:
    description: Instructions for setting up switch binary sensors with ESPHome.
---

{{< anchor "switch-binary-sensor" >}}

The Switch Binary Sensor platform allows you to view the state of any switch component as a
read-only binary sensor.

```yaml
# Example configuration entry
binary_sensor:
  - platform: switch
    name: "Output state"
    source_id: relay1

switch:
  - platform: gpio
    id: relay1
    pin: GPIOXX
```

## Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The source switch to observe.
- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## See Also

- {{< docref "/components/binary_sensor" >}}
- {{< apiref "switch/binary_sensor/switch_binary_sensor.h" "switch/binary_sensor/switch_binary_sensor.h" >}}
