---
description: "Instructions for setting up an Atlas Scientific Peristaltic Pump in ESPHome"
title: "Atlas Scientific Peristaltic Pump"
params:
  seo:
    description: Instructions for setting up an Atlas Scientific Peristaltic Pump in ESPHome
    image: ezo-pmp.jpg
---

The `ezo_pmp` component allows you to use an Atlas Scientific Peristaltic Pump with ESPHome.
Both the EZO-PMP ([datasheet](https://files.atlas-scientific.com/EZO_PMP_Datasheet.pdf))
and EZO-PMP-L ([datasheet](https://files.atlas-scientific.com/EZO_PMP_L_Datasheet.pdf)) are supported.
The [I²C Bus](/components/i2c) is required to be set up in your configuration for this sensor to work.

> [!NOTE]
> This component will not be directly controllable in the Home Assistant front-end automatically because
> Home Assistant doesn't have support for pumps. In order to control the pump from the frontend you will need to use
> templates to offer access to the actions you need. Please see [Home Assistant Configuration](#ezo-pmp-ha-config).

{{< img src="ezo-pmp.jpg" alt="Image" width="80.0%" class="align-center" >}}

## Component/Hub

```yaml
ezo_pmp:
  id: ezo_pmp
  address: 103  # Default Address for the EZO-PMP.
  update_interval: 60s
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): Specify the ID of the pump so that you can control it.
- **address** (*Optional*, int): Specify the I²C address of the sensor. Defaults to 103.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

## Sensor

Since the EZO-PMP offers a large number of sensors and each sensor needs to be polled individually, the code has been
optimized not to update a sensor that is not defined in the config. If you need very quick updates from the pump (under 3 seconds)
only enable the sensors that you actually need.

{{< anchor "ezo_pmp-current_volume_dosed_sensor" >}}

### `current_volume_dosed`

This sensor indicates the volume (in milliliters) that has been dosed in the currently running or last ran dosing command.

```yaml
sensor:
  - platform: ezo_pmp
    current_volume_dosed:
      id: current_volume_dosed
      name: Current Volume Dosed
```

#### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

{{< anchor "ezo_pmp-total_volume_dosed_sensor" >}}

### `total_volume_dosed`

This sensor indicates total volume (in milliliters) that has been dosed since the last time the pump was turned on. Could be a
negative number if the pump has been run in reverse.

```yaml
sensor:
  - platform: ezo_pmp
    total_volume_dosed:
      id: total_volume_dosed
      name: Total Volume Dosed
```

#### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

{{< anchor "ezo_pmp-absolute_total_volume_dosed_sensor" >}}

### `absolute_total_volume_dosed`

This sensor indicates the absolute total volume (in milliliters) that has been dosed since the last time the pump was turned on.

```yaml
sensor:
  - platform: ezo_pmp
    absolute_total_volume_dosed:
      id: absolute_total_volume_dosed
      name: Absolute Total Volume Dosed
```

#### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

{{< anchor "ezo_pmp-last_volume_requested_sensor" >}}

### `last_volume_requested`

Indicates the total volume (in milliliters) that is being dosed (or was if the pump has already finished) by a dose Action. When
using dosing actions that have a duration, the sensor will have the right calculation for total volume.

```yaml
sensor:
  - platform: ezo_pmp
    last_volume_requested:
      id: last_volume_requested
      name: Last Volume Requested
```

#### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

{{< anchor "ezo_pmp-max_flow_rate_sensor" >}}

### `max_flow_rate`

The pump provides its own calculation of the maximum flow rate it can provide (in ml/minute). Dosing requests that exceed this rate
will fail. When using the [Dose Continuously](#ezo_pmp-dose_continuously_action) Action, this is the volume the pump will
dose every minute. This value will get updated after the pump is calibrated (see [`ezo_pmp.set_calibration_volume` Action](#ezo_pmp-set_calibration_volume_action)).

```yaml
sensor:
  - platform: ezo_pmp
    max_flow_rate:
      id: max_flow_rate
      name: Max Flow Rate
```

#### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Set the ID of this sensor for use in lambdas.
- All other options from [Sensor](/components/sensor).

{{< anchor "ezo_pmp-pump_voltage_sensor" >}}

### `pump_voltage`

The current voltage of the power supply that powers the pump. Not to be confused with the voltage that powers the electronics on the pump.

```yaml
sensor:
  - platform: ezo_pmp
    pump_voltage:
      id: pump_voltage
      name: Pump Voltage
```

#### Configuration variables

- All options from [Sensor](/components/sensor).

## Binary Sensor

{{< anchor "ezo_pmp-pump_state_binary_sensor" >}}

### `pump_state`

Indicates if the pump is currently running or not.

```yaml
binary_sensor:
  - platform: ezo_pmp
    pump_state:
      id: pump_state
      name: Pump State
```

#### Configuration variables

- All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

{{< anchor "ezo_pmp-is_paused_binary_sensor" >}}

### `is_paused`

Indicates if a dosing action is currently paused.

```yaml
binary_sensor:
  - platform: ezo_pmp
    is_paused:
      id: is_paused
      name: Is Paused
```

#### Configuration variables

- All options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## Text Sensor

{{< anchor "ezo_pmp-dosing_mode_text_sensor" >}}

### `dosing_mode`

Indicates the dosing mode the pump is currently running as. Can be any of `Volume`, `Volume/Time`, `Constant Flow Rate`, `Continuous`, `None`.

```yaml
text_sensor:
  - platform: ezo_pmp
    dosing_mode:
      id: dosing_mode
      name: Dosing Mode
```

#### Configuration variables

- All options from [Text Sensor](/components/text_sensor#config-text_sensor).

{{< anchor "ezo_pmp-calibration_status_text_sensor" >}}

### `calibration_status`

Indicates calibration status of the pump. Can be any of `Fixed Volume`, `Volume/Time`, `Fixed Volume & Volume/Time` or `Uncalibrated`.

```yaml
text_sensor:
  - platform: ezo_pmp
    calibration_status:
      id: calibration_status
      name: Calibration Status
```

#### Configuration variables

- All options from [Text Sensor](/components/text_sensor#config-text_sensor).

## Actions

{{< anchor "ezo_pmp-dose_continuously_action" >}}

### `ezo_pmp.dose_continuously` Action

Use this action in an [automations](/automations) to have the peristaltic pump dose continuously
at the [Maximum Flow Rate](#ezo_pmp-max_flow_rate_sensor). The pump will automatically stop after 20 days
of running in continuous mode.

```yaml
on_...:
  then:
  - ezo_pmp.find:
      id: ezo_pmp
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.

{{< anchor "ezo_pmp-dose_volume_action" >}}

### `ezo_pmp.dose_volume` Action

Use this action in an [automations](/automations) to have the peristaltic pump dose an specific volume (in milliliters)
at the [Maximum Flow Rate](#ezo_pmp-max_flow_rate_sensor). If the volume is negative the pump will run backwards.

```yaml
on_...:
  then:
  - ezo_pmp.dose_volume:
      id: ezo_pmp
      volume: 10

  # Templated
  - ezo_pmp.dose_volume:
      id: ezo_pmp
      volume: !lambda |-
        return id(some_volume_sensor).state;
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.
- **volume** (**Required**, float, [templatable](/automations/templates)): The volume to dose in milliliters. If negative, pump will run in reverse.

{{< anchor "ezo_pmp-dose_volume_over_time_action" >}}

### `ezo_pmp.dose_volume_over_time` Action

Use this action in an [automations](/automations) to have the peristaltic pump dose an specific `volume` (in milliliters)
over the provided `duration` (in minutes). At the end of the time period the pump will have dosed the specified `volume`.
If the volume is negative the pump will run backwards.

```yaml
on_...:
  then:
  - ezo_pmp.dose_volume_over_time:
      id: ezo_pmp
      volume: 23.4
      duration: 2

  # Templated
  - ezo_pmp.dose_volume_over_time:
      id: ezo_pmp
      volume: !lambda |-
        return id(some_volume_sensor).state;
      duration: !lambda |-
        return id(some_duration_sensor).state;
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.
- **volume** (**Required**, float, [templatable](/automations/templates)): The volume to dose in milliliters. If negative, pump will run in reverse.
- **duration** (**Required**, int, [templatable](/automations/templates)): The time (in minutes) the pump will take to dose the volume requested.

{{< anchor "ezo_pmp-dose_with_constant_flow_rate_action" >}}

### `ezo_pmp.dose_with_constant_flow_rate` Action

Use this action in an [automations](/automations) to have the peristaltic pump dose an specific `volume` (in milliliters) every minute
for the provided `duration` (in minutes). At the end of the time period the pump will have dosed the specified `volume` times the `duration`.
If the volume is negative the pump will run backwards.

```yaml
on_...:
  then:
  - ezo_pmp.dose_with_constant_flow_rate:
      id: ezo_pmp
      volume_per_minute: 10.2
      duration: 2

  # Templated
  - ezo_pmp.dose_with_constant_flow_rate:
      id: ezo_pmp
      volume_per_minute: !lambda |-
        return id(some_volume_sensor).state;
      duration: !lambda |-
        return id(some_duration_sensor).state;
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.
- **volume_per_minute** (**Required**, float, [templatable](/automations/templates)): The volume to dose in milliliters every minute. If negative, pump will run in reverse.
- **duration** (**Required**, int, [templatable](/automations/templates)): The time (in minutes) the pump will dose the volume requested every minute.

{{< anchor "ezo_pmp-pause_dosing_action" >}}

### `ezo_pmp.pause_dosing` Action

Use this action to pause a Dosing command that was previously issued. To determine if the dosing is paused or not, you can use the [Is Paused](#ezo_pmp-is_paused_binary_sensor) sensor.
If the pump is currently paused, issuing this action again will unpause it.

```yaml
on_...:
  then:
  - ezo_pmp.pause_dosing:
      id: ezo_pmp
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.

{{< anchor "ezo_pmp-stop_dosing_action" >}}

### `ezo_pmp.stop_dosing` Action

Use this action to stop the current Dosing command.

```yaml
on_...:
  then:
  - ezo_pmp.stop_dosing:
      id: ezo_pmp
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.

{{< anchor "ezo_pmp-clear_total_volume_dosed_action" >}}

### `ezo_pmp.clear_total_volume_dosed` Action

Clear the values of the [Current Volume Dosed](#ezo_pmp-current_volume_dosed_sensor), [Total Volume Dosed](#ezo_pmp-total_volume_dosed_sensor)
and [Absolute Total Volume Dosed](#ezo_pmp-absolute_total_volume_dosed_sensor) sensors.

```yaml
on_...:
  then:
  - ezo_pmp.clear_total_volume_dosed:
      id: ezo_pmp
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.

{{< anchor "ezo_pmp-set_calibration_volume_action" >}}

### `ezo_pmp.set_calibration_volume` Action

Use this action to calibrate the peristaltic pump. The EZO-PMP needs two forms of calibration: absolute volume and volume over time. You can check
the calibration status by using the [Calibration Status](#ezo_pmp-calibration_status_text_sensor) sensor. For the procedure on calibrating the pump
check the datasheet.

```yaml
on_...:
  then:
  - ezo_pmp.set_calibration_volume:
      id: ezo_pmp
      volume: 10.01

  # Templated
  - ezo_pmp.set_calibration_volume:
      id: ezo_pmp
      volume: !lambda |-
        return id(some_volume_sensor).state;
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.
- **volume** (**Required**, float, [templatable](/automations/templates)): The volume measured as part of the calibration process.

{{< anchor "ezo_pmp-clear_calibration_action" >}}

### `ezo_pmp.clear_calibration` Action

Clear the calibration values stored in the pump. You can check the calibration status by using the
[Calibration Status](#ezo_pmp-calibration_status_text_sensor) sensor.

```yaml
on_...:
  then:
  - ezo_pmp.clear_calibration:
      id: ezo_pmp
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.

{{< anchor "ezo_pmp-find_action" >}}

### `ezo_pmp.find` Action

Use this action to make the LED on the Pump control board to blink for a minute. The pump will not respond to any other action while the LED is blinking.

```yaml
on_...:
  then:
  - ezo_pmp.find:
      id: ezo_pmp
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.

{{< anchor "ezo_pmp-change_i2c_address_action" >}}

### `ezo_pmp.change_i2c_address` Action

Changes the i2c address of the pump to the provided value. After the address is changed you must upload a new version of the ESPHome firmware with the updated I2C
address for the pump to work.

```yaml
on_...:
  then:
  - ezo_pmp.change_i2c_address:
      id: ezo_pmp
      address: 100
```

#### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the pump.
- **address** (**Required**, int, [templatable](/automations/templates)): The new I2C address for the pump.

{{< anchor "ezo-pmp-ha-config" >}}

## Home Assistant Configuration

In order to provide control of the pump from the home assistant frontend it is important to expose the actions
as components that have UI rendering. This could be done using templates in ESPHome. Here is an example using a
`Template Button` and `Template Number` to dose a certain volume over time.

```yaml
i2c:

ezo_pmp:
  id: ezo_pmp

number:
  - platform: template
    id: volume
    name: "Volume"
    optimistic: true
    min_value: 0
    max_value: 100
    step: 0.01
  - platform: template
    id: duration
    name: "Duration"
    optimistic: true
    min_value: 0
    max_value: 100
    step: 1

button:
  - platform: template
    name: "Dose Over Time"
    on_press:
      - ezo_pmp.dose_volume_over_time:
          id: ezo_pump
          volume: !lambda |-
                  return id(volume).state;
          duration: !lambda |-
                  return id(duration).state;
```

{{< anchor "ezo_pmp-lambda_calls" >}}

## lambda calls

From [lambdas](/automations/templates#config-lambda), you can also access the actions on the peristaltic pump to do some
advanced stuff (see the full API Reference for more info). The name of the functions is the same as the name
of the actions in YAML config.

## See Also

- {{< apiref "ezo_pmp/ezo_pmp.h" "ezo_pmp/ezo_pmp.h" >}}
