---
description: "Instructions for setting up JSN-SR04T waterproof ultrasonic distance sensor in ESPHome."
title: "JSN-SR04T Waterproof Ultrasonic Range Finder"
params:
  seo:
    description: Instructions for setting up JSN-SR04T waterproof ultrasonic distance sensor in ESPHome.
    image: jsn-sr04t-v3.jpg
---

This sensor allows you to use the JSN-SR04T and AJ_SR04M Waterproof Ultrasonic Range Finder **in Mode 1 and 2**
with ESPHome to measure distances. This sensor can measure
ranges between 25 centimeters and 600 centimeters with a resolution of 1 millimeter.

Configure the JSN-SR04T for mode 1:

- **V1.0 and V2.0**: Add a 47k resistor to pad R27.
- **V3.0**: Short pad M1 or add 47k resistor to pad mode.

Configure the JSN-SR04T for mode 2:

- **V1.0 and V2.0**: Add a 120k resistor to pad R27.
- **V3.0**: Short pad M2 or add 120k resistor to pad mode.

Configure the AJ_SR04M for mode 1:

- Add a 120k resistor to pad R19.

Configure the AJ_SR04M for mode 2:

- Add a 47k resistor to pad R19.

{{< img src="jsn-sr04t-v3-mode-select-pads.jpg" alt="Image" caption="JSN-SR04T Waterproof Ultrasonic Range Finder Mode Select Pads." width="50.0%" class="align-center" >}}

In mode 1 the module continuously takes measurements approximately every 100mS and outputs the distance on the TX pin at 9600 baud.
In this mode [Sensor Filters](/components/sensor#sensor-filters) are highly recommended.

In mode 2 the module takes a measurement only when a trigger command of 0x55 is sent to the RX pin on the module.
The module then outputs the distance on its TX pin. The frequency of the measurements can be set with the **update_interval** option.

To use the sensor, first set up an [UART Bus](/components/uart) with a baud rate of 9600 and connect the sensor to the specified pin.

{{< img src="jsn-sr04t-v3.jpg" alt="Image" caption="JSN-SR04T Waterproof Ultrasonic Range Finder." width="70.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: "jsn_sr04t"
    name: "Distance"
    update_interval: 1s
```

## Configuration variables

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`. Not applicable in mode 1.

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the [UART bus](/components/uart) you wish to use for this sensor.
  Use this if you want to use multiple UART buses at once.

- **model** (*Optional*): Sensor model. Available options: `jsn_sr04t` (default) and `aj_sr04m`.
- All other options from [Sensor](/components/sensor).

## See Also

- [UART Bus](/components/uart)
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "jsn_sr04t/jsn_sr04t.h" "jsn_sr04t/jsn_sr04t.h" >}}
