---
description: "Instructions for setting up a Tuya device sensor."
title: "Tuya Sensor"
params:
  seo:
    description: Instructions for setting up a Tuya device sensor.
    image: tuya.png
---

The `tuya` sensor platform creates a sensor from a tuya component
and requires {{< docref "/components/tuya" >}} to be configured.

```text
[13:46:01][C][tuya:023]: Tuya:
[13:46:01][C][tuya:032]:   Datapoint 1: switch (value: OFF)
[13:46:01][C][tuya:032]:   Datapoint 2: switch (value: OFF)
[13:46:01][C][tuya:034]:   Datapoint 3: int value (value: 19)
[13:46:01][C][tuya:034]:   Datapoint 4: int value (value: 17)
[13:46:01][C][tuya:034]:   Datapoint 5: int value (value: 0)
[13:46:01][C][tuya:036]:   Datapoint 7: enum (value: 1)
[13:46:01][C][tuya:046]:   Product: '{"p":"ynjanlglr4qa6dxf","v":"1.0.0","m":0}'
```

On this controller, the datapoint 5 represents the countdown timer in minutes
which is what we are interested in reading using this platform.

Based on this, you can create the sensor as follows:

```yaml
# Create a sensor
sensor:
  - platform: "tuya"
    name: "MySensor"
    sensor_datapoint: 5
```

## Configuration variables

- **sensor_datapoint** (**Required**, int): The datapoint id number of the sensor.
- All other options from [Sensor](/components/sensor).

## See Also

- {{< docref "/components/tuya" >}}
- {{< docref "/components/sensor" >}}
- {{< apiref "tuya/sensor/tuya_sensor.h" "tuya/sensor/tuya_sensor.h" >}}
