---
description: "Instructions for setting up sensors that track the total daily energy usage per day and accumulate the power usage."
title: "Total Daily Energy Sensor"
params:
  seo:
    description: Instructions for setting up sensors that track the total daily energy usage per day and accumulate the power usage.
    image: sigma.svg
---

The `total_daily_energy` sensor is a helper sensor that can use the power value of
other sensors like the {{< docref "hlw8012" "HLW8012" >}}, {{< docref "hlw8032" "HLW8032" >}}, {{< docref "cse7766" "CSE7766" >}}, {{< docref "atm90e32" "ATM90E32" >}}, etc and integrate
it over time.

So this component allows you to convert readings in `W` or `kW` to readings of the total
daily energy usage in `Wh` or `kWh`.

```yaml
# Example configuration entry
sensor:
  - platform: total_daily_energy
    name: 'Total Daily Energy'
    power_id: my_power
    unit_of_measurement: 'kWh'
    state_class: total_increasing
    device_class: energy
    accuracy_decimals: 3
    filters:
      # Multiplication factor from W to kW is 0.001
      - multiply: 0.001

  # The power sensor to convert, can be any power sensor
  - platform: hlw8012
    # ...
    power:
      id: my_power

# Enable time component to reset energy at midnight
time:
  - platform: homeassistant
    id: homeassistant_time
```

## Configuration variables

- **power_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the power sensor
  to integrate over time.

- **restore** (*Optional*, boolean): Whether to store the intermediate result on the device so
  that the value can be restored upon power cycle or reboot.
  Defaults to `true`.

- **method** (*Optional*, string): The method to use for calculating the total daily energy. One of
  `trapezoid`, `left` or `right`. Defaults to `right`.

- All other options from [Sensor](/components/sensor).

## Converting from W to kW

Some sensors such as the {{< docref "hlw8012" "HLW8012" >}} expose their power sensor with a unit of measurement of
`W`. To have your readings in `kW`, use a filter:

```yaml
sensor:
  # The power sensor to convert, can be any power sensor
  - platform: hlw8012
    # ...
    power:
      id: my_power
      filters:
        # Multiplication factor from W to kW is 0.001
        - multiply: 0.001
      unit_of_measurement: kW
```

## Lifetime instead of Daily

For a more-generic version of this component which does not reset every midnight, see {{< docref "integration/" >}}, which can provide device-lifetime values instead of daily values with the following example settings:

```yaml
# Example configuration entry
sensor:
  - platform: integration
    name: 'Total Energy'
    sensor: my_power
    time_unit: h
    restore: true
    state_class: total_increasing
    device_class: energy
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "hlw8012/" >}}
- {{< docref "cse7766/" >}}
- {{< docref "integration/" >}}
- {{< docref "/components/sensor/pulse_counter" >}}
- {{< docref "/components/sensor/pulse_meter" >}}
- {{< docref "/components/time/homeassistant" >}}
- {{< docref "/cookbook/power_meter" >}}
- {{< apiref "total_daily_energy/total_daily_energy.h" "total_daily_energy/total_daily_energy.h" >}}
