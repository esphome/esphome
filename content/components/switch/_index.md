---
description: "Instructions for setting up generic switches in ESPHome."
title: "Switch Component"
params:
  seo:
    description: Instructions for setting up generic switches in ESPHome.
    image: folder-open.svg
---

The `switch` domain includes all platforms that should show up like a
switch and can only be turned ON or OFF.

{{< anchor "config-switch" >}}

## Base Switch Configuration

```yaml
switch:
  - platform: ...
    name: "Switch Name"
    icon: "mdi:restart"
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name of the switch. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the switch to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the
  sensor in the frontend.

- **inverted** (*Optional*, boolean): Whether to invert the binary
  state, i.e. report ON states as OFF and vice versa. Defaults
  to `false`.

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **restore_mode** (*Optional*): Control how the switch attempts to restore state on bootup.
  **NOTE** : Not all components consider **restore_mode**. Check the documentation of the specific component to understand how
  this feature works for a particular component or device.
  For restoring on ESP8266s, also see `restore_from_flash` in the
  {{< docref "/components/esp8266" "esp8266 section" >}}.

  - `RESTORE_DEFAULT_OFF` - Attempt to restore state and default to OFF if not possible to restore.
  - `RESTORE_DEFAULT_ON` - Attempt to restore state and default to ON.
  - `RESTORE_INVERTED_DEFAULT_OFF` - Attempt to restore state inverted from the previous state and default to OFF.
  - `RESTORE_INVERTED_DEFAULT_ON` - Attempt to restore state inverted from the previous state and default to ON.
  - `ALWAYS_OFF` (Default) - Always initialize the switch as OFF on bootup.
  - `ALWAYS_ON` - Always initialize the switch as ON on bootup.
  - `DISABLED` - Does nothing and leaves it up to the downstream platform component to decide. For example, the component could read hardware and determine the state, or have a specific configuration option to regulate initial state.

  Unless a specific platform defines another default value, the default is `ALWAYS_OFF`.

- **on_turn_on** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the switch is turned on. See [`switch.on_turn_on` / `switch.on_turn_off` Trigger](#switch-on_turn_on_off_trigger).

- **on_turn_off** (*Optional*, [Action](/automations/actions#all-actions)): An automation to perform
  when the switch is turned off. See [`switch.on_turn_on` / `switch.on_turn_off` Trigger](#switch-on_turn_on_off_trigger).

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options.
  Set to `""` to remove the default entity category.

- **device_class** (*Optional*, string): The device class for the switch.
  See <https://www.home-assistant.io/integrations/switch/#device-class>
  for a list of available options.

- If MQTT enabled, All other options from [MQTT Component](/components/mqtt#config-mqtt-component).
- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

{{< anchor "switch-toggle_action" >}}

### `switch.toggle` Action

This action toggles a switch with the given ID when executed.

```yaml
on_...:
  then:
    - switch.toggle: relay_1
```

{{< anchor "switch-turn_on_action" >}}

### `switch.turn_on` Action

This action turns a switch with the given ID on when executed.

```yaml
on_...:
  then:
    - switch.turn_on: relay_1
```

{{< anchor "switch-turn_off_action" >}}

### `switch.turn_off` Action

This action turns a switch with the given ID off when executed.

```yaml
on_...:
  then:
    - switch.turn_off: relay_1
```

{{< anchor "switch-control_action" >}}

### `switch.control` Action

This action allows you to control a switch with more flexibility than the basic `turn_on` and `turn_off` actions.
It accepts a templatable `state` parameter, making it useful when the desired switch state is determined dynamically.

```yaml
on_...:
  then:
    - switch.control:
        id: relay_1
        state: true

    # Or with a template
    - switch.control:
        id: relay_1
        state: !lambda |-
          return id(some_sensor).state > 50.0;
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the switch to control.
- **state** (**Required**, boolean, [templatable](/automations/templates)):
  The state to set the switch to. `true` turns the switch on, `false` turns it off.

{{< anchor "switch-is_on_condition" >}}
{{< anchor "switch-is_off_condition" >}}

### `switch.is_on` / `switch.is_off` Condition

This [Condition](/automations/actions#all-conditions) checks if the given switch is ON (or OFF).

```yaml
# In some trigger:
on_...:
  if:
    condition:
      # Same syntax for is_off
      switch.is_on: my_switch
```

{{< anchor "switch-lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all switches to do some
advanced stuff (see the full API Reference for more info).

- `publish_state()`  : Manually cause the switch to publish a new state and store it internally.
  If it's different from the last internal state, it's additionally published to the frontend.

```yaml
    // Within lambda, make the switch report a specific state
    id(my_switch).publish_state(false);
    id(my_switch).publish_state(true);
```

> [!NOTE]
> Keep in mind that this does not change the actual state of the switch. It only
> changes the state in the frontend and the internal state. If you want to
> change the actual state of the switch, you need to call `turn_on()`,
> `turn_off()` or `toggle()`.
>
> For example, if you are using a {{< docref "/components/switch/gpio" >}}, calling `publish_state()` will
> not change the GPIO pin level. To do that, you need to call `turn_on()`,
> `turn_off()`, `toggle()` or `control()`. The same applies to other switch platforms.

- `state`  : Retrieve the current state of the switch.

```yaml
    // Within lambda, get the switch state and conditionally do something
    if (id(my_switch).state) {
      // Switch is ON, do something here
    } else {
      // Switch is OFF, do something else here
    }
```

- `turn_off()`  /`turn_on()`  : Manually turn the switch ON/OFF from code.
  Similar to the `switch.turn_on` and `switch.turn_off` actions,
  but can be used in complex lambda expressions.

```yaml
    id(my_switch).turn_off();
    id(my_switch).turn_on();
    // Toggle the switch
    id(my_switch).toggle();
```

- `control()`  : Control the switch state using a boolean parameter.
  This provides a unified interface for setting switch state dynamically.

```yaml
    // Within lambda, control switch based on a condition
    id(my_switch).control(true);   // Turn ON
    id(my_switch).control(false);  // Turn OFF
    id(my_switch).control(some_condition);  // Set based on condition
```

{{< anchor "switch-on_turn_on_off_trigger" >}}

### `switch.on_turn_on` / `switch.on_turn_off` Trigger

This trigger is activated each time the switch is turned on. It becomes active
right after the switch component has acknowledged the state (e.g. after it switched
ON/OFF itself).

```yaml
switch:
  - platform: gpio  # or any other platform
    # ...
    on_turn_on:
    - logger.log: "Switch Turned On!"
    on_turn_off:
    - logger.log: "Switch Turned Off!"
```

{{< anchor "switch-on_state_trigger" >}}

### `switch.on_state` Trigger

This trigger is activated each time the switch changes state (either ON or OFF).
It provides the new state as a boolean variable `x` that can be used in the automation.

```yaml
switch:
  - platform: gpio  # or any other platform
    # ...
    on_state:
      - light.control:
          id: my_light
          state: !lambda return x;
      - if:
          condition:
            lambda: 'return x;'
          then:
            - logger.log: "Switch is now ON!"
          else:
            - logger.log: "Switch is now OFF!"
```

The variable `x` is a boolean that represents the new state:

- `true` when the switch turns ON
- `false` when the switch turns OFF

## See Also

- {{< apiref "switch/switch.h" "switch/switch.h" >}}
