---
description: "Instructions for setting up water heaters in ESPHome."
title: "Water Heater Component"
---

The `water_heater` component is a generic representation of water heaters (boilers) in ESPHome. A water heater handles a target temperature setpoint and an operation mode (like *Eco*, *Electric*, or *Performance*).

> [!NOTE]
> Home Assistant integration for water heater entities is not yet available. See [home-assistant/core#159201](https://github.com/home-assistant/core/pull/159201) for progress.

{{< anchor "config-water-heater" >}}

## Base Water Heater Configuration

All water heater config schemas inherit from this schema - you can set these keys for water heaters.

```yaml
water_heater:
  - platform: ...
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the water heater. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and you want the water heater
> to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the water heater in the frontend.

Advanced options:

- **visual** (*Optional*): Configuration for the frontend representation.
  - **min_temperature** (*Optional*, float): Override the minimum temperature shown in the frontend.
  - **max_temperature** (*Optional*, float): Override the maximum temperature shown in the frontend.
  - **target_temperature_step** (*Optional*, float): Override the temperature steps shown in the frontend.

- **supported_modes** (*Optional*, list): Static list of operation modes that will be exposed to the frontend (for example Home Assistant). When not specified, all modes supported by the platform are exposed.

  > **Note**
  > This option is platform-dependent. Not all water heater platforms allow configuring supported modes.

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will not be exposed to the
  frontend (like Home Assistant). Only specifying an `id` without a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI). Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity. See
  <https://developers.home-assistant.io/docs/core/entity/#generic-properties> for a list of available options. Set to
  `""` to remove the default entity category.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

MQTT options:

- **mode_command_topic** (*Optional*, string): The topic to receive mode commands on.
- **mode_state_topic** (*Optional*, string): The topic to publish mode state changes to.
- **target_temperature_command_topic** (*Optional*, string): The topic to receive target temperature commands on.
- **target_temperature_state_topic** (*Optional*, string): The topic to publish target temperature state changes to.
- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

{{< anchor "water-heater-control_action" >}}

## `water_heater.control` Action

This [action](/automations/actions#all-actions) allows you to set the operation mode and/or target temperature of the water heater.

```yaml
on_...:
  then:
    - water_heater.control:
        id: boiler_1
        mode: ECO
        target_temperature: 55.0
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The water heater to control.
- **mode** (*Optional*, string): The operation mode to set. See [Modes](#water-heater-modes) for available options.
- **target_temperature** (*Optional*, float): The target temperature to set (e.g., `60.0`).

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> auto call = id(boiler_1).make_call();
> call.set_mode("PERFORMANCE");
> call.set_target_temperature(65.0);
> call.perform();
> ```

{{< anchor "water-heater-modes" >}}

## Water Heater Modes

The following modes are available for water heaters. Note that not all platforms support all modes.

- `OFF`
- `ECO`
- `ELECTRIC`
- `PERFORMANCE`
- `HIGH_DEMAND`
- `HEAT_PUMP`
- `GAS`

{{< anchor "water-heater-lambda_calls" >}}

## Lambdas

From [lambdas](/automations/templates#config-lambda), you can access the current state of the water heater.

- `current_temperature` : Retrieve the current measured temperature of the water (float).

```cpp
    if (id(my_boiler).current_temperature < 40.0) {
      // Water is cold
    }
```

- `target_temperature` : Retrieve the target setpoint temperature (float).

- `mode` : Retrieve the current operation mode.

```cpp
    if (id(my_boiler).mode == water_heater::WATER_HEATER_MODE_ECO) {
      // Boiler is in ECO mode
    } else if (id(my_boiler).mode == water_heater::WATER_HEATER_MODE_PERFORMANCE) {
      // Boiler is in PERFORMANCE mode
    }
```

Available C++ enums for modes:

- `water_heater::WATER_HEATER_MODE_OFF`
- `water_heater::WATER_HEATER_MODE_ECO`
- `water_heater::WATER_HEATER_MODE_ELECTRIC`
- `water_heater::WATER_HEATER_MODE_PERFORMANCE`
- `water_heater::WATER_HEATER_MODE_HIGH_DEMAND`
- `water_heater::WATER_HEATER_MODE_HEAT_PUMP`
- `water_heater::WATER_HEATER_MODE_GAS`

## See Also

- {{< docref "/components/water_heater/template" >}}
- {{< apiref "water_heater/water_heater.h" "water_heater/water_heater.h" >}}
