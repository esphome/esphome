---
description: "Instructions for setting up SFA30 Formaldehyde Sensor"
title: "SFA30 Formaldehyde Sensor"
params:
  seo:
    description: Instructions for setting up SFA30 Formaldehyde Sensor
    image: sfa30.jpg
---

The `sfa30` sensor platform allows you to use your Sensirion SFA30 Formaldehyde
([datasheet](https://sensirion.com/media/documents/DEB1C6D6/63D92360/Sensirion_formaldehyde_sensors_datasheet_SFA30.pdf)) sensors with ESPHome.
The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.
This sensor supports both UART and I²C communication. However, at the moment only I²C communication is implemented.

{{< img src="sfa30.jpg" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: sfa30
    formaldehyde:
      name: "Formaldehyde"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

## Configuration variables

- **formaldehyde** (*Optional*): The information for the Formaldehyde sensor.
  All options from [Sensor](/components/sensor).

- **temperature** (*Optional*): The information for the Temperature sensor.
  All options from [Sensor](/components/sensor).

- **humidity** (*Optional*): The information for the Humidity sensor.
  All options from [Sensor](/components/sensor).

- **address** (*Optional*, int): Manually specify the I²C address of the sensor.
  Defaults to `0x5D`.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< apiref "sfa30/sfa30.h" "sfa30/sfa30.h" >}}
