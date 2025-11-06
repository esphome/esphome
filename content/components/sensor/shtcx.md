---
description: "Instructions for setting up SHTC1 and SHTC3 temperature and humidity sensors"
title: "SHTCx Temperature+Humidity Sensors"
params:
  seo:
    description: Instructions for setting up SHTC1 and SHTC3 temperature and humidity sensors
    image: shtc3.jpg
---

The `shtcx` sensor platform Temperature+Humidity sensor allows you to use your Sensirion SHTC1
([datasheet](https://sensirion.com/media/documents/21BF77EA/63A5A411/Datasheet_SHTC1.pdf),
[Sensirion SHTC1](https://sensirion.com/products/catalog/SHTC1/)) and
the newer SHTC3
([datasheet](https://sensirion.com/media/documents/643F9C8E/63A5A436/Datasheet_SHTC3.pdf),
`SparkFun`_ ) sensors with
ESPHome. The [I²C Bus](#i2c) is
required to be set up in your configuration for this sensor to work.

{{< img src="temperature-humidity.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: shtcx
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

## Configuration variables

- **temperature** (*Optional*): The information for the temperature sensor.

  - All options from [Sensor](#config-sensor).

- **humidity** (*Optional*): The information for the humidity sensor.

  - All options from [Sensor](#config-sensor).

- **address** (*Optional*, int): Manually specify the I²C address of the sensor.
  Defaults to `0x70`.

- **update_interval** (*Optional*, [Time](#config-time)): The interval to check the
  sensor. Defaults to `60s`.

## See Also

- [Sensor Filters](#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< docref "dht/" >}}
- {{< docref "dht12/" >}}
- {{< docref "hdc1080/" >}}
- {{< docref "htu21d/" >}}
- {{< docref "sht3xd/" >}}
- {{< apiref "shtcx/shtcx.h" "shtcx/shtcx.h" >}}
