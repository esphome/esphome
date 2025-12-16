---
description: "Instructions for setting up an analog threshold binary sensors."
title: "Analog Threshold Binary Sensor"
params:
  seo:
    description: Instructions for setting up an analog threshold binary sensors.
    image: analog_threshold.svg
---

The `analog_threshold` binary sensor platform allows you to convert analog values
(i.e. {{< docref "/components/sensor/index" "sensor" >}} readings)
into boolean values, using a threshold as a reference.
When the signal is above or equal to the threshold the binary sensor is `true`
(this behavior can be changed by adding an `invert` filter).

It provides an *hysteresis* option to reduce instability when the source signal is noisy
using different limits depending on the current state.
Additionally a [delay filter](/components/binary_sensor#binary_sensor-filters) could be used to only change
after a new state has been kept a minimum time.

If the source sensor is uninitialized at the moment of component creation, the initial
state of the binary sensor wil be `false`, if later it has some reading errors, those
invalid source updates will be ignored, and the binary sensor will keep it´s last state.

For example, below configuration would turn the readings of current sensor into
a binary sensor.

```yaml
# Example configuration entry
binary_sensor:
  - platform: analog_threshold
    name: "Garage Door Opening"
    sensor_id: motor_current_sensor
    threshold: 0.5
```

Alternatively, you can achieve similar functionality using a
[Template Binary Sensor](/components/binary_sensor/template) with the `condition` option:

```yaml
# Alternative using template binary sensor
binary_sensor:
  - platform: template
    name: "Engine Running"
    condition:
      sensor.in_range:
        id: motor_current_sensor
        above: 0.5
```

> [!NOTE]
> The template approach does not support hysteresis. Use `analog_threshold` if you need
> different upper and lower thresholds to reduce noise.

## Configuration variables

- **sensor_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the source sensor.
- **threshold** (**Required**, float [templatable](/automations/templates) or mapping): Configures the reference for comparison. Accepts either a shorthand
   float number that will be used as both upper/lower threshold, or a mapping to define different values for each (to
   use hysteresis).

  - **upper** (**Required**, float [templatable](/automations/templates)): Upper threshold, that needs to be crossed to transition from `low` to `high` states.
  - **lower** (**Required**, float [templatable](/automations/templates)): Lower threshold, that needs to be crossed to transition from `high` to `low` states.
- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## See Also

- {{< docref "/components/binary_sensor" >}}
- {{< docref "/components/sensor" >}}
- [Automation](/automations)
- {{< apiref "analog_threshold/analog_threshold_binary_sensor.h" "analog_threshold/analog_threshold_binary_sensor.h" >}}
