---
description: "Instructions for setting up additional binary sensors for Haier climate devices."
title: "Haier Climate Binary Sensors"
params:
  seo:
    description: Instructions for setting up additional binary sensors for Haier climate devices.
    image: haier.svg
---

Additional sensors for Haier Climate device. **These sensors are supported only by the hOn protocol**.

{{< img src="haier-climate.jpg" alt="Image" width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
binary_sensor:
  - platform: haier
    haier_id: haier_ac
    compressor_status:
      name: Haier Outdoor Compressor Status
    defrost_status:
      name: Haier Defrost Status
    four_way_valve_status:
      name: Haier Four Way Valve Status
    indoor_electric_heating_status:
      name: Haier Indoor Electric Heating Status
    indoor_fan_status:
      name: Haier Indoor Fan Status
    outdoor_fan_status:
      name: Haier Outdoor Fan Status
```

## Configuration variables

- **haier_id** (**Required**, [ID](/guides/configuration-types#id)): The id of haier climate component
- **compressor_status** (*Optional*): A binary sensor that indicates Haier climate compressor activity.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **defrost_status** (*Optional*): A binary sensor that indicates defrost procedure activity.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **four_way_valve_status** (*Optional*): A binary sensor that indicates four way valve status.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **indoor_electric_heating_status** (*Optional*): A binary sensor that indicates electrical heating system activity.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **indoor_fan_status** (*Optional*): A binary sensor that indicates indoor fan activity.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

- **outdoor_fan_status** (*Optional*): A binary sensor that indicates outdoor fan activity.
  All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## See Also

- {{< docref "/components/climate/haier" "Haier Climate" >}}
