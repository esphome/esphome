---
description: "Information about the base representation of all binary sensors."
title: "Binary Sensor Component"
params:
  seo:
    description: Information about the base representation of all binary sensors.
    image: folder-open.svg
---

With ESPHome you can use different types of binary sensors. They will
automatically appear in the Home Assistant front-end and have several
configuration options.

{{< anchor "config-binary_sensor" >}}

## Base Binary Sensor Configuration

All binary sensors have a platform and an optional device class. By
default, the binary will chose the appropriate device class itself, but
you can always override it.

```yaml
binary_sensor:
  - platform: ...
    device_class: motion
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the binary sensor. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the binary sensor to use that name, you can set `name: None`.

- **device_class** (*Optional*, string): The device class for the
  sensor. See <https://www.home-assistant.io/integrations/binary_sensor/#device-class>
  for a list of available options.

- **icon** (*Optional*, icon): Manually set the icon to use for the binary sensor in the frontend.
- **filters** (*Optional*, list): A list of filters to apply on the binary sensor values such as
  inverting signals. See [Binary Sensor Filters](#binary_sensor-filters).

Automations:

- **on_press** (*Optional*, [Automation](/automations)): An automation to perform
  when the button is pressed. See [`on_press`](#binary_sensor-on_press).

- **on_release** (*Optional*, [Automation](/automations)): An automation to perform
  when the button is released. See [`on_release`](#binary_sensor-on_release).

- **on_state** (*Optional*, [Automation](/automations)): An automation to perform
  when a state change is published. See [`on_state`](#binary_sensor-on_state).

- **on_click** (*Optional*, [Automation](/automations)): An automation to perform
  when the button is held down for a specified period of time.
  See [`on_click`](#binary_sensor-on_click).

- **on_double_click** (*Optional*, [Automation](/automations)): An automation to perform
  when the button is pressed twice for specified periods of time.
  See [`on_double_click`](#binary_sensor-on_double_click).

- **on_multi_click** (*Optional*, [Automation](/automations)): An automation to perform
  when the button is pressed in a specific sequence.
  See [`on_multi_click`](#binary_sensor-on_multi_click).

Advanced options:

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **trigger_on_initial_state** (*Optional*, boolean): If true, any applicable triggers will be fired when the binary sensor
  state changes from `unknown` to a valid state. This applies to the first valid state set, and any valid state set after
  a `binary_sensor.invalidate_state` action has been excuted. The default is `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options.
  Set to `""` to remove the default entity category.

