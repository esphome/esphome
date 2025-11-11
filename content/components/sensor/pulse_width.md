---
description: "Instructions for setting up pulse width sensors in ESPHome"
title: "Pulse Width Sensor"
params:
  seo:
    description: Instructions for setting up pulse width sensors in ESPHome
    image: pulse.svg
---

The `pulse_width` sensor allows you to measure how long a given digital signal
is HIGH. For example this can be used to measure PWM signals to transmit some
value over a simple protocol. The unit of measurement for this sensor is seconds.

> [!NOTE]
> This component is intended for measurements in the microsecond to seconds range!
> The largest period this component can measure is just over 70 minutes.

```yaml
# Example configuration entry
sensor:
  - platform: pulse_width
    pin: D0
    name: Pulse Width Sensor
```

## Configuration variables

- **pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to observe for the
  pulse width.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor.
  Defaults to `60s`.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "pulse_width/pulse_width.h" "pulse_width/pulse_width.h" >}}
