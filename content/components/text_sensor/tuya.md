---
description: "Instructions for setting up a Tuya device sensor."
title: "Tuya Text Sensor"
params:
  seo:
    description: Instructions for setting up a Tuya device sensor.
    image: tuya.png
---

The `tuya` text sensor platform creates a sensor from a tuya component
and requires {{< docref "/components/tuya" >}} to be configured.

You can create the text sensor as follows:

```yaml
# Create a sensor
text_sensor:
  - platform: "tuya"
    name: "MyTextSensor"
    sensor_datapoint: 18
```

## Configuration variables

- **sensor_datapoint** (**Required**, int): The datapoint id number of the sensor.
- All other options from [Text Sensor](/components/text_sensor#config-text_sensor).

## See Also

- {{< docref "/components/tuya" >}}
- {{< docref "/components/text_sensor" >}}
- {{< apiref "tuya/text_sensor/tuya_text_sensor.h" "tuya/text_sensor/tuya_text_sensor.h" >}}
