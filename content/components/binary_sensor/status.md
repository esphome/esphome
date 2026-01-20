---
description: "Instructions for setting up status binary sensors."
title: "Status Binary Sensor"
params:
  seo:
    description: Instructions for setting up status binary sensors.
    image: server-network.svg
---

The Status Binary Sensor exposes the node state (if it's connected to via MQTT/native API)
for Home Assistant.

{{< img src="status-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
binary_sensor:
  - platform: status
    name: "Living Room Status"
```

## Configuration variables

- **update_interval** (*Optional*, {{< docref "/guides/configuration-types#time" "Time" >}}): The interval
  to check the connection status. Defaults to `1s`.
- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor). (Inverted mode is not supported)

## See Also

- {{< docref "/components/binary_sensor" >}}
- {{< docref "/components/mqtt" >}}
- {{< apiref "status/status_binary_sensor.h" "status/status_binary_sensor.h" >}}
