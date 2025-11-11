---
description: "Instructions for setting up number components in ESPHome."
title: "Number Component"
params:
  seo:
    description: Instructions for setting up number components in ESPHome.
    image: folder-open.svg
---

ESPHome has support for components to create a number entity. A number entity is
like a sensor that can read a value from a device, but is useful when that value
can be set by the user/frontend.

> [!NOTE]
> Home Assistant Core 2021.7 or higher is required for ESPHome number entities to work.

{{< anchor "config-number" >}}

## Base Number Configuration

All numbers in ESPHome have a name and an optional icon.

```yaml
# Example number configuration
name: Livingroom Volume

# Optional variables:
icon: "mdi:volume-high"
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the number. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the number to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the number in the frontend.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options.
  Set to `""` to remove the default entity category.

- **unit_of_measurement** (*Optional*, string): Manually set the unit
  of measurement for the number.

- **mode** (*Optional*, string): Defines how the number should be displayed in the frontend.
  See <https://developers.home-assistant.io/docs/core/entity/number/#properties>
  for a list of available options.
  Defaults to `"auto"`.

- **device_class** (*Optional*, string): The device class for the number.
  See <https://www.home-assistant.io/integrations/number/#device-class>
  for a list of available options.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

Automations:

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when a new value is published. See [`on_value`](#number-on_value).

- **on_value_range** (*Optional*, [Automation](/automations)): An automation to perform
  when a published value transition from outside to a range to inside. See [`on_value_range`](#number-on_value_range).

MQTT Options:

- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

## Number Automation

You can access the most recent state of the number in [lambdas](/automations/templates#config-lambda) using
`id(number_id).state`.

{{< anchor "number-on_value" >}}

### `on_value`

This automation will be triggered when a new value is published. In [Lambdas](/automations/templates#config-lambda)
you can get the value from the trigger with `x`.

```yaml
number:
  - platform: template
    # ...
    on_value:
      then:
        - light.turn_on:
            id: light_1
            red: !lambda "return x/255;"
```

Configuration variables: See [Automation](/automations).

{{< anchor "number-on_value_range" >}}

### `on_value_range`

With this automation you can observe if a number value passes from outside
a defined range of values to inside a range.
This trigger will only trigger when the new value is inside the range and the previous value
was outside the range. On startup, the last state before reboot is restored and if the value crossed
the boundary during the boot process, the trigger is also executed.

Define the range with `above` and `below`. If only one of them is defined, the interval is half-open.
So for example `above: 5` with no below would mean the range from 5 to positive infinity.

```yaml
number:
  - platform: template
    # ...
    on_value_range:
      above: 5
      below: 10
      then:
        - switch.turn_on: relay_1
```

Configuration variables:

- **above** (*Optional*, float): The minimum for the trigger.
- **below** (*Optional*, float): The maximum for the trigger.
- See [Automation](/automations).

{{< anchor "number-in_range_condition" >}}

### `number.in_range` Condition

This condition passes if the state of the given number is inside a range.

Define the range with `above` and `below`. If only one of them is defined, the interval is half-open.
So for example `above: 5` with no below would mean the range from 5 to positive infinity.

```yaml
# in a trigger:
on_...:
  if:
    condition:
      number.in_range:
        id: my_number
        above: 50.0
    then:
      - script.execute: my_script
```

Configuration variables:

- **above** (*Optional*, float): The minimum for the condition.
- **below** (*Optional*, float): The maximum for the condition.

{{< anchor "number-set_action" >}}

### `number.set` Action

This is an [Action](/automations/actions#all-actions) for setting a number state.

```yaml
- number.set:
    id: my_number
    value: 42
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the number to set.
- **value** (**Required**, float, [templatable](/automations/templates)):
  The value to set the number to.

{{< anchor "number-increment_action" >}}

### `number.increment` Action

This is an [Action](/automations/actions#all-actions) for incrementing a number value by its
step size (default: 1).

```yaml
- number.increment:
    id: my_number
    cycle: false

# Shorthand
- number.increment: my_number
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the number component to update.
- **cycle** (*Optional*, boolean): Whether or not to set the number to its minimum
  value when the increment pushes the value beyond its maximum value. This will only
  work when the number component uses a minimum and maximum value.
  Defaults to `true`.

{{< anchor "number-decrement_action" >}}

### `number.decrement` Action

This is an [Action](/automations/actions#all-actions) for decrementing a number value by its
step size (default: 1).

```yaml
- number.decrement:
    id: my_number
    cycle: false

# Shorthand
- number.decrement: my_number
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the number component to update.
- **cycle** (*Optional*, boolean): Whether or not to set the number to its maximum
  value when the decrement pushes the value below its minimum value. This will only
  work when the number component uses a minimum and maximum value.
  Defaults to `true`.

{{< anchor "number-to-min_action" >}}

### `number.to_min` Action

This is an [Action](/automations/actions#all-actions) setting a number to its minimum value, given
a number component that has a minimum value defined for it.

```yaml
- number.to_min:
    id: my_number

# Shorthand
- number.to_min: my_number
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the number component to update.

{{< anchor "number-to-max_action" >}}

### `number.to_max` Action

This is an [Action](/automations/actions#all-actions) setting a number to its maximum value, given
a number component that has a maximum value defined for it.

```yaml
- number.to_max:
    id: my_number

# Shorthand
- number.to_max: my_number
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the number component to update.

{{< anchor "number-operation_action" >}}

### `number.operation` Action

This is an [Action](/automations/actions#all-actions) that can be used to perform an operation
on a number component (set to minimum or maximum value, decrement, increment),
using a generic templatable action call.

```yaml
# Using values
- number.operation:
    id: my_number
    operation: Increment
    cycle: true

# Or templated (lambda)
- number.operation:
    id: my_number
    operation: !lambda "return NUMBER_OP_INCREMENT;"
    cycle: !lambda "return true;"
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the number to update.
- **operation** (**Required**, string, [templatable](/automations/templates)):
  What operation to perform on the number component. One of `TO_MIN`,
  `TO_MAX`, `DECREMENT` or `INCREMENT` (case insensitive). When writing a
  lambda for this field, then return one of the following enum values:
  `NUMBER_OP_TO_MIN`, `NUMBER_OP_TO_MAX`, `NUMBER_OP_DECREMENT` or
  `NUMBER_OP_INCREMENT`.

- **cycle** (*Optional*, bool, [templatable](/automations/templates)):
  Can be used with `DECREMENT` or `INCREMENT` to specify whether or not to
  wrap around the value when respectively the minimum or maximum value of the
  number is exceeded.

{{< anchor "number-lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all numbers to do some
advanced stuff (see the full API Reference for more info).

- `.make_call()`  : Make a call for updating the number value.

```cpp
    // Within lambda, push a value of 42
    auto call = id(my_number).make_call();
    call.set_value(42);
    call.perform();
```

  Check the API reference for information on the methods that are available for
  the `NumberCall` object. You can for example also use `call.number_to_min()`
  to set the number to its minimum value or `call.number_increment(true)` to increment
  the number by its step size with the cycle feature enabled.

- `.state`  : Retrieve the current value of the number. Is `NAN` if no value has been read or set.

```cpp
    // For example, create a custom log message when a value is received:
    ESP_LOGI("main", "Value of my number: %f", id(my_number).state);
```

## See Also

- {{< apiref "Number" "number/number.h" >}}
- {{< apiref "NumberCall" "number/number_call.h" >}}
