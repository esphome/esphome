---
description: "Instructions for setting up AHT10 temperature and humidity sensors"
title: "AHT10 Temperature+Humidity Sensor"
params:
  seo:
    description: Instructions for setting up AHT10 temperature and humidity sensors
    image: aht10.jpg
---

The `aht10` Temperature+Humidity sensor allows you to use your AHT10
([datasheet](http://www.aosong.com/userfiles/files/media/aht10%E8%A7%84%E6%A0%BC%E4%B9%A6v1_1%EF%BC%8820191015%EF%BC%89.pdf)), AHT20 ([datasheet](https://cdn-learn.adafruit.com/assets/assets/000/091/676/original/AHT20-datasheet-2020-4-16.pdf?1591047915)) or AHT30 ([datasheet](https://eleparts.co.kr/data/goods_attach/202306/good-pdf-12751003-1.pdf)) [I²C](/components/i2c)-based sensor with ESPHome.

The DHT20 ([datasheet](https://cdn.sparkfun.com/assets/8/a/1/5/0/DHT20.pdf)) sensor has the packaging of the {{< docref "dht/" >}} series, but has the AHT20 inside and is speaking [I²C](/components/i2c) as well.

{{< img src="aht10-full.jpg" alt="Image" caption="AHT10 Temperature & Humidity Sensor." width="50.0%" class="align-center" >}}

{{< img src="temperature-humidity.png" alt="Image" width="80.0%" class="align-center" >}}

> [!NOTE]
> When configured for humidity, the log *'Components should block for at most 20-30ms in loop().'* will be generated in verbose mode. This is due to technical specs of the sensor and can not be avoided.

```yaml
# Example configuration entry
sensor:
  - platform: aht10
    variant: AHT10
    temperature:
      name: "Living Room Temperature"
    humidity:
      name: "Living Room Humidity"
    update_interval: 60s
```

## Configuration variables

- **variant** (*Optional*, enum): Set the variant of the device in use. Defaults to `AHT10`.

  - `AHT10` - For AHT10 devices.
  - `AHT20` - For AHT20 and AHT30 devices.

- **temperature** (**Required**): The information for the temperature sensor.

  - All options from [Sensor](/components/sensor).

- **humidity** (**Required**): The information for the humidity sensor

  - All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< apiref "aht10/aht10.h" "aht10/aht10.h" >}}
- [AHT10 Library](https://github.com/Thinary/AHT10) by [Thinary Electronic](https://github.com/Thinary)
- [Unofficial Translated AHT10 Datasheet (en)](https://wiki.liutyi.info/download/attachments/30507639/Aosong_AHT10_en_draft_0c.pdf)