- If MQTT enabled, all other options from [MQTT Component](/components/mqtt#config-mqtt-component).
- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

## Actions

{{< anchor "binary_sensor-invalidate_state-action" >}}

### `binary_sensor.invalidate_state` Action

This action will invalidate the current state of the sensor. It is most useful with the Template binary sensor.
After the state is invalidated, it will be reported to Home Assistant as `unknown`. Example:

```yaml
on_...:
  binary_sensor.invalidate_state: my_binary_sensor_id
```

The state may also be invalidated by an API call in a lambda - see the API reference linked below.

{{< anchor "binary_sensor-filters" >}}

## Binary Sensor Filters

With binary sensor filters you can customize how ESPHome handles your binary sensor values even more.
They are similar to [Sensor Filters](/components/sensor#sensor-filters). All filters are processed in a pipeline.
This means all binary sensor filters are processed in the order given in the configuration (so order
of these entries matters!)

```yaml
binary_sensor:
  - platform: ...
    # ...
    filters:
      - invert:
      - delayed_on: 100ms
      - delayed_off: 100ms
      # Templated, delays for 1s (1000ms) only if a reed switch is active
      - delayed_on_off: !lambda "if (id(reed_switch).state) return 1000; else return 0;"
      - delayed_on_off:
          time_on: 10s
          time_off: !lambda "if (id(reed_switch).state) return 1000; else return 0;"
      - autorepeat:
        - delay: 1s
          time_off: 100ms
          time_on: 900ms
        - delay: 5s
          time_off: 100ms
          time_on: 400ms
      - lambda: |-
          if (id(other_binary_sensor).state) {
            return x;
          } else {
            return {};
          }
```

### `invert`

Simple filter that just inverts every value from the binary sensor.

### `delayed_on`

(**Required**, time, [templatable](/automations/templates)): When a signal ON is received,
wait for the specified time period until publishing an ON state. If an OFF value is received
while waiting, the ON action is discarded. Or in other words:
Only send an ON value if the binary sensor has stayed ON for at least the specified time period.
When using a lambda call, you should return the delay value in milliseconds.
**Useful for debouncing push buttons**.

### `delayed_off`

(**Required**, time, [templatable](/automations/templates)): When a signal OFF is received,
wait for the specified time period until publishing an OFF state. If an ON value is received
while waiting, the OFF action is discarded. Or in other words:
Only send an OFF value if the binary sensor has stayed OFF for at least the specified time period.
When using a lambda call, you should return the delay value in milliseconds.
**Useful for debouncing push buttons**.

### `delayed_on_off`

Only send an ON or OFF value if the binary sensor has stayed in the same state for at least the specified time period.

This filter uses two time delays: on and off.

If the delays are equal, then you can configure the filter in short form by passing the time parameter:

```yaml
binary_sensor:
  - platform: ...
    # ...
    filters:
      - delayed_on_off: 1s
```

(**Required**, time, [templatable](/automations/templates)): ON and OFF delay.
When using a lambda call, you should return the delay value in milliseconds.
**Useful for debouncing binary switches**.

If the delays are different, then you need to pass them as in the example below:

```yaml
binary_sensor:
  - platform: ...
    # ...
    filters:
      - delayed_on_off:
          time_on: 10s
          time_off: 20s
```

Configuration variables:

- **time_on** (**Required**, time, [templatable](/automations/templates)): ON delay.
- **time_off** (**Required**, time, [templatable](/automations/templates)): OFF delay.

When using a lambda call, you should return the delay value in milliseconds.

### `autorepeat`

A filter implementing the autorepeat behavior. The filter is parametrized by a list of timing descriptions.
When a signal ON is received it is passed to the output and the first `delay` is started. When this
interval expires the output is turned OFF and toggles using the `time_off` and `time_on` durations
for the OFF and ON state respectively. At the same time the `delay` of the second timing description
is started and the process is repeated until the list is exhausted, in which case the timing of the
last description remains in use. Receiving an OFF signal stops the whole process and immediately outputs OFF.

The example thus waits one second with the output being ON, toggles it once per second for five seconds,
then toggles twice per second until OFF is received.

An `autorepeat` filter with no timing description is equivalent to one timing with all the parameters
set to default values.

Configuration variables:

- **delay** (*Optional*, [Time](/guides/configuration-types#time)): Delay to proceed to the next timing. Defaults to `1s`.
- **time_off** (*Optional*, [Time](/guides/configuration-types#time)): Interval to hold the output at OFF. Defaults to `100ms`.
- **time_on** (*Optional*, [Time](/guides/configuration-types#time)): Interval to hold the output at ON. Defaults to `900ms`.

### `lambda`

Specify any [lambda](/automations/templates#config-lambda) for more complex filters. The input value from
the binary sensor is `x` and you can return `true` for ON, `false` for OFF, and `{}` to stop
the filter chain.

### `settle`

(**Required**, time, [templatable](/automations/templates)): When a signal is received, publish the state
but wait for the received state to remain the same for specified time period before publishing any
additional state changes. This filter complements the `delayed_on_off` filter but publishes value changes at
the beginning of the delay period.
When using a lambda call, you should return the delay value in milliseconds.
**Useful for debouncing binary switches**.

### `timeout`

(**Required**, time, [templatable](/automations/templates)): If no value is published for the specified
time period, invalidate the state.

## Binary Sensor Automation

The triggers for binary sensors in ESPHome use the lingo from computer mouses.
For example, a `press` is triggered in the first moment when the button on your mouse is pushed down.

You can access the current state of the binary sensor in [lambdas](/automations/templates#config-lambda) using
`id(binary_sensor_id).state`.

{{< anchor "binary_sensor-on_press" >}}

### `on_press`

This automation will be triggered when the button is first pressed down, or in other words on the leading
edge of the signal.

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_press:
      then:
        - switch.turn_on: relay_1
```

Configuration variables: See [Automation](/automations).

{{< anchor "binary_sensor-on_release" >}}

### `on_release`

This automation will be triggered when a button press ends, or in other words on the falling
edge of the signal.

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_release:
      then:
        - switch.turn_off: relay_1
```

Configuration variables: See [Automation](/automations).

{{< anchor "binary_sensor-on_state" >}}

### `on_state`

This automation will be triggered when a new state is received (and thus combines `on_press`
and `on_release` into one trigger). The new state will be given as the variable `x` as a boolean
and can be used in [lambdas](/automations/templates#config-lambda). It will not be called when the state is invalidated; it will be called when
the state initially becomes valid only if `trigger_on_initial_state` is true.

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_state:
      then:
        - switch.turn_off: relay_1
```

Configuration variables: See [Automation](/automations).

{{< anchor "binary_sensor-on_state_change" >}}

### `on_state_change`

An alternative to `on_state` that is also triggered when the binary sensor state is invalidated. It is passed two parameters, `x` as for `on_change`
will be the new value, and `x_previous` is the value immediately prior to the change. Both these parameters are of type `optional<bool>` so also indicate
if the values were valid. Note that this is called on all state changes, including initial states.

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_state_change:
      then:
      - logger.log:
          format: "Old state was %s"
          args: ['x_previous.has_value() ? ONOFF(x_previous.value()) : "Unknown"']
      - logger.log:
          format: "New state is %s"
          args: ['x.has_value() ? ONOFF(x.value()) : "Unknown"']
```

Configuration variables: See [Automation](/automations).

{{< anchor "binary_sensor-on_click" >}}

### `on_click`

This automation will be triggered when a button is pressed down for a time period of length
`min_length` to `max_length`. Any click longer or shorter than this will not trigger the automation.
The automation is therefore triggered on the falling edge of the signal.

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_click:
      min_length: 50ms
      max_length: 350ms
      then:
        - switch.turn_off: relay_1
```

Configuration variables:

- **min_length** (*Optional*, [Time](/guides/configuration-types#time)): The minimum duration the click should last. Defaults to `50ms`.
- **max_length** (*Optional*, [Time](/guides/configuration-types#time)): The maximum duration the click should last. Defaults to `350ms`.
- See [Automation](/automations).

> [!NOTE]
> Multiple `on_click` entries can be defined like this (see also [`on_multi_click`](#binary_sensor-on_multi_click)
> for more complex matching):
>
> ```yaml
> binary_sensor:
>   - platform: gpio
>     # ...
>     on_click:
>     - min_length: 50ms
>       max_length: 350ms
>       then:
>         - switch.turn_off: relay_1
>     - min_length: 500ms
>       max_length: 1000ms
>       then:
>         - switch.turn_on: relay_1
> ```

{{< anchor "binary_sensor-on_double_click" >}}

### `on_double_click`

This automation will be triggered when a button is pressed down twice, with the first click lasting between
`min_length` and `max_length`. When a second leading edge then happens within `min_length` and
`max_length`, the automation is triggered.

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_double_click:
      min_length: 50ms
      max_length: 350ms
      then:
        - switch.turn_off: relay_1
```

Configuration variables:

- **min_length** (*Optional*, [Time](/guides/configuration-types#time)): The minimum duration the click should last. Defaults to `50ms`.
- **max_length** (*Optional*, [Time](/guides/configuration-types#time)): The maximum duration the click should last. Defaults to `350ms`.
- See [Automation](/automations).

{{< anchor "binary_sensor-on_multi_click" >}}

### `on_multi_click`

This automation will be triggered when a button is pressed in a user-specified sequence.

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_multi_click:
    - timing:
        - ON for at most 1s
        - OFF for at most 1s
        - ON for 0.5s to 1s
        - OFF for at least 0.2s
      then:
        - logger.log: "Double-Clicked"
```

Configuration variables:

- **timing** (**Required**): The timing of the multi click. This uses a language-based grammar using
  these styles:

  - `<ON/OFF> for <TIME> to <TIME>`
  - `<ON/OFF> for at least <TIME>`
  - `<ON/OFF> for at most <TIME>`

- **invalid_cooldown** (*Optional*, [Time](/guides/configuration-types#time)): If a multi click is started, but the timing
  set in `timing` does not match, a "cool down" period will be activated during which no timing
  will be matched. Defaults to `1s`.

- See [Automation](/automations).

> [!NOTE]
> Getting the timing right for your use-case can sometimes be a bit difficult. If you set the
> [global log level](/components/logger#logger-log_levels) to `VERBOSE`, the multi click trigger shows logs
> about what stopped the trigger from happening.

You can use an `OFF` timing at the end of the timing sequence to differentiate between different
kinds of presses. For example the configuration below will differentiate between double, long and short
presses.

```yaml
on_multi_click:
- timing:
    - ON for at most 1s
    - OFF for at most 1s
    - ON for at most 1s
    - OFF for at least 0.2s
  then:
    - logger.log: "Double Clicked"
- timing:
    - ON for 1s to 2s
    - OFF for at least 0.5s
  then:
    - logger.log: "Single Long Clicked"
- timing:
    - ON for at most 1s
    - OFF for at least 0.5s
  then:
    - logger.log: "Single Short Clicked"
```

While [`on_click`](#binary_sensor-on_click) only triggers on the falling edge of the signal,
and [`on_double_click`](#binary_sensor-on_double_click) only on the second leading edge, an
automation for `on_multi_click` can trigger at any time. For example, an `ON for at least`
timing without an `OFF` does not await a falling edge. This supports implementing a
continuous longpress, optionally also handling clicks for the very same sensor:

```yaml
binary_sensor:
  - platform: gpio
    id: button_1
    # ...
    on_multi_click:
    # One can also replace this part with `on_click`
    - timing:
        - ON for at most 0.7s
      then:
        - light.turn_on:
            id: light_1
            brightness: 10%
    - timing:
        - ON for at least 1s
      then:
        - while:
            condition:
              # Self-reference this very sensor
              binary_sensor.is_on: button_1
            then:
              - light.dim_relative:
                  id: light_1
                  relative_brightness: 5%
                  transition_length: 0.1s
              - delay: 0.1s
```

{{< anchor "binary_sensor-is_on_condition" >}}
{{< anchor "binary_sensor-is_off_condition" >}}

### `binary_sensor.is_on` / `binary_sensor.is_off` Condition

This [Condition](/automations/actions#all-conditions) checks if the given binary sensor is ON (or OFF).

```yaml
# In some trigger:
on_...:
  if:
    condition:
      # Same syntax for is_off
      binary_sensor.is_on: my_binary_sensor
```

{{< anchor "binary_sensor-lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all binary sensors to do some
advanced stuff.

- `publish_state()`  : Manually cause the binary sensor to publish and store a state from anywhere
  in the program.

```yaml
    // Within lambda, publish an OFF state.
    id(my_binary_sensor).publish_state(false);

    // Within lambda, publish an ON state.
    id(my_binary_sensor).publish_state(true);
```

- `.state`  : Retrieve the current state of the binary sensor.

```yaml
    // Within lambda, get the binary sensor state and conditionally do something
    if (id(my_binary_sensor).state) {
      // Binary sensor is ON, do something here
    } else {
      // Binary sensor is OFF, do something else here
    }
```

## See Also

- {{< apiref "binary_sensor/binary_sensor.h" "binary_sensor/binary_sensor.h" >}}
