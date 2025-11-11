---
description: "Instructions for setting up SGP40/SGP41 Volatile Organic Compound and NOx sensor"
title: "SGP40 Volatile Organic Compound Sensor and SGP41 VOC and NOx Sensor"
params:
  seo:
    description: Instructions for setting up SGP40/SGP41 Volatile Organic Compound and NOx sensor
    image: sgp40.jpg
---

The `sgp4x` sensor platform allows you to use your Sensirion SGP40
([datasheet](https://sensirion.com/media/documents/296373BB/6203C5DF/Sensirion_Gas_Sensors_Datasheet_SGP40.pdf)) or SGP41
([datasheet](https://sensirion.com/media/documents/5FE8673C/61E96F50/Sensirion_Gas_Sensors_Datasheet_SGP41.pdf)) with ESPHome.
The type of sensor used is automatically detected.
The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

> [!NOTE]
> This sensor need to be driven at a rate of 1Hz. Because of this, the
> sensor will be read out on device once a second separately from the
> update_interval. The state will be reported to other components, or
> the front end at the update_interval, saving wifi power and network
> communication.

{{< img src="sgp40.jpg" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: sgp4x
    voc:
      name: "VOC Index"
    nox:
      name: "NOx Index"
```

## Configuration variables

- **voc** (*Optional*): VOC Index

  - **algorithm_tuning** (*Optional*): The VOC algorithm can be customized by tuning 6 different parameters. For more details see [Engineering Guidelines for SEN5x](https://sensirion.com/media/documents/25AB572C/62B463AA/Sensirion_Engineering_Guidelines_SEN5x.pdf)

    - **index_offset** (*Optional*): VOC index representing typical (average) conditions. Allowed values are in range 1..250. The default value is 100.
    - **learning_time_offset_hours** (*Optional*): Time constant to estimate the VOC algorithm offset from the history in hours. Past events will be forgotten after about twice the learning time. Allowed values are in range 1..1000. The default value is 12 hour
    - **learning_time_gain_hours** (*Optional*): Time constant to estimate the VOC algorithm gain from the history in hours. Past events will be forgotten after about twice the learning time. Allowed values are in range 1..1000. The default value is 12 hours.
    - **gating_max_duration_minutes** (*Optional*): Maximum duration of gating in minutes (freeze of estimator during high VOC index signal). Zero disables the gating. Allowed values are in range 0..3000. The default value is 180 minutes
    - **std_initial** (*Optional*): Initial estimate for standard deviation. Lower value boosts events during initial learning period, but may result in larger device-todevice variations. Allowed values are in range 10..5000. The default value is 50.
    - **gain_factor** (*Optional*): Gain factor to amplify or to attenuate the VOC index output. Allowed values are in range 1..1000. The default value is 230.

  - All other options from [Sensor](/components/sensor).

- **nox** (*Optional*): NOx Index. Only available with SGP41. If a SGP40 sensor is detected this sensor will be ignored

  - **algorithm_tuning** (*Optional*): The NOx algorithm can be customized by tuning 5 different parameters.For more details see [Engineering Guidelines for SEN5x](https://sensirion.com/media/documents/25AB572C/62B463AA/Sensirion_Engineering_Guidelines_SEN5x.pdf)

    - **index_offset** (*Optional*): NOx index representing typical (average) conditions. Allowed values are in range 1..250. The default value is 100.
    - **learning_time_offset_hours** (*Optional*): Time constant to estimate the NOx algorithm offset from the history in hours. Past events will be forgotten after about twice the learning time. Allowed values are in range 1..1000. The default value is 12 hour
    - **learning_time_gain_hours** (*Optional*): Time constant to estimate the NOx algorithm gain from the history in hours. Past events will be forgotten after about twice the learning time. Allowed values are in range 1..1000. The default value is 12 hours.
    - **gating_max_duration_minutes** (*Optional*): Maximum duration of gating in minutes (freeze of estimator during high NOx index signal). Zero disables the gating. Allowed values are in range 0..3000. The default value is 180 minutes
    - **std_initial** (*Optional*): The initial estimate for standard deviation parameter has no impact for NOx. This parameter is still in place for consistency reasons with the VOC tuning parameters command. This parameter must always be set to 50.
    - **gain_factor** (*Optional*): Gain factor to amplify or to attenuate the VOC index output. Allowed values are in range 1..1000. The default value is 230.

  - All other options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`
- **store_baseline** (*Optional*, boolean): Stores and retrieves the baseline information for quicker startups. Defaults to `true`

- **compensation** (*Optional*): The block containing sensors used for compensation. If not set defaults will be used.

  - **temperature_source** (*Optional*, [ID](/guides/configuration-types#id)): Give an external temperature sensor ID
    here. This can improve the sensor's internal calculations. Defaults to `25`

  - **humidity_source** (*Optional*, [ID](/guides/configuration-types#id)): Give an external humidity sensor ID
    here. This can improve the sensor's internal calculations. Defaults to `50`

## Example With Compensation

```yaml
# Example configuration entry
sensor:
- platform: sgp4x
  voc:
    name: "VOC Index"
  nox:
    name: "NOx Index"
  compensation:
    humidity_source: dht1_hum
    temperature_source: dht1_temp
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "dht/" >}}
- {{< docref "dht12/" >}}
- {{< docref "hdc1080/" >}}
- {{< docref "htu21d/" >}}
- {{< docref "sht3xd/" >}}
- {{< docref "sht4x/" >}}
- {{< apiref "sgp4x/sgp4x.h" "sgp4x/sgp4x.h" >}}
