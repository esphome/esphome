---
description: "Instructions for setting up BME68x temperature, humidity, pressure, and gas sensors via BSEC2."
title: "BME68x Temperature, Humidity, Pressure & Gas Sensor via BSEC2"
params:
  seo:
    description: Instructions for setting up BME68x temperature, humidity, pressure, and gas sensors via BSEC2.
    image: bme680.jpg
---

## Component/Hub

The `bme68x_bsec2_i2c` sensor platform allows you to use your
[BME680](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme680-ds001.pdf) and
[BME688](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf)
([Adafruit](https://www.adafruit.com/product/3660), [Pimoroni](https://shop.pimoroni.com/products/bme680-breakout)) temperature, humidity, pressure and gas sensors with ESPHome via the Bosch Sensortec
Environmental Cluster 2 (BSEC2) software library. The use of Bosch's proprietary algorithms provide an Index for Air
Quality (IAQ) measurement derived from the gas resistance sensor's response to specific Volatile Organic Compounds
(VOCs). The BSEC software also provides estimated values for CO₂ and Breath Volatile Organic Compounds (b-VOC) using
a correlation between VOC and CO₂ in a human's exhaled breath.

The [I²C](/components/i2c) is required to be set up in your configuration for this sensor to work.

> [!NOTE]
> The BSEC2 library is only available for use after accepting its software license agreement. By enabling this
> component in your configuration, you are explicitly agreeing to the terms of the [BSEC license agreement](https://www.bosch-sensortec.com/media/boschsensortec/downloads/software/bme688_development_software/2024_12/20241219_clickthrough_license_terms_bsec_bme680_bme688_bme690.pdf). Note
> that the license forbids distribution of any compiled firmware binaries that include this component.

{{< img src="bme680-full.jpg" alt="Image" caption="BME680 Temperature, Pressure, Humidity & Gas Sensor." width="50.0%" class="align-center" >}}

{{< img src="bme680-bsec-ui.png" alt="Image" caption="Example UI" width="80.0%" class="align-center" >}}

```yaml
# Minimal example configuration with common sensors
bme68x_bsec2_i2c:
  address: 0x76
  model: bme680
  operating_age: 28d
  sample_rate: LP
  supply_voltage: 3.3V
```

### Configuration variables

- **address** (_Optional_, int): Manually specify the I²C address of the sensor. Defaults to `0x76`. The sensor can
  also be configured to use `0x77`.

- **i2c_id** (**Optional**, [ID](/guides/configuration-types#id)): The ID of the [I²C bus](/components/i2c) the bme68x is connected to.
- **model** (_Required_, string): The model of the connected sensor; either `BME680` or `BME688`.
- **algorithm_output** (_Optional_, string): The output of the BSEC2 algorithm. Either `classification` (default) or
  `regression`. _Only valid when model is BME688._

- **operating_age** (_Optional_, string): The history BSEC2 considers for the automatic background calibration of the
  IAQ in days. That means changes in this time period will influence the IAQ value. Either `4d` or `28d` (default).

- **sample_rate** (_Optional_, string): Sample rate. Default is `LP` for low power consumption, sampling every 3
  seconds. Can be `ULP` for ultra-low power, sampling every 5 minutes. This controls the sampling rate for
  gas-dependent sensors and will govern the interval at which the sensor heater is operated. By default, this rate will
  also be used for temperature, humidity and pressure sensors but can be overridden per-sensor if required.

- **supply_voltage** (_Optional_, string): Supply voltage of the sensor. Default is `3.3V`. Can be set to `1.8V` if
  your sensor is powerd with 1.8 volts (for example, the Pimoroni PIM357 BME680 breakout module).

- **temperature_offset** (_Optional_, float): Temperature offset if device is in enclosure and reads too high. This
  value is subtracted from the reading (for example, if the sensor reads 5°C higher than expected, set this to `5`  )
  and also corrects the relative humidity readings. Defaults to `0`.

- **state_save_interval** (_Optional_, [Time](/guides/configuration-types#time)): The minimum interval at which to save the calibrated BSEC2
  algorithm state to flash so that calibration doesn't have to start from scratch on device restart. Defaults to `6h`.

- **id** (_Optional_, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation. Use this ID in the sensor
  section to refer to the correct BME68x sensor if you have more than one device. This will also be used to refer to
  the calibrated BSEC2 algorithm state saved to flash.

## Sensor

```yaml
sensor:
  - platform: bme68x_bsec2
    temperature:
      name: "BME68x Temperature"
    pressure:
      name: "BME68x Pressure"
    humidity:
      name: "BME68x Humidity"
    iaq:
      name: "BME68x IAQ"
      id: iaq
    co2_equivalent:
      name: "BME68x CO2 Equivalent"
    breath_voc_equivalent:
      name: "BME68x Breath VOC Equivalent"
```

### Configuration variables

- **bme68x_bsec2_id** (_Optional_, [ID](/guides/configuration-types#id)): The ID of the `bme68x_bsec2_i2c` component sensors will refer
  to. Useful when multiple devices are present in your configuration.

- **temperature** (_Optional_): Configuration for the temperature sensor.

  - **sample_rate** (_Optional_, string): Optional sample rate override for this sensor. Can be `LP` for low power
    consumption, sampling every 3 seconds or `ULP` for ultra-low power, sampling every 5 minutes.

  - All other options from [Sensor](/components/sensor).

- **pressure** (_Optional_): Configuration for the pressure sensor.

  - **sample_rate** (_Optional_, string): Optional sample rate override for this sensor. Can be `LP` for low power
    consumption, sampling every 3 seconds or `ULP` for ultra-low power, sampling every 5 minutes.

  - All other options from [Sensor](/components/sensor).

- **humidity** (_Optional_): Configuration for the humidity sensor.

  - **sample_rate** (_Optional_, string): Optional sample rate override for this sensor. Can be `LP` for low power
    consumption, sampling every 3 seconds or `ULP` for ultra-low power, sampling every 5 minutes.

  - All other options from [Sensor](/components/sensor).

- **gas_resistance** (_Optional_): Configuration for the gas sensor.

  - All options from [Sensor](/components/sensor).

- **iaq** (_Optional_): Configuration for the IAQ sensor.

  - All options from [Sensor](/components/sensor).

- **iaq_static** (_Optional_): Configuration for the IAQ static sensor.

  - All options from [Sensor](/components/sensor).

- **iaq_accuracy** (_Optional_): Configuration for the numeric IAQ accuracy sensor.

  - All options from [Sensor](/components/sensor).

- **co2_equivalent** (_Optional_): Configuration for the CO₂ equivalent sensor.

  - All options from [Sensor](/components/sensor).

- **breath_voc_equivalent** (_Optional_): Configuration for the Breath VOC equivalent humidity sensor.

  - All options from [Sensor](/components/sensor).

## Text Sensor

The sensor's accuracy can be reported in text format.

```yaml
text_sensor:
  - platform: bme68x_bsec2
    iaq_accuracy:
      name: "BME68x IAQ Accuracy"
```

### Configuration variables

- **bme68x_bsec2_id** (_Optional_, [ID](/guides/configuration-types#id)): The ID of the `bme68x_bsec2_i2c` component the text sensor
  will refer to. Useful when multiple devices are present in your configuration.

- **iaq_accuracy** (_Optional_): Configuration for the IAQ accuracy sensor. Shows: `Stabilizing`, `Uncertain`,
  `Calibrating`, `Calibrated`.

  - All other options from [Text Sensor](/components/text_sensor#config-text_sensor).

## Index for Air Quality (IAQ) Measurement

The measurements are expressed with an index scale ranging from 0 to 500. The index itself is deduced from tests using
ethanol gas, as well as important VOC in the exhaled breath of healthy humans. The VOC values themselves are derived
from several publications on breath analysis studies. The BSEC2 software library defines the levels as follows:

| IAQ Index | Air Quality         |
| --------- | ------------------- |
| 0 - 50    | Excellent           |
| 51 - 100  | Good                |
| 101 - 150 | Lightly polluted |
| 151 - 200 | Moderately polluted |
| 201 - 250 | Heavily polluted |
| 251 - 350 | Severely polluted |
| > 351 | Extremely polluted |

This can be represented by a template text sensor such as below

```yaml
text_sensor:
  - platform: template
    name: "BME68x IAQ Classification"
    lambda: |-
      if (int(id(iaq).state) <= 50) {
        return {"Excellent"};
      } else if (int(id(iaq).state) >= 51 && int(id(iaq).state) <= 100) {
        return {"Good"};
      } else if (int(id(iaq).state) >= 101 && int(id(iaq).state) <= 150) {
        return {"Lightly polluted"};
      } else if (int(id(iaq).state) >= 151 && int(id(iaq).state) <= 200) {
        return {"Moderately polluted"};
      } else if (int(id(iaq).state) >= 201 && int(id(iaq).state) <= 250) {
        return {"Heavily polluted"};
      } else if (int(id(iaq).state) >= 251 && int(id(iaq).state) <= 350) {
        return {"Severely polluted"};
      } else if (int(id(iaq).state) >= 351 && int(id(iaq).state) <= 500) {
        return {"Extremely polluted"};
      } else {
        return {"Error"};
      }
```

The selected b-VOC gasses are as follows:

| Compound           | Molar fraction |
| ------------------ | -------------- |
| [Ethane](https://en.wikipedia.org/wiki/Ethane)          | 5 ppm          |
| [Isoprene](https://en.wikipedia.org/wiki/Isoprene)        | 10 ppm         |
| [Ethanol](https://en.wikipedia.org/wiki/Ethanol) | 10 ppm |
| [Acetone](https://en.wikipedia.org/wiki/Acetone) | 50 ppm |
| [Carbon Monoxide](https://en.wikipedia.org/wiki/Carbon_monoxide) | 15 ppm |

{{< anchor "bsec2-calibration" >}}

## IAQ Accuracy and Calibration

The BSEC2 software automatically calibrates in the background to provide consistent IAQ performance. The calibration
process considers the recent measurement history so that a value of 50 corresponds to a “typical good” level and a
value of 200 to a “typical polluted” level. The IAQ Accuracy sensor will indicate one of the following values:

- `Stabilizing`  : The device has just started, and the sensor is stabilizing (this typically lasts 5 minutes)
- `Uncertain`  : The background history of BSEC2 is uncertain. This typically means the gas sensor data was too stable
  for BSEC2 to clearly define its reference.

- `Calibrating`  : BSEC2 found new calibration data and is currently calibrating.
- `Calibrated`  : BSEC2 calibrated successfully.

Every `state_save_interval`, or as soon thereafter when full calibration is reached, the current algorithm state is
saved to flash so that the process does not have to start from scratch on device restart.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< docref "bme680/" >}}
- {{< apiref "bme68x_bsec2_i2c/bme68x_bsec2_i2c.h" "bme68x_bsec2_i2c/bme68x_bsec2_i2c.h" >}}
- [BME680 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme680-ds001.pdf)
- [BME688 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf)
- [Bosch BSEC2 Library](https://github.com/boschsensortec/Bosch-BSEC2-Library) by [Bosch Sensortec](https://www.bosch-sensortec.com/)
- [Bosch Sensortec Community](https://community.bosch-sensortec.com/)
