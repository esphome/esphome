---
description: "Instructions for setting up STTS22H temperature sensor from STMicroelectronics"
title: "STTS22H Temperature Sensor"
params:
  seo:
    description: Instructions for setting up STTS22H temperature sensor from STMicroelectronics with ESPHome
    image: stts22h.png
---

The `stts22h` sensor platform allows you to use a STTS22H temperature sensor
([datasheet](https://www.st.com/resource/en/datasheet/stts22h.pdf)) with ESPHome. This is a low-power,
high-accuracy digital temperature sensor with ±0.5°C accuracy (typical at 25°C) and a temperature range
from -40°C to +125°C.

The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

{{< img src="stts22h.png" alt="STTS22H Temperature Sensor"
caption="SparkFun STTS22H Temperature Sensor Breakout Boards" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: stts22h
    name: "STTS22H Temperature"
    address: 0x3C
    update_interval: 60s
```

## Configuration variables

- **i2c_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the
  [I²C Bus](/components/i2c) if you want to use multiple I²C buses.

- **address** (*Optional*, int): The I²C address of the sensor. Defaults to
  `0x3C`.
  The address is determined by the ADDR pin configuration on the sensor.
  Possible addresses: `0x38`, `0x3C`, `0x3E`, `0x3F`.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check
  the sensor. Defaults to `60s`.

- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- [I²C Bus](/components/i2c)
- [STTS22H Datasheet (PDF)](https://www.st.com/resource/en/datasheet/stts22h.pdf)
- [STTS22H Product Overview - STMicroelectronics](https://www.st.com/en/mems-and-sensors/stts22h.html)
