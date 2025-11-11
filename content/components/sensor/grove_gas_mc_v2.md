---
description: "Instructions for setting up Grove Multichannel Gas Sensor V2 that"
title: "Grove Multichannel Gas Sensor V2"
params:
  seo:
    description: Instructions for setting up Grove Multichannel Gas Sensor V2 that
    image: grove-gas-mc-v2.png
---

The `grove_gas_mc_v2` sensor platform allows you to use your [Grove Multichannel GasSensor V2](https://wiki.seeedstudio.com/Grove-Multichannel-Gas-Sensor-V2)
with ESPHome. It exposes 4 different gas sensors for qualitatively measuring
Nitrogen Dioxide (NO2), Carbon Monoxide (CO), Ethanol (C2H5OH), and Volatile Organic
Compounds (VOCs).

> [!NOTE]
> The Grove Multichannel Gas Sensor V2 is a qualitative, not quantitative, sensor.
> This means values reported back are raw ADC values. Values are **not** in a common unit
> of measurement, such as PPM (parts per million). If you have known baseline readings
> for any of the gases, [Sensor Filters](/components/sensor#sensor-filters) could be used to calibrate the raw readings.

{{< img src="grove-gas-mc-v2.png" alt="Image" caption="Grove Multichannel Gas Sensor V2" width="50.0%" class="align-center" >}}

The communication with this sensor is done via [I²C Bus](/components/i2c), so you need to have
an `i2c:` section in your config for this integration to work.

```yaml
sensor:
  - platform: grove_gas_mc_v2
    nitrogen_dioxide:
      name: "Nitrogen Dioxide"
    ethanol:
      name: "Ethanol"
    carbon_monoxide:
      name: "Carbon Monoxide"
    tvoc:
      name: "Volatile Organic Compounds"
```

## Configuration variables

- **nitrogen_dioxide** (**Required**): The Nitrogen Dioxide sensor data.
  All options from [Sensor](/components/sensor).

- **ethanol** (**Required**): The Ethanol (C2H5OH) sensor data.
  All options from [Sensor](/components/sensor).

- **carbon_monoxide** (**Required**): The Carbon Monoxide sensor data.
  All options from [Sensor](/components/sensor).

- **tvoc** (**Required**): The Total Volatile Organic Compounds (TVOC) sensor data.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

Advanced:

- **address** (*Optional*, int): The [I²C](/components/i2c) address of the sensor.
  Defaults to `0x08`

{{< anchor "grove-gas-mc-v2-preheating" >}}

## Preheating

If the sensor is stored for a long period of time (without power) there is a recommended
minimum warm-up time required for the sensor before the readings settle down and become
more accurate.

A recommended warm-up time of 24 hours is recommend if the sensor has been stored
less than a month, 48 hours for 1-6 months and at least 72 hours for anything longer
than 6 months.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- [Grove Multichannel V2 Library](https://github.com/Seeed-Studio/Seeed_Arduino_MultiGas)
- {{< apiref "grove_gas_mc_v2/grove_gas_mc_v2.h" "grove_gas_mc_v2/grove_gas_mc_v2.h" >}}
