---
description: "Instructions for setting up A02YYUW waterproof ultrasonic distance sensor in ESPHome."
title: "A02YYUW Waterproof Ultrasonic Sensor"
params:
  seo:
    description: Instructions for setting up A02YYUW waterproof ultrasonic distance sensor in ESPHome.
    image: a02yyuw.jpg
---

This sensor allows you to use A02YYUW waterproof ultrasonic sensor by DFRobot
([datasheet](https://wiki.dfrobot.com/_A02YYUW_Waterproof_Ultrasonic_Sensor_SKU_SEN0311))
with ESPHome to measure distances. This sensor can measure
ranges between 3 centimeters and 450 centimeters with a resolution of 1 millimeter.

Since this sensor reads multiple times per second, [Sensor Filters](/components/sensor#sensor-filters) are highly recommended.

To use the sensor, first set up an [UART Bus](/components/uart) with a baud rate of 9600 and connect the sensor to the specified pin.

{{< img src="a02yyuw-full.jpg" alt="Image" caption="A02YYUW Waterproof Ultrasonic Distance Sensor." width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: "a02yyuw"
    name: "Distance"
```

## Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the [UART bus](/components/uart) you wish to use for this sensor.
  Use this if you want to use multiple UART buses at once.

- All other options from [Sensor](/components/sensor).

> [!NOTE]
> [PWM and RS485](https://www.dypcn.com/uploads/A02-Datasheet.pdf) versions of the A02YYUW are not supported by this component.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- [UART Bus](/components/uart)
- {{< apiref "a02yyuw/a02yyuw.h" "a02yyuw/a02yyuw.h" >}}
