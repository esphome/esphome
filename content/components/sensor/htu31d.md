---
description: "Instructions for setting up HTU31D temperature and humidity sensors."
title: "HTU31D Temperature & Humidity Sensor"
params:
  seo:
    description: Instructions for setting up HTU31D temperature and humidity sensors.
    image: htu31d.jpg
---

The HTU31D Temperature & Humidity component allows you to use HTU31D sensors with
ESPHome. The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

Example sensors:

- ([Adafruit](https://www.adafruit.com/product/4832))

{{< img src="htu31d.jpg" alt="Image" caption="HTU31D Temperature & Humidity Sensor. Image by [Adafruit](https://www.adafruit.com)." width="50.0%" class="align-center" >}}

{{< img src="temperature-humidity.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: htu31d
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

## Configuration variables

- **temperature** (*Optional*): The information for the temperature sensor.
  All options from [Sensor](/components/sensor).

- **humidity** (*Optional*): The information for the humidity sensor.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< docref "htu21d/" >}}
- {{< docref "dht/" >}}
- {{< docref "dht12/" >}}
- {{< docref "hdc1080/" >}}
- {{< docref "sht3xd/" >}}
- {{< apiref "htu31d/htu31d.h" "htu31d/htu31d.h" >}}
- [i2cdevlib](https://github.com/jrowberg/i2cdevlib) by [Jeff Rowberg](https://github.com/jrowberg)
