---
description: "Guide for building automations in ESPHome"
title: "Actions, Triggers, Conditions"
params:
  seo:
    description: Guide for building automations in ESPHome
    image: auto-fix.svg
---

{{< anchor "actions-triggers" >}}

ESPHome *actions* are how we make an ESPHome device *do something.*

Let's begin with an example. Suppose you have a configuration file which contains:

```yaml
switch:
  - platform: gpio
    pin: GPIOXX
    name: "Living Room Dehumidifier"

binary_sensor:
  - platform: gpio
    pin: GPIOXX
    name: "Living Room Dehumidifier Toggle Button"
```

With this file you can already perform some basic tasks. You can control the ON/OFF state of the dehumidifier in your
living room from Home Assistant's front-end. But in many cases, controlling everything strictly from the frontend is
not desirable. That's why you've also installed a simple push button next to the dehumidifier wired to pin GPIOXX.
A simple push of this button should toggle the state of the dehumidifier.

You *could* write an automation to do this task in Home Assistant's automation engine, but IoT devices should not
depend on network connections to perform their jobs -- especially not for something as simple as switching on/off a
dehumidifier.

With ESPHome's automation engine, you can define automations using a syntax that is (hopefully) about as easy to use
as Home Assistant's. For example, this configuration would achieve your desired behavior for the dehumidifier:

```yaml
switch:
  - platform: gpio
    pin: GPIOXX
    name: "Living Room Dehumidifier"
    id: dehumidifier1

binary_sensor:
  - platform: gpio
    pin: GPIOXX
    name: "Living Room Dehumidifier Toggle Button"
    on_press:
      then:
        - switch.toggle: dehumidifier1
```

Let's step through what's happening here:

```yaml
switch:
   - platform: gpio
     # ...
     id: dehumidifier1
```

