---
description: "Instructions for setting up SGP30 CO₂eq and Volatile Organic Compound sensor"
title: "SGP30 CO₂ and Volatile Organic Compound Sensor"
params:
  seo:
    description: Instructions for setting up SGP30 CO₂eq and Volatile Organic Compound sensor
    image: sgp30.jpg
---

The `sgp30` sensor platform allows you to use your Sensirion SGP30 multi-pixel gas
([datasheet](https://sensirion.com/media/documents/984E0DD5/61644B8B/Sensirion_Gas_Sensors_Datasheet_SGP30.pdf)) sensors or the SVM30 breakout-boards ([product page](https://sensirion.com/products/catalog/SVM30/)) with ESPHome.
The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

{{< img src="eco2-tvoc.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: sgp30
    eco2:
      name: "eCO2"
    tvoc:
      name: "TVOC"
```

## Configuration variables

- **eco2** (*Optional*): The information for the CO₂eq. sensor.

  - All options from [Sensor](/components/sensor).

- **tvoc** (*Optional*): The information for the total Volatile Organic Compounds sensor.

  - All options from [Sensor](/components/sensor).

- **store_baseline** (*Optional*, boolean): Store the sensor baselines persistently when calculated or updated.
  Defaults to `true`.

- **address** (*Optional*, int): Manually specify the I²C address of the sensor.
  Defaults to `0x58`.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

Advanced:

- **baseline** (*Optional*): The block containing baselines for calibration purposes. See [Calibrating Baseline](#sgp30-calibrating) for more info.

  - **eco2_baseline** (**Required**, int): The eCO2 baseline for calibration purposes. After OTA, this value is used to calibrate the sensor.

  - **tvoc_baseline** (**Required**, int): The TVOC baseline for calibration purposes. After OTA, this value is used to calibrate the sensor.

- **eco2_baseline** (*Optional*): The information for the CO₂eq. baseline value sensor. Baseline value is published in decimals.

  - All options from [Sensor](/components/sensor).

- **tvoc_baseline** (*Optional*): The information for the TVOC baseline value sensor. Baseline value is published in decimals.

  - All options from [Sensor](/components/sensor).

- **compensation** (*Optional*): The block containing sensors used for compensation. Both values must be supplied in order to be able to generate the absolute humidity to be reported to the sensor.

  - **temperature_source** (*Optional*, [ID](/guides/configuration-types#id)): Give an external temperature sensor ID
    here. The data must be in Celsius. This can improve the sensor's internal calculations.

  - **humidity_source** (*Optional*, [ID](/guides/configuration-types#id)): Give an external relative humidity sensor ID
    here. This can improve the sensor's internal calculations.

{{< anchor "sgp30-calibrating" >}}

## Calibrating Baseline

The SGP30 sensor will re-calibrate its baseline each time it is powered on. During the first power-up this will take up to 12 hours.
Exposing to outside air for at least 10 minutes cumulative time is advised during the calibration period.

For best performance and faster startup times, the current **baseline** needs to be persistently stored on the device before shutting it down and set again accordingly after boot up.
It implies that if the sensor reboots at a time when the air is less clean than normal, the values will have a constant offset and cannot be compared to the values before the last boot.

Using the **store_baseline** option will automatically store the baseline values after calibration or when it is updated during operation. When booting up, the stored values will then be
(re)applied in the sensor. Stored baselines are cleared after OTA.

Another method is to manually specify the baseline values in the configuration file. To do this, let the sensor boot up with no baseline set and let the sensor calibrate itself.
After around 12 hours you can then view the remote logs on the ESP. The nexttime the sensor is read out, you will see a log message with something like
`Current eCO2 baseline: 0x86C5, TVOC baseline: 0x8B38`.

Another way to obtain the baseline values is to configure the eco2 and TVOC baseline value sensors. Values will be published to your Home Automation system.
Convert the decimal value to hex value before use (e.g. 37577 --> 0x92C9)

Now set the baseline property in your configuration file like so with the value you got
via the logs:

```yaml
# Example configuration entry
sensor:
  - platform: sgp30
    # ...
    baseline:
      eco2_baseline: 0x86C5
      tvoc_baseline: 0x8B38
```

The next time you upload the code, the SGP30 will be continue its operation with this baseline and you will get consistent values.

Please note while the sensor is off, baseline values are valid for a maximum of seven days.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "dht/" >}}
- {{< docref "dht12/" >}}
- {{< docref "hdc1080/" >}}
- {{< docref "htu21d/" >}}
- {{< docref "sht3xd/" >}}
- {{< apiref "sgp30/sgp30.h" "sgp30/sgp30.h" >}}
