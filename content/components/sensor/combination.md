---
description: "Instructions for setting up a combination sensor"
title: "Combine the state of several sensors"
params:
  seo:
    description: Instructions for setting up a combination sensor
---

The `combination` sensor platform allows you to combine the state of several
sensors into one. To use this sensor, specify the combination type and add your source sensors.

The `unit_of_measurement`, `device_class`, `entity_category`, `icon`, and
`accuracy_decimals` properties are by default inherited from the first sensor.
`state_class` is explicitly not inherited, because `total_increasing` states
could still decrease when multiple sensors are used for several of the combination types.

The source sensor states can be combined in several ways:

- `KALMAN` filter: This type filters one or several
  sensors into one with a reduced error. If using a single sensor as data source,
  it acts like a [sensor-filter-exponential_moving_average](/components/sensor/filter/sliding_window_moving_average#sensor-filter-exponential_moving_average) filter. With
  multiple sensors, it combines their values based on their respective standard
  deviation.

```yaml
# Example configuration entry
sensor:
  - platform: combination
    type: kalman
    name: "Temperature"
    process_std_dev: 0.001
    sources:
      - source: temperature_sensor_1
        error: 1.0
      - source: temperature_sensor_2
        error: !lambda |-
          return 0.5 + std::abs(x - 25) * 0.023
```

- `LINEAR` combination: This type sums all source sensors after multiplying each by
  a configured coeffecient.

```yaml
# Example configuration entry
sensor:
  - platform: combination
    type: linear
    name: "Balance Power"
    sources:
      - source: total_power
        coeffecient: 1.0
      - source: circuit_1_power
        coeffecient: -1.0
```

- `MAXIMUM`, `MEAN`, `MEDIAN`, `MINIMUM`, `MOST_RECENTLY_UPDATED`,
  `RANGE`, `SUM` combinations: These types compute the specified combination among
  all source sensor states.

```yaml
# Example configuration entry
sensor:
  - platform: combination
    type: median
    name: "Median Temperature"
    sources:
      - source: temperature_sensor_1
      - source: temperature_sensor_2
      - source: temperature_sensor_3
```

## Configuration variables

- **type** (**Required**, enum): Combination statistic type, should be one of
  `KALMAN`, `LINEAR`, `MAXIMUM`, `MEAN`, `MEDIAN`, `MINIMUM`,
  `MOST_RECENTLY_UPDATED`, `RANGE`, or `SUM`.

- **sources** (**Required**, list): A list of sensors to use as source.

  - **source** (**Required**, [ID](/guides/configuration-types#id) of a {{< docref "/components/sensor" >}}): The
    sensor id that is used as sample source.

  - **error** (**Required**, only for `KALMAN` type, float, [templatable](/automations/templates)):
    The standard deviation of the sensor's measurements. This works like the `process_std_dev`
    parameter, with low values marking accurate data. If implemented as a template, the
    measurement is in parameter `x`.

  - **coeffecient** (**Required**, only for `LINEAR` type, float, [templatable](/automations/templates)):
    The coeffecient to multiply the sensor's state by before summing all source sensor states.
    If implemented as a template, the measurement is in parameter `x`.

- **process_std_dev** (**Required**, only for `KALMAN` type, float): The standard deviation of the
  measurement's change per second (e.g. `1/3600 = 0.000277` if the
  temperature usually changes at most by one Kelvin per hour). A low value here
  will place high importance on the current state and be slow to respond to
  changes in the measured samples. A high value will update faster, but also be
  more noisy.

- **std_dev** (*Optional*, only for `KALMAN` type, [Sensor](/components/sensor)): A sensor
  that publishes the current standard deviation of the state with each update.

- All other options from [Sensor](/components/sensor).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "combination/combination.h" "combination/combination.h" >}}