First, we have to give the dehumidifier `switch` an [ID](/guides/configuration-types#id) so that we can refer to it inside of our
automation.

{{< anchor "actions-trigger" >}}

## Triggers

```yaml
binary_sensor:
  - platform: gpio
    # ...
    on_press:
```

We now attach a special attribute `on_press` to the binary sensor (which represents the button). This part is called
a "trigger". In this example, the *automation* which follows on the next few lines will execute whenever someone
*begins* to press the button. Note the terminology follows what you would call these events on mouse buttons. A *press*
happens when you begin pressing the button. There are also other triggers like `on_release`, `on_click` or
`on_double_click` available.

```yaml
# ...
on_press:
  then:
    - switch.toggle: dehumidifier1
```

{{< anchor "actions-action" >}}

## Actions

Now comes the actual automation block. With `then`, you tell ESPHome what should happen when the press happens.
Within this block, you can define several "actions" that will be executed sequentially. For example, `switch.toggle`
and the line after that form an action. Each action is separated by a dash and multiple actions can be executed in
sequence simply by adding another `-` like so:

```yaml
# ...
on_press:
  then:
    - switch.toggle: dehumidifier1
    - delay: 2s
    - switch.toggle: dehumidifier1
```

With this automation, a press of the push button would cause the dehumidifier to turn on/off for 2 seconds, and then
cycle back to its original state. You can also have a single trigger with multiple automations:

```yaml
# ...
on_press:
  - then:
      - switch.toggle: dehumidifier1
  - then:
      - light.toggle: dehumidifier_indicator_light

# Same as:
on_press:
  then:
    - switch.toggle: dehumidifier1
    - light.toggle: dehumidifier_indicator_light
```

As a final example, let's make our dehumidifier "smart". Let's make it turn on automatically when
the humidity reported by a sensor is above 65% and make it turn off again when it falls below 50%:

```yaml
sensor:
  - platform: dht
    humidity:
      name: "Living Room Humidity"
      on_value_range:
        - above: 65.0
          then:
            - switch.turn_on: dehumidifier1
        - below: 50.0
          then:
            - switch.turn_off: dehumidifier1
    temperature:
      name: "Living Room Temperature"
```

That's a lot of indentation. 😉

`on_value_range` is a special trigger for sensors that triggers when the value of the sensor is within/above/below
the specified range. In the first example, this range is defined as "any value above or including 65.0" and the second
range refers to any (humidity) value 50% or below.

Finally, for the cases where the "pure" YAML automations just don't quite reach far enough, ESPHome has another
extremely powerful tool to offer: {{< docref "templates/" >}}.

Now that concludes the introduction to actions in ESPHome. They're a powerful tool to automate almost everything on
your device with an easy-to-use syntax. What follows below is an index of common actions which you're sure to find
useful (and even essential) for building all sorts of automations.

{{< anchor "common-actions" >}}

## Common Actions

{{< anchor "delay_action" >}}

### `delay` Action

This action delays the execution of the next action in the action list by a specified
time period.

```yaml
on_...:
  then:
    - switch.turn_on: relay_1
    - delay: 2s
    - switch.turn_off: relay_1
    # Templated, waits for 1s (1000ms) only if a reed switch is active
    - delay: !lambda "if (id(reed_switch).state) return 1000; else return 0;"
```

> [!NOTE]
> This is a "smart" asynchronous delay - other code will still run in the background while
> the delay is happening. When using a lambda call, you should return the delay value in milliseconds.

{{< anchor "if_action" >}}

### `if` Action

This action first evaluates the `condition:` and then either
executes the `then:` branch if the condition returns true or the `else:` branch if the condition returns false.

After the chosen branch (`then` or `else`  ) is done with execution, the next action is performed.

For example below you can see an automation that checks if a sensor value is below 30 and if so
turns on a light for 5 seconds. Otherwise, the light is turned off immediately.

```yaml
on_...:
  then:
    - if:
        condition:
          lambda: 'return id(some_sensor).state < 30;'
        then:
          - logger.log: "The sensor value is below 30!"
          - light.turn_on: my_light
          - delay: 5s
        else:
          - logger.log: "The sensor value is above 30!"
    - light.turn_off: my_light
```

#### Configuration variables

At least one of `condition`, `all` or `any` must be provided.

- **condition** (*Optional*, [Condition](#all-conditions)): The condition to check to determine which branch to take.
  If this is configured with a list of conditions then they must all be true for the condition to be true.

- **all** (*Optional*, [Condition](#all-conditions)): Takes a list of conditions, all of which must be true (and is
  therefore equivalent to `condition`  .)

- **any** (*Optional*, [Condition](#all-conditions)): Takes a list of conditions; if at least one is true, the
  condition will be true.

- **then** (*Optional*, [Action](#all-actions)): The action to perform if the condition evaluates to true.
  Defaults to doing nothing.

- **else** (*Optional*, [Action](#all-actions)): The action to perform if the condition evaluates to false.
  Defaults to doing nothing.

{{< anchor "lambda_action" >}}

### `lambda` Action

This action executes an arbitrary piece of C++ code (see [Lambda](/automations/templates#config-lambda)).

```yaml
on_...:
  then:
    - lambda: |-
        id(some_binary_sensor).publish_state(false);
```

{{< anchor "repeat_action" >}}

### `repeat` Action

This action allows you to repeat a block a given number of times.
For example, the automation below will flash the light five times.

```yaml
on_...:
  - repeat:
      count: 5
      then:
        - lambda: ESP_LOGI("main", "Turning lights on for iteration [%d]", iteration);
        - light.turn_on: some_light
        - delay: 1s
        - lambda: ESP_LOGI("main", "Turning lights off for iteration [%d]", iteration);
        - light.turn_off: some_light
        - delay: 10s
```

#### Configuration variables

- **count** (**Required**, int): The number of times the action should be repeated. The counter is available to
  lambdas using the implicit script parameter `iteration`.

- **then** (**Required**, [Action](#all-actions)): The action to repeat.

{{< anchor "wait_until_action" >}}

### `wait_until` Action

This action allows your automations to wait until a condition evaluates to true. (So this is just
a shorthand way of writing a `while` action with an empty `then` block.)

```yaml
# In a trigger:
on_...:
  - logger.log: "Waiting for binary sensor"
  - wait_until:
      binary_sensor.is_on: some_binary_sensor
  - logger.log: "Binary sensor is ready"
```

If you want to use a timeout, the term "condition" is required:

```yaml
# In a trigger:
on_...:
  - logger.log: "Waiting for binary sensor"
  - wait_until:
      condition:
        binary_sensor.is_on: some_binary_sensor
      timeout: 8s
  - logger.log: "Binary sensor might be ready"
```

#### Configuration variables

- **condition** (**Required**, [Condition](#all-conditions)): The condition to wait to become true.
- **timeout** (*Optional*, [Time](/guides/configuration-types#time)): Time to wait before timing out. Defaults to never timing out.

{{< anchor "while_action" >}}

### `while` Action

This action is similar to the [if](#if_action) Action. The `while` action loops
through a block as long as the given condition is true.

```yaml
# In a trigger:
on_...:
  - while:
      condition:
        binary_sensor.is_on: some_binary_sensor
      then:
      - logger.log: "Still executing"
      - light.toggle: some_light
      - delay: 5s
```

#### Configuration variables

- **condition** (**Required**, [Condition](#all-conditions)): The condition to check to determine whether or not to
  execute.

- **then** (**Required**, [Action](#all-actions)): The action to perform until the condition evaluates to false.

{{< anchor "component-update_action" >}}

### `component.update` Action

Using this action you can manually call the `update()` method of a component.

Please note that this only works with some component types and others will result in a
compile error.

```yaml
on_...:
  then:
    - component.update: my_component

    # The same as:
    - lambda: 'id(my_component).update();'
```

{{< anchor "component-suspend_action" >}}

### `component.suspend` Action

Using this action you can manually call the `stop_poller()` method of a component.

After this action the component will stop being refreshed.

While the poller is suspendend, it's still possible to trigger on-demand updates by
using [component.update](#component-update_action)

Please note that this only works with PollingComponent types and others will result in a
compile error.

```yaml
on_...:
  then:
    - component.suspend: my_component

    # The same as:
    - lambda: 'id(my_component).stop_poller();'
```

{{< anchor "component-resume_action" >}}

### `component.resume` Action

Using this action you can manually call the `start_poller()` method of a component.

After this action the component will refresh at the original update_interval rate

This will allow the component to resume automatic update at the defined interval.

This action also allows to change the update interval, calling it without suspend,
replace the poller directly.

Please note that this only works with PollingComponent types and others will result in a
compile error.

```yaml
on_...:
  then:
    - component.resume: my_component

    # The same as:
    - lambda: 'id(my_component).start_poller();'

# Change the poller interval
on_...:
  then:
    - component.resume:
        id: my_component
        update_interval: 15s
```

{{< anchor "common_conditions" >}}

## Common Conditions

"Conditions" provide a way for your device to take an action only when a specific (set of) condition(s) is satisfied.

{{< anchor "and_condition" >}}
{{< anchor "all_condition" >}}
{{< anchor "or_condition" >}}
{{< anchor "any_condition" >}}
{{< anchor "xor_condition" >}}
{{< anchor "not_condition" >}}

### `and` / `all` / `or` / `any` / `xor` / `not` Condition

Check a combination of conditions. `all` is a synonym for `and`, and `any` is a synonym for `or`.
`all` and `any` may also be used directly in place of `condition`.

```yaml
on_...:
  then:
    - if:
        condition:
          # Same syntax for `and` as well as `xor` conditions
          or:
            - binary_sensor.is_on: some_binary_sensor
            - binary_sensor.is_on: other_binary_sensor
        # ...

    - if:
        any:
          - not:
              binary_sensor.is_off: some_binary_sensor
          - binary_sensor.is_on: some_other_sensor
```

{{< anchor "for_condition" >}}

### `for` Condition

Allows you to check if a given condition has been true for at least a given amount of time.

```yaml
on_...:
  if:
    condition:
      for:
        time: 5min
        condition:
          api.connected:
    then:
      - logger.log: API has stayed connected for at least 5 minutes!
```

#### Configuration variables

- **time** (**Required**, [templatable](/automations/templates), [Time](/guides/configuration-types#time)):
  The time for which the condition has to have been true.

- **condition** (**Required**, [condition](#all-conditions)): The condition to check.

### `component.is_idle` Condition

This condition checks if a given component is idle. A component is considered to be idle if it has completed
setup, has not been marked as failed, and is not currently being called by the loop task. This is useful for
synchronizing actions with the state of the component, for example, an e-paper display component that requires
a significant amount of time to update the display panel.

```yaml
on_...:
  then:
    - if:
        condition:
          component.is_idle: some_component
        # ...
```

### `lambda` Condition

This condition performs an arbitrary piece of C++ code (see [Lambda](/automations/templates#config-lambda))
and can be used to create conditional flow in actions.

```yaml
on_...:
  then:
    - if:
        condition:
          # Should return either true or false
          lambda: |-
            return id(some_sensor).state < 30;
        # ...
```

{{< anchor "config-action" >}}

## All Actions

{{< render-automations "actions" >}}
{{< anchor "config-condition" >}}

## All Conditions

{{< render-automations "conditions" >}}
{{< anchor "tips-and-tricks" >}}

## Tips and Tricks

{{< anchor "automation-networkless" >}}

### Do Automations Work Without a Network Connection

This is a common question and the answer is **YES!** All automations you define in ESPHome are executed on the
microcontroller itself and will continue to work even if the Wi-Fi network is down or the MQTT server is not reachable.

There is one caveat though: ESPHome will automatically reboot periodically if no connection is made to its API. This
helps in the event that there is an issue in the device's network stack preventing it from being reachable on the
network. You can adjust this behavior (or even disable automatic rebooting) using the `reboot_timeout` option in any
of the following components:

- {{< docref "/components/wifi" >}}
- {{< docref "/components/api" >}}
- {{< docref "/components/mqtt" >}}

Beware, however, that disabling the reboot timeout(s) effectively disables the reboot watchdog, so you will need to
power-cycle the device if it proves to be/remain unreachable on the network.

{{< anchor "timers-timeouts" >}}

### Timers and Timeouts

While ESPHome does not provide a construction for timers, you can easily implement them by
combining `script` and `delay`. You can have an absolute timeout or sliding timeout by
using script modes `single` and `restart` respectively.

```yaml
script:
  - id: hallway_light_script
    mode: restart     # Light will be kept on during 1 minute since
                      # the latest time the script is executed
    then:
      - light.turn_on: hallway_light
      - delay: 1 min
      - light.turn_off: hallway_light

...
  on_...:           # can be called from different wall switches
    - script.execute: hallway_light_script
```

Sometimes you'll also need a timer which does not perform any action; in this case, you can use a single `delay`
action and then (in your automation) use the `script.is_running` condition to know if your "timer" is active or not.

## See Also

- {{< docref "index/" >}}
- {{< docref "templates/" >}}
