---
description: ""
headless: true
---

Convert your sensor values available as resistance values into corresponding
temperatures using an NTC characteristic curve.

Configuration variables:

- **calibration** (**Required**): calibration data.

A resistance/temperature characteristic curve is required to use this filter.
This can be taken from a corresponding diagram in a data sheet. If you do not
have access to the data sheet or want to calculate these values yourself, you
must first measure three resistance values at different temperatures.
Heat or cool the NTC to three different temperatures (preferably widely separated
temperatures) and note the resistance values at these temperatures.
Then enter these values in the calibration parameter:

```yaml
# Example configuration entry
- platform: template
  id: to_ntc_temperature_sensor1
  unit_of_measurement: "°C"
  lambda: |-
    return id(some_sensor).state;
  update_interval: 1s
  filters:
    - to_ntc_temperature:
        calibration:
          - 10.0kOhm -> 25°C
          - 27.219kOhm -> 0°C
          - 14.674kOhm -> 15°C
```

The filter determines coefficients for the [Steinhart-Hart](https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation) equation from the specified
pairs of values which can also be specified directly as an alternative.

```yaml
# Example configuration entry
- platform: template
  id: to_ntc_temperature_sensor2
  unit_of_measurement: "°C"
  lambda: |-
    return id(some_sensor).state;
  update_interval: 1s
  filters:
    - to_ntc_temperature:
        calibration:
          a: 1.439114856904070E-03
          b: 2.693066430764570E-04
          c: 1.653440958554570E-07
```
