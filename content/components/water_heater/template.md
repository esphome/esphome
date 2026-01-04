---
description: "Instructions for setting up template water heaters in ESPHome."
title: "Template Water Heater"
params:
  seo:
    description: Instructions for setting up template water heaters in ESPHome.
    image: description.svg
---

The `template` water heater platform allows you to create simple water heaters out of just a few actions and lambdas. Once
defined, it will automatically appear in Home Assistant as a water heater entity and can be controlled through the frontend.

```yaml
# Example configuration entry
water_heater:
  - platform: template
    name: "Template Boiler"
    id: my_boiler

    # Lambda to read the current temperature (e.g. from a sensor)
    current_temperature: !lambda 'return id(my_sensor).state;'

    # Lambda to read the current operation mode (optional)
    mode: !lambda 'return water_heater::WATER_HEATER_MODE_ECO;'
    optimistic: true

    # List of modes to show in the UI (optional)
    supported_modes:
      - "OFF"
      - ECO
      - GAS

    visual:
      min_temperature: 10.0
      max_temperature: 85.0
      target_temperature_step: 0.5

    set_action:
      - lambda: |-
          ESP_LOGI("boiler", "New mode: %d", id(my_boiler).get_mode());
```

Possible return values for the lambdas:

- `current_temperature`: Returns a `float` (e.g. `42.5`).
- `mode`: Returns a `WaterHeaterMode` enum (e.g. `water_heater::WATER_HEATER_MODE_ECO`).

## Configuration variables

- **current_temperature** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated repeatedly to get the current temperature of the water. Expects a float return value.

- **mode** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated repeatedly to get the current operation mode. Expects a `WaterHeaterMode` enum return value.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode, any command sent to
  the template water heater will immediately update the reported state. Defaults to `true`.

- **set_action** (*Optional*, [Action](/automations/actions#all-actions)):
  The action to perform when the water heater receives a command (mode change, target temperature change, etc.).
  This is where you implement the actual control logic for your water heater.

- **supported_modes** (*Optional*, list):
  Static list of operation modes that will be exposed to the frontend (for example Home Assistant). This controls the `operation_list` reported to Home Assistant and affects only the UI and available service calls. It does not change runtime behavior or control logic. When not specified, all supported water heater modes are shown by default.

  > [!NOTE]
  > The list of `supported_modes` is static and evaluated at startup. It cannot be changed dynamically and does not support templates or lambdas.

- **restore_mode** (*Optional*, enum): Control how the water heater attempts to restore state on bootup.

  - `NO_RESTORE` (Default): Do not save or restore state.
  - `RESTORE`  : Attempts to restore the state (target temp & mode) on startup, but doesn't perform the `set_action`.
  - `RESTORE_AND_CALL`  : Attempts to restore the state on startup and immediately executes the `set_action`.

- All other options from [Water Heater](/components/water_heater#config-water-heater).

{{< anchor "water_heater-template-publish_action" >}}

## `water_heater.template.publish` Action

You can also publish state to a template water heater from elsewhere in your YAML file
with the `water_heater.template.publish` action.

```yaml
# Example action
- water_heater.template.publish:
    id: my_boiler
    current_temperature: 55.0
    target_temperature: 60.0
    mode: ECO
```

Configuration options:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the template water heater.
- **current_temperature** (*Optional*, [templatable](/automations/templates), float):
  The current measured temperature to publish.
- **target_temperature** (*Optional*, [templatable](/automations/templates), float):
  The target setpoint temperature to publish.
- **mode** (*Optional*, [templatable](/automations/templates), string):
  The operation mode to publish. See [Water Heater Modes](/components/water_heater#water-heater-modes) for options.

> [!NOTE]
> This action can also be written in lambdas:
>
> ```cpp
> id(my_boiler).set_current_temperature(55.0);
> id(my_boiler).publish_state();
> ```

## See Also

- {{< docref "/components/water_heater" >}}
- [Automation](/automations)
- {{< apiref "template/water_heater/template_water_heater.h" "template/water_heater/template_water_heater.h" >}}
