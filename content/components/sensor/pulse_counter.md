---
description: "Instructions for setting up pulse counter sensors."
title: "Pulse Counter Sensor"
params:
  seo:
    description: Instructions for setting up pulse counter sensors.
    image: pulse.svg
---

The pulse counter sensor allows you to count the number of pulses and the frequency of a signal
on any pin.

On the ESP32, this sensor is even highly accurate because it's using the hardware [pulse counterperipheral](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/pcnt.html)
on the ESP32. However, due to the use of the pulse counter peripheral, a maximum of 8 channels can be used!

{{< img src="pulse-counter.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: pulse_counter
    pin: GPIOXX
    name: "Pulse Counter"
```

## Configuration variables

- **pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin to count pulses on.
- **count_mode** (*Optional*): Configure how the counter should behave
  on a detected rising edge/falling edge.

  - **rising_edge** (*Optional*): What to do when a rising edge is
    detected. One of `DISABLE`, `INCREMENT` and `DECREMENT`.
    Defaults to `INCREMENT`.

  - **falling_edge** (*Optional*): What to do when a falling edge is
    detected. One of `DISABLE`, `INCREMENT` and `DECREMENT`.
    Defaults to `DISABLE`.

- **use_pcnt** (*Optional*, boolean): Use hardware `PCNT` pulse counter. Only supported on ESP32. Defaults to `true`.
- **internal_filter** (*Optional*, [Time](/guides/configuration-types#time)): If a pulse shorter than this
  time is detected, it's discarded and no pulse is counted. Defaults to `13us`. On the ESP32, when using the hardware pulse counter
  this value can not be higher than `13us`, for the ESP8266 or with `use_pcnt: false` you can use larger intervals too.
  If you enable this, set up the `count_mode` to increase on the falling edge, not leading edge. For S0 pulse meters that are used to meter power consumption 50-100 ms is a reasonable value.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the sensor. Defaults to `60s`.
- **total** (*Optional*): Report the total number of pulses.
- All other options from [Sensor](/components/sensor).

> [!NOTE]
> See {{< docref "/components/sensor/integration" "integration sensor" >}} for summing up pulse counter
> values over time.

## Converting units

The sensor defaults to measuring its values using a unit of measurement
of “pulses/min”. You can change this by using [Sensor Filters](/components/sensor#sensor-filters).
For example, if you're using the pulse counter with a photodiode to
count the light pulses on a power meter, you can do the following:

```yaml
# Example configuration entry
sensor:
  - platform: pulse_counter
    pin: GPIOXX
    unit_of_measurement: 'kW'
    name: 'Power Meter House'
    filters:
      - multiply: 0.06  # (60s/1000 pulses per kWh)
```

## Counting total pulses

When the total sensor is configured, the pulse_counter also reports the total
number of pulses measured. When used on a power meter, this can be used to
measure the total consumed energy in kWh.

```yaml
# Example configuration entry
sensor:
  - platform: pulse_counter
    pin: GPIOXX
    unit_of_measurement: 'kW'
    name: 'Power Meter House'
    filters:
      - multiply: 0.06  # (60s/1000 pulses per kWh)

    total:
      unit_of_measurement: 'kWh'
      name: 'Energy Meter House'
      filters:
        - multiply: 0.001  # (1/1000 pulses per kWh)
```

## (Re)Setting the total pulse count

Using this action, you are able to reset/set the total pulse count. This can be useful
if you would like the `total` sensor to match what you see on your meter you are
trying to match.

```yaml
# Set pulse counter total from home assistant using this action:
api:
  actions:
    - action: set_pulse_total
      variables:
        new_pulse_total: int
      then:
        - pulse_counter.set_total_pulses:
            id: pulse_counter_id
            value: !lambda 'return new_pulse_total;'
```

> [!NOTE]
> This value is the raw count of pulses, and not the value you see after the filters
> are applied.

## Wiring

If you want to count pulses from a simple reed switch, the simplest way is to make
use of the internal pull-up/pull-down resistors.

You can wire the switch between a GPIO pin and GND; in this case set the pin to input, pullup and inverted:

```yaml
# Reed switch between GPIO and GND
sensor:
  - platform: pulse_counter
    pin:
      number: 12
      inverted: true
      mode:
        input: true
        pullup: true
    name: "Pulse Counter"
```

If you wire it between a GPIO pin and +3.3V, set the pin to input, pulldown:

```yaml
# Reed switch between GPIO and +3.3V
sensor:
  - platform: pulse_counter
    pin:
      number: 12
      mode:
        input: true
        pulldown: true
    name: "Pulse Counter"
```

The safest way is to use GPIO + GND, as this avoids the possibility of short
circuiting the wire by mistake.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "/components/sensor/pulse_meter" >}}
- {{< docref "rotary_encoder/" >}}
- [esp-idf Pulse Counter API](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/pcnt.html).
- {{< apiref "pulse_counter/pulse_counter_sensor.h" "pulse_counter/pulse_counter_sensor.h" >}}
