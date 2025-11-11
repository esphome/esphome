---
description: "Instructions for setting up the base fan component."
title: "Fan Component"
params:
  seo:
    description: Instructions for setting up the base fan component.
    image: folder-open.svg
---

With the `fan` domain you can create components that appear as fans in
the Home Assistant frontend. A fan can be switched on or off, optionally
has a speed between 1 and the maximum supported speed of the fan, and can have an
oscillation and direction output.

{{< img src="fan-ui.png" alt="Image" class="align-center" >}}

{{< anchor "config-fan" >}}

## Base Fan Configuration

```yaml
fan:
  - platform: ...
    name: ...
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name of the fan. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the fan to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the fan in the frontend.
- **restore_mode** (*Optional*): Control how the fan attempts to restore state on boot.

  - `NO_RESTORE` - Don't restore any state.
  - `RESTORE_DEFAULT_OFF` - Attempt to restore state and default to OFF if not possible to restore.
  - `RESTORE_DEFAULT_ON` - Attempt to restore state and default to ON.
  - `RESTORE_INVERTED_DEFAULT_OFF` - Attempt to restore state inverted from the previous state and default to OFF.
  - `RESTORE_INVERTED_DEFAULT_ON` - Attempt to restore state inverted from the previous state and default to ON.
  - `ALWAYS_OFF` (Default) - Always initialize the fan as OFF on bootup.
  - `ALWAYS_ON` - Always initialize the fan as ON on bootup.

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options.
  Set to `""` to remove the default entity category.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

MQTT options:

- **direction_state_topic** (*Optional*, string): The topic to
  publish fan direction state changes to (options: forward, reverse).

- **direction_command_topic** (*Optional*, string): The topic to
  receive fan direction commands on (options: forward, reverse, toggle).

- **oscillation_state_topic** (*Optional*, string): The topic to
  publish fan oscillation state changes to.

- **oscillation_command_topic** (*Optional*, string): The topic to
  receive oscillation commands on.

- **speed_level_state_topic** (*Optional*, int): The topic to publish
  numeric fan speed state changes to (range: 0 to speed count).

- **speed_level_command_topic** (*Optional*, int): The topic to receive
  numeric speed commands on (range: 0 to speed count).

- **speed_state_topic** (*Optional*, string): The topic to publish fan
  speed state changes to (options: LOW, MEDIUM, HIGH).

- **speed_command_topic** (*Optional*, string): The topic to receive
  speed commands on (options: LOW, MEDIUM, HIGH).

- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

Automation triggers:

- **on_state** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the fan state is changed. See [`fan.on_state` Trigger](#fan-on_state_trigger).

- **on_turn_on** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the fan is turned on. See [`fan.on_turn_on` / `fan.on_turn_off` Trigger](#fan-on_turn_on_off_trigger).

- **on_turn_off** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the fan is turned off. See [`fan.on_turn_on` / `fan.on_turn_off` Trigger](#fan-on_turn_on_off_trigger).

- **on_direction_set** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the fan direction is changed. See [`fan.on_direction_set` Trigger](#fan-on_direction_set_trigger).

- **on_oscillating_set** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the fan oscillating state is changed. See [`fan.on_oscillating_set` Trigger](#fan-on_oscillating_set_trigger).

- **on_speed_set** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the fan speed is changed. See [`fan.on_speed_set` Trigger](#fan-on_speed_set_trigger).

- **on_preset_set** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the fan preset mode is changed. See [`fan.on_preset_set` Trigger](#fan-on_preset_set_trigger).

{{< anchor "fan-toggle_action" >}}

## `fan.toggle` Action

Toggles the ON/OFF state of the fan with the given ID when executed.

```yaml
on_...:
  then:
    - fan.toggle: fan_1
```

{{< anchor "fan-turn_off_action" >}}

## `fan.turn_off` Action

Turns the fan with the given ID off when executed.

```yaml
on_...:
  then:
    - fan.turn_off: fan_1
```

{{< anchor "fan-turn_on_action" >}}

## `fan.turn_on` Action

Turns the fan with the given ID on when executed.

```yaml
on_...:
  then:
    - fan.turn_on:
        id: fan_1
    # Shorthand:
    - fan.turn_on: fan_1
```

Configuration options:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the fan.
- **oscillating** (*Optional*, boolean, [templatable](/automations/templates)):
  Set the oscillation state of the fan. Defaults to not affecting oscillation.

- **speed** (*Optional*, int, [templatable](/automations/templates)):
  Set the speed level of the fan. Can be a number between 1 and the maximum speed level of the fan.

- **direction** (*Optional*, string, [templatable](/automations/templates)):
  Set the direction of the fan. Can be either `forward` or `reverse`. Defaults to not changing the direction.

{{< anchor "fan-cycle_speed_action" >}}

## `fan.cycle_speed` Action

Increments through speed levels of the fan with the given ID when executed. If the fan's speed level is set to maximum when executed, fan will cycle off unless `off_speed_cycle` is set to `false`.

```yaml
on_...:
  then:
    - fan.cycle_speed:
        id: fan_1
        off_speed_cycle: true
    # Shorthand:
    - fan.cycle_speed: fan_1
```

Configuration options:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the fan.
- **off_speed_cycle** (*Optional*, boolean, [templatable](/automations/templates)): Determines if the fan will cycle off after cycling though its highest speed. Can be `true` or `false`. If `false` fan will cycle to its lowest speed instead of turning off. Defaults to `true`.

{{< anchor "fan-is_on_condition" >}}
{{< anchor "fan-is_off_condition" >}}

## `fan.is_on` / `fan.is_off` Condition

This [condition](/automations/actions#all-conditions) passes if the given fan is on or off.

```yaml
# in a trigger:
on_...:
  if:
    condition:
      fan.is_on: my_fan
      # same goes for is_off
    then:
    - script.execute: my_script
