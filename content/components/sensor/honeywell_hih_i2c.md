---
description: "Instructions for setting up Honeywell HumidIcon temperature and humidity sensors."
title: "Honeywell HumidIcon (I2C HIH series) Temperature & Humidity Sensor"
params:
  seo:
    description: Instructions for setting up Honeywell HumidIcon temperature and humidity sensors.
    image: honeywellhih.jpg
---

Honeywell HumidIcon (I2C HIH series) Temperature & Humidity sensors with ESPHome
([datasheet](https://prod-edam.honeywell.com/content/dam/honeywell-edam/sps/siot/en-us/products/sensors/humidity-with-temperature-sensors/common/documents/sps-siot-humidity-sensors-line-guide-009034-7-en-ciid-54931.pdf?download=false)).
The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

Example sensors:

```yaml
# Example configuration entry
sensor:
  - platform: honeywell_hih_i2c
    temperature:
      name: "Living Room Temperature"
    humidity:
      name: "Living Room Humidity"
```

## Configuration variables

- **temperature** (**Required**): The information for the temperature sensor.
  All options from [Sensor](/components/sensor).

- **humidity** (**Required**): The information for the humidity sensor.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "honeywell_hih_i2c/honeywell_hih.h" "honeywell_hih_i2c/honeywell_hih.h" >}}
