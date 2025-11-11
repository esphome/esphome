---
description: "Instructions for setting up NTC thermistor sensor in ESPHome"
title: "NTC Sensor"
params:
  seo:
    description: Instructions for setting up NTC thermistor sensor in ESPHome
    image: ntc.jpg
---

The `ntc` platform is a helper sensor that allows you to convert resistance readings
from a NTC thermistor to temperature readings.

First, you need to get resistance readings from the sensor - you can set this up with the
{{< docref "resistance" "resistance" >}} and {{< docref "adc" "adc" >}} sensors.

This platform will then convert the resistance values to temperature readings.
It also requires calibration parameters for this conversion. There are two
ways of obtaining these values: By looking at the datasheet or manual calculation.

If you have the datasheet of the thermistor, you can look at its "B-constant" and
reference temperature/resistance. For example [this product](https://www.adafruit.com/product/372)
would have the following calibration configuration.

```yaml
# Example configuration entry
sensor:
  - platform: ntc
    # ...
    calibration:
      b_constant: 3950
      reference_temperature: 25°C
      reference_resistance: 10kOhm
```

If you don't have access to the datasheet or want to calculate these values yourself,
you have to first measure three resistance values at different temperatures.
Heat/cool the NTC to three different temperatures (best if temperatures are far apart)
and write down the resistance readings at those temperatures. Then enter these values in the
calibration parameter:

```yaml
# Example configuration entry
sensor:
  - platform: ntc
    # ...
    calibration:
      - 10.0kOhm -> 25°C
      - 27.219kOhm -> 0°C
      - 14.674kOhm -> 15°C
```

```yaml
# Example configuration entry
sensor:
  - platform: ntc
    sensor: resistance_sensor
    calibration:
      b_constant: 3950
      reference_temperature: 25°C
      reference_resistance: 10kOhm
    name: NTC Temperature

  # Example source sensors:
  - platform: resistance
    id: resistance_sensor
    sensor: source_sensor
    configuration: DOWNSTREAM
    resistor: 5.6kOhm
    name: Resistance Sensor
  - platform: adc
    id: source_sensor
    pin: A0
```

## Configuration variables

- **sensor** (**Required**, [ID](/guides/configuration-types#id)): The sensor to read the resistance values from
  to convert to temperature readings.

- **calibration** (**Required**, float): The calibration parameters of the sensor - see above
  for more details.

- All other options from [Sensor](/components/sensor).

## Self-Heating

A constant voltage supply to the NTC sensor causes it to heat up and therefore creates unreliable temperature values.
So it's recommended to only supply the NTC sensor (and voltage divider) during the actual measurement.
More info [on Adafruit](https://learn.adafruit.com/thermistor/using-a-thermistor#self-heating-3-22).

To do this, replace the 3.3V side of the voltage divider with a connection to a GPIO pin. This GPIO pin will
be switched HIGH (3.3V) only during the measurement, thus preventing the sensor from heating up.
In the example below, the pin `D0` is the "top" side of the voltage divider:

```yaml
sensor:
  # Same as before:
  - platform: ntc
    sensor: resistance_sensor
    # ...

  - platform: adc
    pin: A0
    id: source_sensor
    # Added:
    update_interval: never

switch:
  - platform: gpio
    pin: D0
    id: ntc_vcc

interval:
  - interval: 60s
    then:
      - switch.turn_on: ntc_vcc
      - component.update: source_sensor
      - switch.turn_off: ntc_vcc
```

## See Also

- {{< docref "adc/" >}}
- {{< docref "resistance/" >}}
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "ntc/ntc.h" "ntc/ntc.h" >}}