```

{{< anchor "fan-on_state_trigger" >}}

## `fan.on_state` Trigger

This trigger is activated each time the fan state is changed. It will fire when the state is either set via API e.g. in Home Assistant or locally by an automation or a lambda function.
A pointer to the `Fan` is available as a variable called `x`.

```yaml
fan:
  - platform: speed # or any other platform
    # ...
    on_state:
    - logger.log:
        format: "Fan State changed!  Fan Speed is %d!"
        args: [ x->speed ]
```

{{< anchor "fan-on_turn_on_off_trigger" >}}

## `fan.on_turn_on` / `fan.on_turn_off` Trigger

This trigger is activated each time the fan is turned on or off. It does not fire
if a command to turn the fan on or off already matches the current state.

```yaml
fan:
  - platform: speed # or any other platform
    # ...
    on_turn_on:
    - logger.log: "Fan Turned On!"
    on_turn_off:
    - logger.log: "Fan Turned Off!"
```

{{< anchor "fan-on_direction_set_trigger" >}}

## `fan.on_direction_set` Trigger

This trigger is activated each time the fan direction is changed. It will fire when the direction is either set via API e.g. in Home Assistant or locally by an automation or a lambda function.
The new direction is available as a variable called `x`. (`0` is FORWARD, `1` is REVERSE)

```yaml
fan:
  - platform: speed # or any other platform
    # ...
    on_direction_set:
    - logger.log:
        format: "Fan Direction was changed to %s!"
        args: [ x == 0 ? "FORWARD" : "REVERSE" ]
```

{{< anchor "fan-on_oscillating_set_trigger" >}}

## `fan.on_oscillating_set` Trigger

This trigger is activated each time the fan oscillating state is changed. It will fire when the state is either set via API e.g. in Home Assistant or locally by an automation or a lambda function.
The new oscillating state is available as a variable called `x`.

```yaml
fan:
  - platform: speed # or any other platform
    # ...
    on_oscillating_set:
    - logger.log:
        format: "Fan Oscillating State was changed to %s!"
        args: [ ONOFF(x) ]
```

{{< anchor "fan-on_speed_set_trigger" >}}

## `fan.on_speed_set` Trigger

This trigger is activated each time the fan speed is changed. It will fire when the speed is either set via API e.g. in Home Assistant or locally by an automation or a lambda function.
The new speed is available as a variable called `x`.

```yaml
fan:
  - platform: speed # or any other platform
    # ...
    on_speed_set:
    - logger.log:
        format: "Fan Speed was changed to %d!"
        args: [ x ]
```

{{< anchor "fan-on_preset_set_trigger" >}}

## `fan.on_preset_set` Trigger

This trigger is activated each time the fan preset mode is changed. It will fire when the preset mode is either set via API e.g. in Home Assistant or locally by an automation or a lambda function.
The new mode is available as a variable called `x`.

```yaml
fan:
  - platform: speed # or any other platform
    # ...
    on_preset_set:
      - logger.log:
          format: "Fan preset mode was changed to %s!"
          args: [ x.c_str() ]
```

## Lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all fans to do some
advanced stuff (see the full API Reference for more info).

- `state`  : Retrieve the current state (on/off) of the fan.

```yaml
    // Within lambda, get the fan state and conditionally do something
    if (id(my_fan).state) {
      // Fan is ON, do something here
    } else {
      // Fan is OFF, do something else here
    }
```

- `speed`  : Retrieve the current speed of the fan.

```yaml
    // Within lambda, get the fan speed and conditionally do something
    if (id(my_fan).speed == 2) {
      // Fan speed is 2, do something here
    } else {
      // Fan speed is not 2, do something else here
    }
```

- `oscillating`  : Retrieve the current oscillating state of the fan.

```yaml
    // Within lambda, get the fan oscillating state and conditionally do something
    if (id(my_fan).oscillating) {
      // Fan is oscillating, do something here
    } else {
      // Fan is not oscillating, do something else here
    }
```

- `direction`  : Retrieve the current direction of the fan.

```yaml
    // Within lambda, get the fan direction and conditionally do something
    if (id(my_fan).direction == FanDirection::FORWARD) {
      // Fan direction is forward, do something here
    } else {
      // Fan direction is reverse, do something else here
    }
```

- `preset_mode`  : Retrieve the current preset mode of the fan.

```yaml
    // Within lambda, get the fan preset mode and conditionally do something
    if (id(my_fan).preset_mode == "auto") {
      // Fan preset mode is "auto", do something here
    } else {
      // Fan preset mode is not "auto", do something else here
    }
```

- `turn_off()`  /`turn_on()`  /`toggle()`  : Manually turn the fan ON/OFF from code.
  Similar to the `fan.turn_on`, `fan.turn_off`, and `fan.toggle` actions,
  but can be used in complex lambda expressions.

```yaml
    // Turn the fan off
    auto call = id(my_fan).turn_off();
    call.perform();

    // Turn the fan on and set the speed, oscillating, and direction
    auto call = id(my_fan).turn_on();
    call.set_speed(2);
    call.set_oscillating(true);
    call.set_direction(FanDirection::REVERSE);
    call.perform();

    // Set a preset mode
    auto call = id(my_fan).turn_on();
    call.set_preset_mode("auto");
    call.perform();

    // Toggle the fan on/off
    auto call = id(my_fan).toggle();
    call.perform();
```

## Full Fan Index

- {{< apiref "fan/fan_state.h" "fan/fan_state.h" >}}
