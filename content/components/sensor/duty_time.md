---
description: "Instructions for setting up a sensor that tracks the duty time of some object."
title: "Duty Time"
params:
  seo:
    description: Instructions for setting up a sensor that tracks the duty time of some object.
    image: timer-play-outline.svg
---

The `duty_time` sensor allows you to track the total duty time of some object, for example, a light bulb, in seconds.
Able to calculate the last turn-on time when an optional sensor `last_time` is included in the configuration.

Supports boolean signal sources: `binary_sensor` or `lambda` that returns a boolean state of the tracked object.
As an alternative to controlling a component in automations, may be used the `sensor.duty_time.start` and `sensor.duty_time.stop` actions.

```yaml
# Example configuration entry
sensor:
  - platform: duty_time
    id: my_climate_work_time
    name: My Climate Work Time
    # Support logical sources (optional): 'binary_sensor'
    sensor: my_binary_sensor
    # ... EOR 'lambda'
    lambda: "return id(my_climate).mode != CLIMATE_MODE_OFF;"
    # Restore (optional, default: False)
    restore: false
    # Sensor for last turn-on time (optional)
    last_time:
      name: My Climate Last Turn-On Time
```

## Configuration variables

- **sensor** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the `binary_sensor` to track the duty time. *May not be
  used with* `lambda`.

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): Lambda that will be called in a loop to get the current
  state of the tracked object. *May not be used with* `sensor`.

- **last_time** (*Optional*): Information of the last switch-on time sensor.
  All options from [Sensor](/components/sensor).

- **restore** (*Optional*, boolean): Whether to store the intermediate result on the device so that the value can be
  restored upon power cycle or reboot.
  Warning: this option can wear out your flash. Defaults to `false`.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The update interval. Defaults to `60s`.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

## Automations

In addition to all basic [sensor automations](/components/sensor#sensor-automations), the component supports the automations below.

{{< anchor "sensor-duty_time-start_action" >}}

### `sensor.duty_time.start` Action

This action starts/resume time tracking. In lambdas, you may use the `start()` method.

```yaml
on_...:
  then:
    - sensor.duty_time.start: my_climate_work_time
```

{{< anchor "sensor-duty_time-stop_action" >}}

### `sensor.duty_time.stop` Action

This action suspends time tracking. Causes the sensor to be updated, including the `last_time` sensor. In lambdas, you may use the `stop()` method.

```yaml
on_...:
  then:
    - sensor.duty_time.stop: my_climate_work_time
```

{{< anchor "sensor-duty_time-reset_action" >}}

### `sensor.duty_time.reset` Action

This action resets the duty time counter. Causes a sensor update. Does not affect the `last_time` sensor. In lambdas, you may use the `reset()` method.

```yaml
on_...:
  then:
    - sensor.duty_time.reset: my_climate_work_time
```

{{< anchor "sensor-duty_time-is_running_action" >}}
{{< anchor "sensor-duty_time-is_not_running_action" >}}

### `sensor.duty_time.is_running` / `sensor.duty_time.is_not_running` Condition

This [Condition](/automations/actions#all-conditions) checks if the `duty_time` counter is currently running (or suspended). In lambdas, you may use the `is_running()` method.

```yaml
# In some trigger:
on_...:
  if:
    condition:
      # Same syntax for 'is_not_running'
      sensor.duty_time.is_running: my_climate_work_time
```

## See Also

- [Base Sensor Configuration](/components/sensor)
- [Templates](/automations/templates#config-lambda)
- [Automation](/automations)
- {{< docref "/components/binary_sensor" >}}
- {{< apiref "duty_time/duty_time_sensor.h" "duty_time/duty_time_sensor.h" >}}
