---
description: "Instructions for setting up ultrasonic distance measurement sensors in ESPHome."
title: "Ultrasonic Distance Sensor"
params:
  seo:
    description: Instructions for setting up ultrasonic distance measurement sensors in ESPHome.
    image: ultrasonic.jpg
---

The ultrasonic distance sensor allows you to use simple ultrasonic
sensors like the HC-SR04
([datasheet](https://www.electroschematics.com/wp-content/uploads/2013/07/HC-SR04-datasheet-version-2.pdf),
[SparkFun](https://www.sparkfun.com/products/13959)) with ESPHome
to measure distances. These sensors usually can't measure anything more
than about two meters and may sometimes make some annoying clicking
sounds.

This sensor platform expects a sensor that can be sent a **trigger
pulse** on a specific pin and will send out an **echo pulse** once a
measurement has been taken. Because sometimes (for example if no object
is detected) the echo pulse is never returned, this sensor also has a
timeout option which specifies how long to wait for values.

{{< img src="ultrasonic-full.jpg" alt="Image" caption="HC-SR04 Ultrasonic Distance Sensor." width="50.0%" class="align-center" >}}

{{< img src="ultrasonic-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: ultrasonic
    trigger_pin: D1
    echo_pin: D2
    name: "Ultrasonic Sensor"
```

## Configuration variables

- **trigger_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The output pin to
  periodically send the trigger pulse to.

- **echo_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The input pin on which to
  wait for the echo.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

- All other options from [Sensor](/components/sensor).

Advanced options:

- **timeout** (*Optional*, float): The number of meters for the
  timeout. Most sensors can only sense up to 2 meters. Defaults to 2 meters.

- **pulse_time** (*Optional*, [Time](/guides/configuration-types#time)): The duration for which the trigger pin will be
  active. Defaults to `10us`.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "template/" >}}
- {{< apiref "ultrasonic/ultrasonic_sensor.h" "ultrasonic/ultrasonic_sensor.h" >}}
