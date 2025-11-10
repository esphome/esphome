---
description: "Instructions for setting up a Xiaomi MiFlora HHCCJCY10 (Pink) using ESPHome."
title: "HHCCJCY10 Xiaomi MiFlora (Pink version)"
params:
  seo:
    description: Instructions for setting up a Xiaomi MiFlora HHCCJCY10 (Pink) using ESPHome.
    image: xiaomi_hhccjcy10.jpg
---

MiFlora, tuya (pink) version, measures temperature, moisture, ambient light and nutrient levels in the soil.

{{< img src="xiaomi_hhccjcy10.jpg" alt="Image" width="60.0%" class="align-center" >}}

```yaml
sensor:
  - platform: xiaomi_hhccjcy10
    mac_address: XX:XX:XX:XX:XX:XX
    temperature:
      name: "Xiaomi HHCCJCY10 Temperature"
    moisture:
      name: "Xiaomi HHCCJCY10 Moisture"
    illuminance:
      name: "Xiaomi HHCCJCY10 Illuminance"
    conductivity:
      name: "Xiaomi HHCCJCY10 Soil Conductivity"
    battery_level:
      name: "Xiaomi HHCCJCY10 Battery Level"
```

## Configuration variables

- **mac_address** (**Required**, string): The MAC address of the device.
- **temperature** (*Optional*): The temperature sensor.
  All options from [Sensor](/components/sensor).

- **moisture** (*Optional*): The moisture sensor.
  All options from [Sensor](/components/sensor).

- **illuminance** (*Optional*): The illuminance sensor.
  All options from [Sensor](/components/sensor).

- **conductivity** (*Optional*): The conductivity sensor.
  All options from [Sensor](/components/sensor).

- **battery_level** (*Optional*): The battery level sensor.
  All options from [Sensor](/components/sensor).

## See Also

- {{< docref "ble_client/" >}}
