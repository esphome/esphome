---
description: "Instructions for setting up Honeywell ABP2 Pressure sensors"
title: "Honeywell ABP 2 Pressure Sensors"
params:
  seo:
    description: Instructions for setting up Honeywell ABP2 Pressure sensors
    image: honeywellabp.jpg
---

The `honeywellabp2_i2c` sensor platform allows you to use your Honeywell ABP2
([datasheet](https://prod-edam.honeywell.com/content/dam/honeywell-edam/sps/siot/en-us/products/sensors/pressure-sensors/board-mount-pressure-sensors/basic-abp2-series/documents/sps-siot-abp2-series-datasheet-32350268-en.pdf?download=false))
pressure and temperature sensors with ESPHome. The [I2C](/components/i2c) is
required to be set up in your configuration for this sensor to work

{{< img src="honeywellabp.jpg" alt="Image" caption="Honeywell ABP Pressure and Temperature Sensor." width="50.0%" class="align-center" >}}

```yaml
sensor:
  - platform: honeywellabp2_i2c
    pressure:
      name: "Honeywell2 pressure"
      min_pressure: 0
      max_pressure: 16000
      transfer_function: "A"
    temperature:
      name: "Honeywell2 temperature"
```

## Configuration variables

The values for `min_pressure` and `max_pressure` and `transfer_function` can be found in the device datasheet for the specific device.
These are used to calculate the pressure reading published by the sensor. Some sensors measure pressure in `bar` or `Psi`  ;
set `min_pressure` and `max_pressure` to the measurement range, `transfer_function` to `A` or `B` and `unit_of_measurement` to the appropriate unit for your device.

- **pressure** (*Optional*): The information for the pressure sensor.

  - **min_pressure** (**Required**, int or float): Minimum pressure for the pressure sensor.
  - **max_pressure** (**Required**, int or float): Maximum pressure for the pressure sensor.
  - **transfer_function** (**Required**, "A" or "B"): Transfer function used by the pressure sensor.
  - All other options from [Sensor](/components/sensor).

Some sensors do not have temperature sensing ability, see datasheet. In some cases the sensor may return a valid temperature even though the
datasheet indicates that the sensor does not measure temperature.

- **temperature** (*Optional*): The information for the temperature sensor.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "honeywellabp/honeywellabp2.h" "honeywellabp/honeywellabp2.h" >}}
