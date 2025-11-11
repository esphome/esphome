---
description: "Instructions for setting up a sensor that tracks the uptime of the ESP."
title: "Uptime Sensor"
params:
  seo:
    description: Instructions for setting up a sensor that tracks the uptime of the ESP.
    image: timer.svg
---

The `uptime` sensor allows you to track the time the ESP has stayed up for in seconds.
Time rollovers are automatically handled.

```yaml
# Example configuration entry
sensor:
  - platform: uptime
    type: seconds
    name: Uptime Sensor
```

## Configuration variables

- **type** (*Optional*): Either:

  - `seconds` (*default*): A simple counter.
  - `timestamp`  : presents the time ESPHome last booted up. Requires a {{< docref "/components/time" >}}.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The sensor reporting interval. Defaults to `60s`.
  Valid only with `type: seconds`.

- All other options from [Sensor](/components/sensor).

## See Also

- {{< docref "/components/text_sensor/uptime" >}}
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "uptime/uptime_sensor.h" "uptime/uptime_sensor.h" >}}
