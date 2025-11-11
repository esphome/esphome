---
description: "Instructions for setting up LM75B temperature sensor."
title: "LM75B Temperature Sensor"
params:
  seo:
    description: Instructions for setting up LM75B temperature sensor.
    image: lm75b.jpg
---

The LM75B Temperature sensor allows you to use your NXP Semiconductors LM75B
([datasheet](https://www.nxp.com/docs/en/data-sheet/LM75B.pdf)) sensor with
ESPHome. The [I²C Bus](/components/i2c) is required to be set up in your configuration
for this sensor to work.

The LM75B is a temperature-to-digital converter using an on-chip band gap
temperature sensor and Sigma-Delta A-to-D conversion technique with an
overtemperature detection output.

The temperature register always stores an 11-bit twos complement data
giving a temperature resolution of 0.125 °C.

{{< img src="lm75b.jpg" alt="Image" caption="LM75B Temperature Sensor." width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: lm75b
    name: "Temperature"
    update_interval: 60s
```

## Configuration variables

- **address** (*Optional*, int): Manually specify the I²C address of the sensor. Defaults to `0x48`.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.
- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "lm75b/lm75b.h" "lm75b/lm75b.h" >}}
- [Datasheet](https://www.nxp.com/docs/en/data-sheet/LM75B.pdf)
