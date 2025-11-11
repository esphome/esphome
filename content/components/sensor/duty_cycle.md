---
description: "Instructions for setting up duty cycle sensors in ESPHome"
title: "Duty Cycle Sensor"
params:
  seo:
    description: Instructions for setting up duty cycle sensors in ESPHome
    image: percent.svg
---

The duty cycle sensor allows you to measure for what percentage of time a signal
on a GPIO pin is HIGH or LOW.

For example, you can measure if a status LED of a pool controller is permanently active
(indicating that the pump is on) or blinking.

{{< img src="duty_cycle-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: duty_cycle
    pin: D0
    name: Duty Cycle Sensor
```

## Configuration variables

- **pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to observe for the duty
  cycle.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "duty_cycle/duty_cycle_sensor.h" "duty_cycle/duty_cycle_sensor.h" >}}
