---
description: "Instructions for setting up sensor components in ESPHome."
title: "Sensor Component"
params:
  seo:
    description: Instructions for setting up sensor components in ESPHome.
    image: folder-open.svg
---

ESPHome has support for many different sensors. Each of them is a
platform of the `sensor` domain and each sensor has several base
configuration options.

{{< anchor "config-sensor" >}}

## Base Sensor Configuration

All sensors in ESPHome have a name and some other
optional configuration options. By default, the sensor platform will
chose appropriate values for all of these by default, but you can always
override them if you want to.

```yaml
# Example sensor configuration
name: Livingroom Temperature

# Optional variables:
unit_of_measurement: "°C"
icon: "mdi:water-percent"
device_class: "temperature"
state_class: "measurement"
accuracy_decimals: 1
expire_after: 30s
filters:
  - sliding_window_moving_average:
      window_size: 15
      send_every: 15
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the sensor. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the sensor to use that name, you can set `name: None`.

- **unit_of_measurement** (*Optional*, string): Manually set the unit
  of measurement the sensor should advertise its values with. This does
  not actually do any maths (conversion between units).

- **device_class** (*Optional*, string): The device class for the
  sensor. See <https://www.home-assistant.io/integrations/sensor/#device-class>
  for a list of available options. Set to `""` to remove the default device class of a sensor.

- **state_class** (*Optional*, string): The state class for the
  sensor. See <https://developers.home-assistant.io/docs/core/entity/sensor/#available-state-classes>
  for a list of available options. Set to `""` to remove the default state class of a sensor.

- **icon** (*Optional*, icon): Manually set the icon to use for the sensor in the frontend.
- **accuracy_decimals** (*Optional*, int): Set the number of digits
  after the decimal point that data consumers should use. While this
  does not change the value sent across the API, Home Assistant's
  ESPHome integration will round the value accordingly before using or
  storing it. The number of digits will also be used to set the
  entity's default display precision. Thus, one can display fewer
  decimals, but cannot increase precision of sensor values in Home
  Assistant. (The default value varies depending on component and
  sensor.)

- **filters** (*Optional*): Specify filters to use for some basic
  transforming of values. See [Sensor Filters](#sensor-filters) for more information.

- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **force_update** (*Optional*, boolean): If true, this option will force the frontend (usually Home
  Assistant) to create a state changed event when the sensor updates even if the value stayed the same.
  Some applications like Grafana require this when working with Home Assistant, but beware it can
  significantly increase the database size. Defaults to `false`.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options.
  Set to `""` to remove the default entity category.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

Automations:

- **on_value** (*Optional*, [Automation](/automations)): An automation to perform
  when a new value is published. See [`on_value`](#sensor-on_value).

- **on_value_range** (*Optional*, [Automation](/automations)): An automation to perform
  when a published value transition from outside to a range to inside. See [`on_value_range`](#sensor-on_value_range).

- **on_raw_value** (*Optional*, [Automation](/automations)): An automation to perform
  when a raw value is received that hasn't passed through any filters. See [`on_raw_value`](#sensor-on_raw_value).

MQTT Options:

- **expire_after** (*Optional*, [Time](/guides/configuration-types#time)): Manually set the time in which
  the sensor values should be marked as “expired”/“unknown”. Not providing any value means no expiry.

- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

> [!NOTE]
> If you're trying to setup filters for a sensor that has multiple outputs - for example a DHT22 which
> reports temperature *and* humidity - put the `filters` option into each sensor output like this:
>
> ```yaml
> sensor:
>   - platform: dht
>     # ...
>     temperature:
>       filters:
>         # ...
>     humidity:
>       filters:
>         # ...
> ```

{{< anchor "sensor-filters" >}}

## Sensor Filters

ESPHome lets you pre-process sensor values before sending them to Home Assistant. This is useful, for example, if you want to apply an average to the last few readings.

Many filters are available for sensors, which you can define by adding a `filters` block in the sensor configuration (at the same level as `platform` or within each sensor block for platforms with multiple sensors).

Filters are applied in the order they are defined in your configuration.

```yaml
# Example filters:
filters:
  - offset: 2.0
  - multiply: !lambda return 1.2;
  - calibrate_linear:
      - 0.0 -> 0.0
      - 40.0 -> 45.0
      - 100.0 -> 102.5
  - filter_out:
      - 42.0
      - 43.0
  - median:
      window_size: 5
      send_every: 5
      send_first_at: 1
  - quantile:
      window_size: 5
      send_every: 5
      send_first_at: 1
      quantile: .9
  - sliding_window_moving_average:
      window_size: 15
      send_every: 15
  - exponential_moving_average:
      alpha: 0.1
      send_every: 15
  - throttle: 1s
  - throttle_average: 1s
  - throttle_with_priority:
      timeout: 1s
      value:
        - 42.0
        - 43.0
  - heartbeat: 5s
  - debounce: 0.1s
  - timeout: 1min
  - delta: 5.0
  - or:
    - throttle: 1s
    - delta: 5.0
  - lambda: return x * (9.0/5.0) + 32.0;
