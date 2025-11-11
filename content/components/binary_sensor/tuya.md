---
description: "Instructions for setting up a Tuya device binary sensor."
title: "Tuya Binary Sensor"
params:
  seo:
    description: Instructions for setting up a Tuya device binary sensor.
---

The `tuya` binary sensor platform creates a binary sensor from a
tuya component and requires {{< docref "/components/tuya" >}} to be configured.

You can create the binary sensor as follows:

```yaml
# Create a binary sensor
binary_sensor:
  - platform: "tuya"
    name: "MyBinarySensor"
    sensor_datapoint: 1
```

## Configuration variables

- **sensor_datapoint** (**Required**, int): The datapoint id number of the binary sensor.
- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## See Also

- {{< docref "/components/tuya" >}}
- {{< docref "/components/binary_sensor" >}}
- {{< apiref "tuya/binary_sensor/tuya_binary_sensor.h" "tuya/binary_sensor/tuya_binary_sensor.h" >}}
