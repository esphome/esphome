---
description: "Instructions for setting up the SDP3x or SDP800 Series Differential Pressure sensor."
title: "SDP3x / SDP800 Series Differential Pressure Sensor"
params:
  seo:
    description: Instructions for setting up the SDP3x or SDP800 Series Differential Pressure sensor.
    image: sdp31.jpg
---

The SDP3x Differential Pressure sensor allows you to use your SDP3x
([datasheet](https://sensirion.com/media/documents/4D045D69/6375F34F/DP_DS_SDP3x_digital_D1.pdf),
[sparkfun](https://www.sparkfun.com/products/17874)) or SDP800 Series ([datasheet](https://sensirion.com/media/documents/90500156/6167E43B/Sensirion_Differential_Pressure_Datasheet_SDP8xx_Digital.pdf))
sensors with ESPHome.

{{< img src="sdp31.jpg" alt="Image" caption="SDP31 Differential Pressure Sensor. (Credit: [Sparkfun](https://www.sparkfun.com/products/17874), image cropped and compressed)" width="30.0%" class="align-center" >}}

To use the sensor, set up an [I²C Bus](/components/i2c) and connect the sensor to the specified pins.

```yaml
# Example configuration entry
- platform: sdp3x
  name: "HVAC Filter Pressure drop"
  id: filter_pressure
```

## Configuration variables

- **address** (*Optional*, int): The I²C address of the sensor. Defaults to `0x21`.
- **measurement_mode** (*Optional*): The measurement mode of the sensor. Valid options are `differential_pressure` and `mass_flow`. Defaults to `differential_pressure`.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.
- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "sdp3x/sdp3x.h" "sdp3x/sdp3x.h" >}}
