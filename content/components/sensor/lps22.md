---
description: "Instructions for setting up LPS22 barometric pressure sensor"
title: "LPS22 Barometric Pressure Sensor"
params:
  seo:
    description: Instructions for setting up LPS22 barometric pressure sensor
---

The `lps22` sensor platform allows you to use your LPS22HB or LPS22HH pressure sensor
([datasheet](https://www.st.com/resource/en/application_note/an4672-lps22hblps25hb-digital-pressure-sensors-hardware-guidelines-for-system-integration-stmicroelectronics.pdf)) with ESPHome.

The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

{{< img src="lps22.webp" alt="Image" class="align-center" >}}

```yaml
sensor:
  - platform: lps22
    temperature:
      name: "LPS22 Temperature"
    pressure:
      name: "LPS22 Pressure"
```

## Configuration variables

- **temperature** (*Optional*): Temperature.

  - All options from [Sensor](/components/sensor).

- **pressure** (*Optional*): Barometric Pressure.

  - All options from [Sensor](/components/sensor).

- **address** (*Optional*, int): Manually specify the I²C address of the sensor. Default is `0x5d`. `0x5c` is another common address.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.

## Sensor sampling details

The LPS22 sensors support variety of sampling and streaming approaches: periodic at various
frequencies from 1Hz to 75Hz, as well as single-shot sampling mode. Single-shot sampling is
implemented in this component letting the sensor to enter the power-down mode between samples,
saving significant power.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "lps22/lps22.h" "lps22/lps22.h" >}}
