---
description: "Instructions for setting up Dallas 1-Wire temperature sensors"
title: "Dallas Temperature Sensor"
params:
  seo:
    description: Instructions for setting up Dallas 1-Wire temperature sensors
    image: dallas.jpg
---

The `dallas_temp` component allows you to use
[DS18B20](https://www.adafruit.com/product/374)
([datasheet](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf))
and similar 1-Wire temperature sensors. A {{< docref "/components/one_wire/index" "1-Wire bus" >}} is
required to be set up in your configuration for this sensor to work.

```yaml
# Example configuration entries
sensor:
  # only one device
  - platform: dallas_temp
    name: temperature
    update_interval: 120s
  # specific address
  - platform: dallas_temp
    address: 0x1234567812345628
    name: temperature2
  # second device
  - platform: dallas_temp
    index: 1
    name: temperature1
```

## Configuration variables

- **address** (*Optional*, int): The address of the sensor. Required if there is more than one device on the bus and index is not specified.
- **index** (*Optional*, byte): The index (0-based) of the sensor. Required if there is more than one device on the bus and address is not specified.
  *Note this index is based on the hardware addresses of the sensors and the order can change if sensors are changed, added, or removed.*
- **resolution** (*Optional*, int): An optional resolution from 9 to 12. Higher means more accurate.
  Defaults to the maximum for most Dallas temperature sensors: 12.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval that the sensors should be checked.
  Defaults to 60 seconds.

- **one_wire_id** (*Optional*, {{< docref "/components/one_wire" >}}): The ID of the 1-Wire bus to use.
  Required if there is more than one bus.

- All other options from [Sensor](/components/sensor).

### See Also

- [Arduino DallasTemperature library](https://github.com/milesburton/Arduino-Temperature-Control-Library)
  by [Miles Burton](https://github.com/milesburton)

- {{< apiref "dallas_temp/dallas_temp.h" "dallas_temp/dallas_temp.h" >}}
