---
description: "Instructions for setting up a Binary Sensor Map"
title: "Binary Sensor Map"
params:
  seo:
    description: Instructions for setting up a Binary Sensor Map
    image: binary_sensor_map.jpg
---

The `binary_sensor_map` sensor platform allows you to map multiple {{< docref "/components/binary_sensor/index" "binary sensor" >}}
to an individual value. Depending on the state of each binary sensor, its associated configured parameters, and this sensor's mapping type,
the `binary_sensor_map` publishes a single numerical value.

Use this sensor to combine one or more binary sensors' `ON` or `OFF` states into a numerical value. Some possible use cases include
touch devices and determining Bayesian probabilities for an event.

This platform supports three measurement types: `BAYESIAN`, `GROUP`, and `SUM`.
You need to specify your desired mapping with the `type:` configuration value.

When using the `BAYESIAN` type, add your binary sensors as `observations` to the binary sensor map.
If you use the `GROUP` or `SUM` type, add your binary sensors as `channels`.
The maximum amount of observations/channels supported is 64.

- `BAYESIAN` This type replicates Home Assistant's [Bayesian sensor](https://www.home-assistant.io/integrations/bayesian/). Based on the observation states, this sensor returns the Bayesian probability of a particular event occurring. The configured `prior:` probability is the likelihood that the Bayesian event is true, ignoring all external influences. Every observation has its own `prob_given_true` and `prob_given_false` parameters. The `prob_given_true:` value is the probability that the observation's binary sensor is `ON` when the Bayesian event is `true`. The `prob_given_false:` value is the probability that the observation's binary sensor is `ON` when the Bayesian event is `false`. Use an {{< docref "/components/binary_sensor/analog_threshold" >}} to convert this sensor's probability to a binary `ON` or `OFF` by setting an appropriate threshold.

```yaml
# Example configuration entry
sensor:
  - platform: binary_sensor_map
    id: bayesian_prob
    name: 'Bayesian Event Probability'
    type: bayesian
    prior: 0.4
    observations:
      - binary_sensor: binary_sensor_0
        prob_given_true: 0.9
        prob_given_false: 0.2
      - binary_sensor: binary_sensor_1
        prob_given_true: 0.6
        prob_given_false: 0.1

binary_sensor:
  # If the Bayesian probability is greater than 0.6,
  # then predict the event is occurring
  - platform: analog_threshold
    name: "Bayesian Event Predicted State"
    sensor_id: bayesian_prob
    threshold: 0.6
  # ...
```

- `GROUP` Each channel has its own `value`. The sensor publishes the average value of all active
  binary sensors or `NAN` if no sensors are active.

```yaml
# Example configuration entry
sensor:
  - platform: binary_sensor_map
    id: group_0
    name: 'Group Map 0'
    type: GROUP
    channels:
      - binary_sensor: touchkey0
        value: 0
      - binary_sensor: touchkey1
        value: 10
      - binary_sensor: touchkey2
        value: 20
      - binary_sensor: touchkey3
        value: 30

# Example binary sensors using MPR121 component
mpr121:
  id: mpr121_first
  address: 0x5A

binary_sensor:
  - platform: mpr121
    channel: 0
    id: touchkey0
  # ...
```

- `SUM` Each channel has its own `value`. The sensor publishes the sum of all the active
  binary sensors values or `0` if no sensors are active.

```yaml
# Example configuration entry
sensor:
  - platform: binary_sensor_map
    id: group_0
    name: 'Group Map 0'
    type: sum
    channels:
      - binary_sensor: bit0
        value: 1
      - binary_sensor: bit1
        value: 2
      - binary_sensor: bit2
        value: 4
      - binary_sensor: bit3
        value: 8

binary_sensor:
  - platform: gpio
    pin: GPIOXX
    id: bit0

  - platform: gpio
    pin: GPIOXX
    id: bit1

  - platform: gpio
    pin: GPIOXX
    id: bit2

  - platform: gpio
    pin: GPIOXX
    id: bit3
  # ...
```

## Configuration variables

- **type** (**Required**, string): The sensor type. Should be one of: `BAYESIAN`, `GROUP`, or `SUM`.
- **channels** (**Required for GROUP or SUM types**): A list of channels that are mapped to certain values.

  - **binary_sensor** (**Required**): The id of the {{< docref "/components/binary_sensor/index" "binary sensor" >}}
    to add as a channel for this sensor.

  - **value** (**Required**): The value this channel should report when its binary sensor is active.
- **prior** (**Required for BAYESIAN type**, float between 0 and 1): The prior probability of the event.
- **observations** (**Required for BAYESIAN type**): A list of observations that influence the Bayesian probability of the event.

  - **binary_sensor** (**Required**): The id of the {{< docref "/components/binary_sensor/index" "binary sensor" >}}
    to add as an observation.

  - **prob_given_true** (**Required**, float between 0 and 1): Assuming the event is true, the probability this observation is on.
  - **prob_given_false** (**Required**, float between 0 and 1): Assuming the event is false, the probability this observation is on.

- All other options from [Sensor](/components/sensor).

## See Also

- {{< docref "/components/binary_sensor/mpr121" >}}
- {{< docref "/components/binary_sensor/analog_threshold" >}}
- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "binary_sensor_map/binary_sensor_map.h" "binary_sensor_map/binary_sensor_map.h" >}}
- [Bayesian sensor in Home Assistant](https://www.home-assistant.io/integrations/bayesian/)
