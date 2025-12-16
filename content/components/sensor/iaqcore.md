---
description: "Instructions for setting up the iAQ-Core sensor."
title: "AMS iAQ-Core Indoor Air Quality Sensor"
params:
  seo:
    description: Instructions for setting up the iAQ-Core sensor.
    image: iaqcore.jpg
---

The AMS iAQ-Core sensor allows you to use your
([datasheet](https://www.digikey.com/en/htmldatasheets/production/1723318/0/0/1/iaq-core))
sensors with ESPHome.

{{< img src="iaqcore.jpg" alt="Image" caption="AMS iAQ-Core Indoor Air Quality Sensor." width="30.0%" class="align-center" >}}

The iAQ-Core sensor module is used to measure VOC levels and provide CO2 equivalent and TVOC equivalent predictions. The data is available via I²C bus.

To use the sensor, first set up an [I²C Bus](/components/i2c) and connect the sensor to the specified pins.

```yaml
# Example configuration entry
sensor:
  - platform: iaqcore
    address: 0x5A
    update_interval: 60s
    co2:
        name: "iAQ Core CO2 Sensor"
    tvoc:
        name: "iAQ Core TVOC Sensor"
```

## Configuration variables

- **i2c_id** (*Optional*, ID): The id of the I²C Bus.
- **address** (*Optional*, int): Manually specify the I²C address of the sensor. Defaults to `0x5A`.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.
- **co2** (*Optional*): The configuration for the CO2 sensor. All options from
  [Sensor](/components/sensor).

- **tvoc** (*Optional*): The configuration for the TVOC sensor. All options from
  [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "/components/sensor" >}}
