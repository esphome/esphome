---
description: "Instructions for setting up the integrated temperature sensor of the ESP32, RP2040 and BK72XX."
title: "Internal Temperature Sensor"
params:
  seo:
    description: Instructions for setting up the integrated temperature sensor of the ESP32, RP2040 and BK72XX.
    image: thermometer.svg
---

The `internal_temperature` sensor platform allows you to use the integrated
temperature sensor of the ESP32, RP2040 and BK72XX chip.

> [!NOTE]
> Some ESP32 variants return a large amount of invalid temperature
> values, including 53.3°C which equates to a raw value of 128. Invalid measurements are ignored by this component.

{{< img src="internal_temperature-ui.png" alt="Image" width="70.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: internal_temperature
    name: "Internal Temperature"
```

## Configuration variables

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval
  to check the sensor. Defaults to `60s`.

- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
