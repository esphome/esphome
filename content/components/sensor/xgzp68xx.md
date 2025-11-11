---
description: "Instructions for setting up the CFSensor XGZP68xx Non-C Series Differential Pressure sensor."
title: "CFSensor XGZP68xx Non-C Series Differential Pressure Sensor"
params:
  seo:
    description: Instructions for setting up the CFSensor XGZP68xx Non-C Series Differential Pressure sensor.
    image: 6897d.jpg
---

CFSensor makes multiple generations of sensors with identical model numbers such as the 6899D or
6897D, but unfortunately with completely different I²C interfaces. You can identify which from
the part number:

- XGZP6897Dxxxxxxxx is the non-C series, which this component supports.
- XGZP6897D**C**xxxxxxxx is the C series, which this component does NOT support.

Another way of telling the difference is the I²C address:

- If the device is one of the non-C series, it appears at I²C address `0x6d`.
- If the device is one of the C series, it appears at I²C address `0x58`.

Unfortunately CFSensor have removed from their website the datasheets for the non-C series of
sensors. You will need to specifically find the older datasheet from another source.
**The v3.1 or later datasheets from CFSensor describe the C series which is a completely
different device even though it has an identical model number to the <= v3.0 device**.

{{< img src="6897d.jpg" alt="Image" caption="XGZP6897D Differential Pressure Sensor. (Credit: [CFSensor](https://cfsensor.net/i2c-differential-pressure-sensor-xgzp6897d/), image cropped and compressed)" width="30.0%" class="align-center" >}}

To use the sensor, set up an [I²C Bus](/components/i2c) and connect the sensor to the specified pins.

```yaml
# Example configuration entry
# It uses a filter offset to calibrate the sensor
sensor:
  - platform: xgzp68xx
    k_value: 16384
    temperature:
        name: "Temperature"
    pressure:
        name: "Differential Pressure"
        oversampling: "32768x"
        filters:
            - offset: 40.5
```

## Configuration variables

- **temperature** (*Optional*): All options from [Sensor](/components/sensor).
- **pressure** (*Optional*): All options from [Sensor](/components/sensor).
  - **oversampling** (*Optional*): One of `256x`, `512x`, `1024x`, `2048x`, `4096x`, `8192x`, `16384x`, `32768x`. It is not possible to disable oversampling. If not specified, this defaults to `4096x`.
- **k_value** (*Optional*, int): The K value comes from the list below. It will default to 4096 if not specified.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.

## Usage

The sensors come in a series of pressure ranges. The now hard to find datasheet lists
a table of pressure ranges to ``k_value``, which you will need to set in your configuration:

- -0.5 kPa to +0.5 kPa (``k_value = 16384``)
- -1 kPa to +1 kPa (``k_value = 8192``)
- -2.5 kPa to +2.5 kPa (``k_value = 2048``)
- -5 kPa to +5 kPa (``k_value = 1024``)
- -10 kPa to +10 kPa (``k_value = 512``)
- -50 kPa to +50 kPa (``k_value = 128``)

On power up, the sensor will read an offset. You will need to calibrate the sensor which can be done
by checking the value that is returned when the ports are open to the air. You can use the offset
option to correct the reading. For example, if your sensor is reading -40Pa when the ports are
disconnected, you can set the offset to 40.

If the sensor is running for a time, you will find that the zero point does drift after a while,
especially if the sensor is used to detect pressure shocks.
Designing in a way to periodically equalise the pressure between the ports so a new offset can be
determined would be wise.

## See Also

- [esphome-pressure device](https://github.com/gcormier/esphome-pressure/)
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "xgzp6xx/xgzp6xx.h" "xgzp6xx/xgzp6xx.h" >}}
