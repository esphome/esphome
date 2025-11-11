---
description: "Instructions for setting up Hydreon rain sensors"
title: "Hydreon Rain Sensor Binary Sensor"
params:
  seo:
    description: Instructions for setting up Hydreon rain sensors
    image: hydreon_rg9.jpg
---

The `hydreon_rgxx` binary sensor platform gives access to information provided by a Hydreon Rain Sensor.
For this sensor to work, a {{< docref "/components/sensor/hydreon_rgxx" >}} must be set up.

```yaml
# Example RG-9 entry
sensor:
  - platform: hydreon_rgxx
    model: "RG_9"
    id: "hydreon_1"
    update_interval: 1s
    moisture:
      name: "rain"
      expire_after: 30s

binary_sensor:
  - platform: hydreon_rgxx
    hydreon_rgxx_id: "hydreon_1"
    too_cold:
      name: "too cold"
```

## Configuration variables

- **hydreon_rgxx_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the Hydreon Rain Sensor display.

- **too_cold** (*Optional*): `true` if the sensor reports being too cold. Hydreon only mentions this feature for the RG-9.

  - All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **lens_bad** (*Optional*): `true` if the sensor reports the lens being bad.

  - All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **em_sat** (*Optional*): `true` if the sensor reports the Emitter being saturated.

  - All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## See Also

- {{< docref "/components/sensor/hydreon_rgxx" >}}
- {{< docref "index/" >}}
