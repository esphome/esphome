---
description: "Instructions for setting up a Tuya climate device."
title: "Tuya Climate"
params:
  seo:
    description: Instructions for setting up a Tuya climate device.
    image: air-conditioner.svg
---

The `tuya` climate platform creates a climate device from a tuya component.

Tuya climate requires a {{< docref "/components/tuya" >}} to be configured.

```text
[11:45:14][C][tuya:041]: Tuya:
[11:45:14][C][tuya:056]:   Datapoint 1: switch (value: OFF)
[11:45:14][C][tuya:058]:   Datapoint 2: int value (value: 65)
[11:45:14][C][tuya:058]:   Datapoint 3: int value (value: 54)
[11:45:14][C][tuya:062]:   Datapoint 4: enum (value: 1)
[11:45:14][C][tuya:056]:   Datapoint 5: switch (value: OFF)
[11:45:14][C][tuya:056]:   Datapoint 6: switch (value: OFF)
[11:45:14][C][tuya:062]:   Datapoint 102: enum (value: 0)
[11:45:14][C][tuya:062]:   Datapoint 103: enum (value: 1)
[11:45:14][C][tuya:074]:   Product: 'N8bUqOZ8HBQjU0K02.0.1'
```

On this controller (BAC-002-ELW), the data points are:

- 1 represents the climate on/off state.
- 2 represents the target temperature.
- 3 represents the current temperature.
- 4 represents the schedule mode but is not yet available to be used in ESPHome.
- 5 represents the ECO mode switch.
- 6 represents the child lock switch. (use the {{< docref "/components/switch/tuya" >}} component to control this)
- 102 represents the HVAC mode (heating, cooling, fan-only, etc.).
- 103 represents the fan speed (auto, low, medium, high, etc.).

Based on this, you can create the climate device as follows:

```yaml
climate:
  - platform: tuya
    name: "My Climate Device"
    switch_datapoint: 1
    target_temperature_datapoint: 2
    current_temperature_datapoint: 3
    supports_heat: true
    supports_cool: true
    active_state:
      datapoint: 102
      cooling_value: 0
      heating_value: 1
      fanonly_value: 2
    fan_mode:
      datapoint: 103
      auto_value: 0
      high_value: 1
      medium_value: 2
      low_value: 3
    preset:
      eco:
        datapoint: 5
        temperature: 28
```

## Configuration variables

- **supports_heat** (*Optional*, boolean): Specifies if the device has a heating mode. Defaults to `true`.
- **supports_cool** (*Optional*, boolean): Specifies if the device has a cooling mode. Defaults to `false`.
- **switch_datapoint** (**Required**, int): The datapoint id number of the climate switch (device on/off).
- **active_state** (*Optional*): Configuration for the Active State detection (or HVAC mode setting and reporting).

  - **datapoint** (**Required**, int): The datapoint id number of the active state - [see below](#active_state_detection).
  - **heating_value** (*Optional*, int): The active state datapoint value when in heating mode. Defaults to `1` - [see below](#active_state_detection).
  - **cooling_value** (*Optional*, int): The active state datapoint value when in cooling mode - [see below](#active_state_detection).
  - **drying_value** (*Optional*, int): The active state datapoint value when in drying mode.
  - **fanonly_value** (*Optional*, int): The active state datapoint value when in fan-only mode.
- **preset** (*Optional*): Configuration for presets.

  - **eco** (*Optional*): Configuration for Eco preset.

    - **datapoint** (**Required**, int): The datapoint id number of the Eco action.
    - **temperature** (*Optional*, int): Temperature setpoint for Eco preset.
  - **sleep** (*Optional*): Configuration for Sleep preset

    - **datapoint** (**Required**, int): The Datapoint id number of the Sleep Action
- **swing_mode** (*Optional*): Configuration for the swing (oscillation) modes.

  - **vertical_datapoint** (*Optional*, int): The datapoint id number of the vertical swing action.
  - **horizontal_datapoint** (*Optional*, int): The datapoint id number of the horizontal swing action.
- **fan_mode** (*Optional*): Configuration for fan modes/fan speeds.

  - **datapoint** (**Required**, int): The datapoint id number of the Fan value state.
  - **auto_value** (*Optional*, int): The datapoint value the device reports when the fan is on `auto` speed.
  - **low_value** (*Optional*, int): The datapoint value the device reports when the fan is on `low` speed.
  - **medium_value** (*Optional*, int): The datapoint value the device reports when the fan is on `medium` speed.
  - **middle_value** (*Optional*, int): The datapoint value the device reports when the fan is on `middle` speed. (May set to device's `high` value if you have a `Turbo` option).
  - **high_value** (*Optional*, int): The datapoint value the device reports when the fan is on `high` speed. (Sometimes called `Turbo`  ).
- **heating_state_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): The input pin indicating that the device is heating - [see below](#active_state_detection). Only used if **active_state_datapoint** is not configured.
- **cooling_state_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): The input pin indicating that the device is cooling - [see below](#active_state_detection). Only used if **active_state_datapoint** is not configured.
- **target_temperature_datapoint** (**Required**, int): The datapoint id number of the target temperature.
- **current_temperature_datapoint** (**Required**, int): The datapoint id number of the current temperature.
- **temperature_multiplier** (*Optional*, float): A multiplier to modify the incoming and outgoing temperature values - [see below](#temperature-multiplier).

- **reports_fahrenheit** (*Optional*, boolean): Set to `true` if the device reports temperatures in Fahrenheit. ESPHome expects all climate temperatures to be in Celcius, otherwise unexpected conversions will take place when it is published to Home Assistant. Defaults to `false`.

If the device has different multipliers for current and target temperatures, **temperature_multiplier** can be replaced with both of:

- **current_temperature_multiplier** (*Optional*, float): A multiplier to modify the current temperature value.
- **target_temperature_multiplier** (*Optional*, float): A multiplier to modify the target temperature value.

- All other options from [Climate](/components/climate#config-climate).

{{< anchor "active_state_detection" >}}

## Active state detection

Some Tuya climate devices don't have a data point for setting and reporting HVAC mode, they use a data point to report their active state (current action). In this case, you can just use the **active_state** configuration.

If your device uses a data point for HVAC mode, but not for reporting the active state, it is possible to modify the hardware so that the relay outputs can be read by the ESP. Please refer to [this discussion](https://github.com/klausahrenberg/WThermostatBeca/issues/17) for more details on the required modifications. You can then use the **heating_state_pin** and/or **cooling_state_pin** configuration variables to detect the current state.

If none of the above variables are set, the active state is inferred from the difference between the current and target temperatures:

- If **supports_heat** is `True` and the current temperature is more than 1 °C below the target temperature, the device is expected to be heating.
- If **supports_cool** is `True` and the current temperature is more than 1 °C above the target temperature, the device is expected to be cooling.

{{< anchor "temperature-multiplier" >}}

## Temperature multiplier

Some Tuya climate devices report the temperature with a multiplied factor. This is because the MCU only utlizes
integers for data reporting and to get a .5 temperature you need to divide by 2 on the ESPHome side.

## See Also

- {{< docref "/components/tuya" >}}
- {{< docref "/components/climate" >}}
- {{< apiref "tuya/climate/tuya_climate.h" "tuya/climate/tuya_climate.h" >}}