```

### `calibrate_linear`

{{< include "filter/calibrate_linear.md" >}}

### `calibrate_polynomial`

{{< include "filter/calibrate_polynomial.md" >}}

### `clamp`

{{< include "filter/clamp.md" >}}

### `debounce`

{{< include "filter/debounce.md" >}}

### `delta`

{{< include "filter/delta.md" >}}

### `exponential_moving_average`

{{< include "filter/exponential_moving_average.md" >}}

### `filter_out`

{{< include "filter/filter_out.md" >}}

### `heartbeat`

{{< include "filter/heartbeat.md" >}}

### `lambda`

{{< include "filter/lambda.md" >}}

### `max`

{{< include "filter/max.md" >}}

### `median`

{{< include "filter/median.md" >}}

### `min`

{{< include "filter/min.md" >}}

### `multiply`

{{< include "filter/multiply.md" >}}

### `offset`

{{< include "filter/offset.md" >}}

### `or`

{{< include "filter/or.md" >}}

### `quantile`

{{< include "filter/quantile.md" >}}

### `round`

{{< include "filter/round.md" >}}

### `round_to_multiple_of`

{{< include "filter/round_to_multiple_of.md" >}}

### `skip_initial`

{{< include "filter/skip_initial.md" >}}

### `sliding_window_moving_average`

{{< include "filter/sliding_window_moving_average.md" >}}

### `throttle`

{{< include "filter/throttle.md" >}}

### `throttle_average`

{{< include "filter/throttle_average.md" >}}

### `throttle_with_priority`

{{< include "filter/throttle_with_priority.md" >}}

### `timeout`

{{< include "filter/timeout.md" >}}

### `to_ntc_resistance`

{{< include "filter/to_ntc_resistance.md" >}}

### `to_ntc_temperature`

{{< include "filter/to_ntc_temperature.md" >}}

## Example: Converting Celsius to Fahrenheit

While I personally don't like the Fahrenheit temperature scale, I do
understand that having temperature values appear in the Fahrenheit unit
is quite useful to some users. ESPHome uses the Celsius temperature
unit internally, and I'm not planning on making converting between the
two simple (😉), but you can use this filter to convert Celsius values to
Fahrenheit.

```yaml
filters:
  - lambda: return x * (9.0/5.0) + 32.0;
unit_of_measurement: "°F"
```

{{< anchor "sensor-automations" >}}

## Sensor Automation

You can access the most recent state of the sensor in [lambdas](/automations/templates#config-lambda) using
`id(sensor_id).state` and the most recent raw state using `id(sensor_id).raw_state`.

{{< anchor "sensor-on_value" >}}

### `on_value`

This automation will be triggered when a new value that has passed through all filters
is published. In [Lambdas](/automations/templates#config-lambda) you can get the value from the trigger
with `x`.

```yaml
sensor:
  - platform: dht
    # ...
    on_value:
      then:
        - light.turn_on:
            id: light_1
            red: !lambda "return x/255;"
```

Configuration variables: See [Automation](/automations).

{{< anchor "sensor-on_value_range" >}}

### `on_value_range`

With this automation you can observe if a sensor value passes from outside
a defined range of values to inside a range. For example you can have an
automation that triggers when a humidity crosses a threshold, and then turns on a dehumidifier.
This trigger will only trigger when the new value is inside the range and the previous value
was outside the range. On startup, the last state before reboot is restored and if the value crossed
the boundary during the boot process, the trigger is also executed.

Define the range with `above` and `below`. If only one of them is defined, the interval is half-open.
So for example `above: 5` with no below would mean the range from 5 to positive infinity.

```yaml
sensor:
  - platform: dht
    # ...
    on_value_range:
      - below: 5.0
        then:
          - switch.turn_on: relay_1
      - above: 5.0
        below: 10.0
        then:
          - switch.turn_on: relay_2
      - above: 10.0
        then:
          - switch.turn_on: relay_3
```

Configuration variables:

- **above** (*Optional*, float): The minimum for the trigger.
- **below** (*Optional*, float): The maximum for the trigger.
- See [Automation](/automations).

{{< anchor "sensor-on_raw_value" >}}

### `on_raw_value`

This automation will be triggered when a new value is received that hasn't passed
through any filters. In [Lambdas](/automations/templates#config-lambda) you can get the value from the
trigger with `x`.

```yaml
sensor:
  - platform: dht
    # ...
    on_raw_value:
      then:
        - light.turn_on:
            id: light_1
            red: !lambda "return x/255;"
```

Configuration variables: See [Automation](/automations).

{{< anchor "sensor-in_range_condition" >}}

### `sensor.in_range` Condition

This condition passes if the state of the given sensor is inside a range.

Define the range with `above` and `below`. If only one of them is defined, the interval is half-open.
So for example `above: 5` with no below would mean the range from 5 to positive infinity.

```yaml
# in a trigger:
on_...:
  if:
    condition:
      sensor.in_range:
        id: my_sensor
        above: 50.0
    then:
    - script.execute: my_script
```

Configuration variables:

- **above** (*Optional*, float): The minimum for the condition.
- **below** (*Optional*, float): The maximum for the condition.

{{< anchor "sensor-lambda_calls" >}}

### Lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all sensors to do some
advanced stuff (see the full API Reference for more info).

- `publish_state()`  : Manually cause the sensor to push out a value. It will then
  be processed by the sensor filters, and once filtered will propagate though ESPHome and though the API to Home Assistant or out via MQTT if configured.

```cpp
    // Within lambda, push a value of 42.0
    id(my_sensor).publish_state(42.0);
```

- `.state`  : Retrieve the current value of the sensor that has passed through all sensor filters.
  Is `NAN` if no value has gotten through all filters yet.

```cpp
    // For example, create a custom log message when a value is received:
    ESP_LOGI("main", "Value of my sensor: %f", id(my_sensor).state);
```

- `raw_state`  : Retrieve the current value of the sensor that has not passed through any filters.
  Is `NAN` if no value has been pushed by the sensor itself yet.

```cpp
    // For example, create a custom log message when a value is received:
    ESP_LOGI("main", "Raw Value of my sensor: %f", id(my_sensor).raw_state);
```

## See Also

- {{< apiref "sensor/sensor.h" "sensor/sensor.h" >}}
