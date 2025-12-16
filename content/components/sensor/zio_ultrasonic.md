---
description: "Instructions for setting up the Zio Ultrasonic Distance sensor."
title: "Zio Ultrasonic Distance Sensor"
params:
  seo:
    description: Instructions for setting up the Zio Ultrasonic Distance sensor.
    image: zio_ultrasonic.jpg
---

The Zio Ultrasonic Distance sensor allows you to use your compatible
([datasheet](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf),
[sparkfun](https://www.sparkfun.com/products/17777))
sensors with ESPHome.

{{< img src="zio_ultrasonic.jpg" alt="Image" caption="Zio Ultrasonic Distance Sensor. (Credit: [Sparkfun](https://www.sparkfun.com/products/17777), image cropped and compressed)" width="30.0%" class="align-center" >}}

The Zio Ultrasonic Distance Sensor is an ultrasonic distance sensor based on the HC-SR04 sensor. Unlike the {{< docref "/components/sensor/ultrasonic" "Ultrasonic Distance Sensor component" >}}, measurements are read over the I²C bus.

To use the sensor, first set up an [I²C Bus](/components/i2c) and connect the sensor to the specified pins.

```yaml
# Example configuration entry
sensor:
  - platform: zio_ultrasonic
    name: "Distance"
    update_interval: 60s
```

## Configuration variables

- **address** (*Optional*, int): Manually specify the I²C address of the sensor. Defaults to `0x00`.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.
- All other options from [Sensor](/components/sensor).

## See Also

- {{< docref "/components/sensor/ultrasonic" "Ultrasonic Sensor Component" >}}
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "template/" >}}
