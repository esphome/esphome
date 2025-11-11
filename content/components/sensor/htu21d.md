---
description: "Instructions for setting up HTU21D temperature and humidity sensors."
title: "HTU21D | Si7021 | SHT21 Temperature & Humidity Sensor"
params:
  seo:
    description: Instructions for setting up HTU21D temperature and humidity sensors.
    image: htu21d.jpg
---

The HTU21D Temperature & Humidity component allows you to use HTU21D, Si7021 and SHT21 sensors with
ESPHome. The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

Example sensors:

- ([Adafruit](https://www.adafruit.com/product/1899))

{{< img src="htu21d-full.jpg" alt="Image" caption="HTU21D Temperature & Humidity Sensor." width="50.0%" class="align-center" >}}

{{< img src="temperature-humidity.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: htu21d
    model: htu21d
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
    heater:
      name: "Heater"
```

## Configuration variables

- **temperature** (*Optional*): The information for the temperature sensor.
  All options from [Sensor](/components/sensor).

- **humidity** (*Optional*): The information for the humidity sensor.
  All options from [Sensor](/components/sensor).

- **heater** (*Optional*): The information for the heater sensor.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.

- **model** (*Optional*): Possible values are HTU21D, SI7021, SHT21. Some boards may be mislabeled. If you
  encounter I2C errors, try using HTU21D. Defaults to HTU21D.

The heater may be enabled to help correct the reading; see the datasheet for more information.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< docref "dht/" >}}
- {{< docref "dht12/" >}}
- {{< docref "hdc1080/" >}}
- {{< docref "sht3xd/" >}}
- {{< apiref "htu21d/htu21d.h" "htu21d/htu21d.h" >}}
- [i2cdevlib](https://github.com/jrowberg/i2cdevlib) by [Jeff Rowberg](https://github.com/jrowberg)
