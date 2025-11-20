---
description: "Instructions for setting up HC8 CO2 sensors"
title: "HC8 CO₂ Sensor"
params:
  seo:
    description: Instructions for setting up HC8 CO2 sensors
    image: hc8.png
---

The `hc8` sensor platform allows you to use HC8 CO₂ sensors.

{{< img src="hc8-full.png" alt="HC8 CO₂ Sensor" caption="HC8 CO₂ Sensor." width="50.0%" class="align-center" >}}

As the communication with the HC8 sensor is done using UART, you need
to have an [UART bus](/components/uart) in your configuration with the `rx_pin` connected to the TX pin of the
HC8 and the `tx_pin` connected to the RX Pin of the HC8 (it's switched because the
TX/RX labels are from the perspective of the HC8). Additionally, you need to set the baud rate to 9600.

```yaml
# Example configuration entry
sensor:
  - platform: hc8
    co2:
      name: HC8 CO2 Value
```

## Configuration Variables

- **co2** (*Optional*): The CO₂ data from the sensor in parts per million (ppm).
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for actions.

- **warmup_time** (*Optional*, [Time](/guides/configuration-types#time)): The sensor has a warmup period during which it returns inaccurate readings (e.g., 500ppm, 505ppm). This setting discards readings until the warmup time has elapsed (returning `NaN` during warmup). The datasheet specifies a 10-minute preheating time for full accuracy, with 90% accuracy achieved after 3 minutes. Empirical evidence shows that reasonable values are usually returned after about 1 minute. Defaults to `75s`.

{{< anchor "hc8-calibrate_action" >}}

## `hc8.calibrate` Action

This [action](/automations/actions#all-actions) executes baseline calibration command on the sensor with the given ID.

Before executing baseline calibration, ensure the HC8 sensor has been operating in a stable gas environment
(with known CO₂ concentration) for at least 2 minutes.

**Warning:** Only calibrate the sensor in a known stable environment (e.g., outdoors or in a well-ventilated room).
Incorrect calibration will result in inaccurate readings.

```yaml
on_...:
  then:
    - hc8.calibrate:
        id: my_hc8_id
        baseline: 420  # Current outdoor CO₂ level
```

You can provide an [action](/components/api#api-device-actions) to perform from Home Assistant

```yaml
api:
  actions:
    - action: hc8_calibrate
      variables:
        my_baseline: int
      then:
        - hc8.calibrate:
            id: my_hc8_id
            baseline: !lambda 'return my_baseline;'
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "hc8/hc8.h" "hc8/hc8.h" >}}
