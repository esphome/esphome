---
description: "Instructions for setting up the Rohm Semiconductor BH1900NUX temperature sensor in ESPHome."
title: "BH1900NUX Temperature Sensor"
params:
  seo:
    description: Instructions for setting up the Rohm Semiconductor BH1900NUX temperature sensor in ESPHome
    image: bh1900nux-evk-001.png
---
{{< anchor "bh1900nux" >}}
The `bh1900nux` sensor platform allows you to use the **BH1900NUX**
([datasheet](https://fscdn.rohm.com/en/products/databook/datasheet/ic/sensor/temperature/bh1900nux-e.pdf))
**temperature sensor** from Rohm Semiconductor with ESPHome.
The [I²C bus](/components/i2c) must be set up in your configuration for this sensor to work.

{{< img src="bh1900nux-evk-001.png" alt="BH1900NUX-EVK-001 Evaluation Board" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: bh1900nux
    name: "BH1900NUX Temperature"
    address: 0x48
    update_interval: 60s
```

## Configuration Variables  

- **address** (*Optional*, int): Manually specify the I²C address of the sensor. Defaults to `0x48`.  

- **update_interval** (*Optional*, [Time](/guides/configuration-types#config-time)): The interval to check the sensor. Defaults to `60s`.

- All other options from [Sensor](/components/sensor#config-sensor).

> [!NOTE]
> The following features are **not supported**: `ALERT` pin functionality and `TLOW`/`THIGH` configuration (thermostat mode).  

## Configurable I²C Addresses  

The BH1900NUX provides **3 address pins (A0, A1, A2)** to set the I²C address by connecting them to **VCC (1)** or **GND (0)**.
This allows **8 possible addresses**:

| Address | A0 | A1 | A2 |
|---------|----|----|----|
| `0x48`  | 0  | 0  | 0  |
| `0x49`  | 0  | 0  | 1  |
| `0x4A`  | 0  | 1  | 0  |
| `0x4B`  | 0  | 1  | 1  |
| `0x4C`  | 1  | 0  | 0  |
| `0x4D`  | 1  | 0  | 1  |
| `0x4E`  | 1  | 1  | 0  |
| `0x4F`  | 1  | 1  | 1  |

## See Also  

- [Sensor Filters](/components/sensor#sensor-filters)
- [Product Page](https://www.rohm.com/products/sensors-mems/temperature-sensor-ics/bh1900nux-product)
- [BH1900NUX Datasheet](https://fscdn.rohm.com/en/products/databook/datasheet/ic/sensor/temperature/bh1900nux-e.pdf)
