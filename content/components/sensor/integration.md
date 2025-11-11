---
description: "Instructions for setting up sensors that integrate values over time."
title: "Integration Sensor"
params:
  seo:
    description: Instructions for setting up sensors that integrate values over time.
    image: sigma.svg
---

The `integration` sensor is a helper sensor that can integrate values from other sensors over
time. This can for example be useful to integrate the values of a water flow sensor (in m^3/s) over
time (result is in m^3).

This component can be considered a more-generic version of the {{< docref "total_daily_energy/" >}}.

```yaml
# Example configuration entry
sensor:
  - platform: integration
    name: "Total Daily Energy"
    sensor: my_flow_meter
    time_unit: min

  # The sensor to integrate, can be any power sensor
  - platform: pulse_counter
    # ...
    id: my_flow_meter
```

## Configuration variables

- **sensor** (**Required**, [ID](/guides/configuration-types#id)): The ID of the sensor to integrate over time.
- **time_unit** (**Required**, string): The time unit to integrate with, one of
  `ms`, `s`, `min`, `h` or `d`.

- **integration_method** (*Optional*, string): The integration method to use. One of
  `trapezoid`, `left` or `right`. Defaults to `trapezoid`.

- **restore** (*Optional*, boolean): Whether to store the intermediate result on the device so
  that the value can be restored upon power cycle or reboot.
  Warning: this option can wear out your flash. Defaults to `false`.

- All other options from [Sensor](/components/sensor).

{{< anchor "sensor-integration-reset_action" >}}

## `sensor.integration.reset` Action

This [Action](/automations/actions#all-actions) allows you to reset the value of the integration sensor
to zero. For example this can be used to reset the integration sensor to zero at midnight with
a time-based automation.

```yaml
on_...:
  - sensor.integration.reset:  my_integration_sensor
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "total_daily_energy/" >}}
- {{< docref "/cookbook/power_meter" >}}
- {{< apiref "integration/integration_sensor.h" "integration/integration_sensor.h" >}}
