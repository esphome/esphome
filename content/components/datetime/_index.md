---
description: "Instructions for setting up datetime components in ESPHome."
title: "Datetime Component"
params:
  seo:
    description: Instructions for setting up datetime components in ESPHome.
    image: folder-open.svg
---

ESPHome has support for components to create a datetime entity. A datetime entity
currently represents a date that can be set by the user/frontend.

> [!NOTE]
> Requires Home Assistant 2024.4 or newer.

{{< anchor "config-datetime" >}}

## Base Datetime Configuration

All datetime in ESPHome have a name and an optional icon.

```yaml
# Example datetime configuration
name: Date to check

# Optional variables:
icon: "mdi:calendar-alert"
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the datetime. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the datetime to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the datetime in the frontend.
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

- **time_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the time entity. Automatically set
  to the ID of a time component if only a single one is defined. Required if `on_time` is used.

- If Webserver enabled and version 3 is selected, All other options from Webserver Component.. See [Webserver Version 3](/components/web_server#config-webserver-version-3-options).

MQTT Options:

- All other options from [MQTT Component](/components/mqtt#config-mqtt-component).

Time and DateTime Options:

- **on_time** (*Optional*, [Automation](/automations)): Automation to run when the current datetime or time matches the current state.
  Only valid on `time` or `datetime` types. Use of `on_time` causes `time_id` to be required, `time_id` will be automatically assigned if a time source exists in the config, and will cause an invalid configuration if there is no {{< docref "/components/time" >}} configured.

## Automation

You can access the most recent state as a `ESPTime` object by `id(datetime_id).state_as_esptime()`

{{< anchor "datetime-on_value" >}}

### `on_value`

This automation will be triggered when a new value is published. In [Lambdas](/automations/templates#config-lambda)
you can get the value as a ESPTime object from the trigger with `x`.

```yaml
datetime:
  - platform: template
    # ...
    on_value:
      then:
        - lambda: |-
            if(x.hour >= 12) {
              ESP_LOGD("main", "Updated hour is later or equal to 12");
            } else {
              ESP_LOGD("main", "Updated hour is earlier than 12");
            }
```

Configuration variables: See [Automation](/automations).

## Date Automation

{{< anchor "datetime-date_set_action" >}}

### `datetime.date.set` Action

This is an [Action](/automations/actions#all-actions) for setting a datetime date state.
The `date` provided can be in one of 3 formats:

```yaml
# String date
- datetime.date.set:
    id: my_datetime_date
    date: "2023-12-04"

# Individual date parts
- datetime.date.set:
    id: my_datetime_date
    date:
      year: 2023
      month: 12
      day: 4

# Using a lambda
- datetime.date.set:
    id: my_datetime_date
    date: !lambda |-
      // Return an ESPTime struct
      return {.day_of_month = 4, .month = 12, .year = 2023};
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the datetime to set.
- **date** (**Required**, string, date parts, [templatable](/automations/templates)):
  The value to set the datetime to.

{{< anchor "datetime-lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all datetimes to do some
advanced stuff (see the full API Reference for more info).

- `.make_call()`  : Make a call for updating the datetime value.

```cpp
    // Within lambda, set the date to 2024-02-25
    auto call = id(my_datetime_date).make_call();
    call.set_date("2024-02-25");
    call.perform();
```

  Check the API reference for information on the methods that are available for
  the `DateCall` object.

- `.year`  : Retrieve the current year of the `date`. It will be `0` if no value has been set.
- `.month`  : Retrieve the current month of the `date`. It will be `0` if no value has been set.
- `.day`  : Retrieve the current day of the `date`. It will be `0` if no value has been set.
- `.state_as_esptime()`  : Retrieve the current value of the datetime as a {{< apistruct "ESPTime" "ESPTime" >}} object.

```cpp
    // For example, create a custom log message when a value is received:
    ESP_LOGI("main", "Value of my datetime: %04d-%02d-%02d", id(my_date).year, id(my_date).month, id(my_date).day);
```

## Time Automation

{{< anchor "datetime-time_set_action" >}}

### `datetime.time.set` Action

This is an [Action](/automations/actions#all-actions) for setting a datetime time state.
The `time` provided can be in one of 3 formats:

```yaml
# String time
- datetime.time.set:
    id: my_datetime_time
    time: "12:34:56"

# Individual time parts
- datetime.time.set:
    id: my_datetime_time
    time:
      hour: 12
      minute: 34
      second: 56

# Using a lambda
- datetime.time.set:
    id: my_datetime_time
    time: !lambda |-
      // Return an ESPTime struct
      return {.second = 56, .minute = 34, .hour = 12};
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the datetime to set.
- **time** (**Required**, string, time parts, [templatable](/automations/templates)):
  The value to set the datetime to.

{{< anchor "datetime-time-lambda_calls" >}}

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call several methods on all datetimes to do some
advanced stuff (see the full API Reference for more info).

- `.make_call()`  : Make a call for updating the datetime value.

```cpp
    // Within lambda, set the time to 12:34:56
    auto call = id(my_datetime_time).make_call();
    call.set_time("12:34:56");
    call.perform();
```

  Check the API reference for information on the methods that are available for
  the `TimeCall` object.

- `.hour`  : Retrieve the current hour of the `time`. It will be `0` if no value has been set.
- `.minute`  : Retrieve the current minute of the `time`. It will be `0` if no value has been set.
- `.second`  : Retrieve the current second of the `time`. It will be `0` if no value has been set.
- `.state_as_esptime()`  : Retrieve the current value of the datetime as a {{< apistruct "ESPTime" "ESPTime" >}} object.

```cpp
    // For example, create a custom log message when a value is received:
    ESP_LOGI("main", "Value of my datetime: %0d:%02d:%02d", id(my_datetime_time).hour, id(my_datetime_time).minute, id(my_datetime_time).second);
```

## DateTime Automation

{{< anchor "datetime-datetime_set_action" >}}

### `datetime.datetime.set` Action

This is an [Action](/automations/actions#all-actions) for setting a datetime datetime state.
The `datetime` provided can be in one of 3 formats:

```yaml
# String datetime
- datetime.time.set:
    id: my_datetime
    datetime: "2024-12-31 12:34:56"

# Individual datetime parts
- datetime.datetime.set:
    id: my_datetime
    datetime:
      year: 2024
      month: 12
      day: 31
      hour: 12
      minute: 34
      second: 56

# Using a lambda
- datetime.datetime.set:
    id: my_datetime
    datetime: !lambda |-
      // Return an ESPTime struct
      return {.second = 56, .minute = 34, .hour = 12, .day_of_month = 31, .month = 12, .year = 2024};
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the datetime to set.
- **datetime** (**Required**, string, datetime parts, [templatable](/automations/templates)):
  The value to set the datetime to.

{{< anchor "datetime-datetime-lambda_calls" >}}

### Lambda calls

For more complex use cases, several methods are available for use on datetimes from within [lambdas](/automations/templates#config-lambda). See the full API Reference for more information.

- `.make_call()`  : Make a call for updating the datetime value.

```cpp
    // Within lambda, set the datetime to 2024-12-31 12:34:56
    auto call = id(my_datetime).make_call();
    call.set_datetime("2024-12-31 12:34:56");
    call.perform();
```

  Check the API reference for information on the methods that are available for
  the `DateTimeCall` object.

- `.year`  : Retrieve the current year of the `datetime`. It will be `0` if no value has been set.
- `.month`  : Retrieve the current month of the `datetime`. It will be `0` if no value has been set.
- `.day`  : Retrieve the current day of the `datetime`. It will be `0` if no value has been set.
- `.hour`  : Retrieve the current hour of the `datetime`. It will be `0` if no value has been set.
- `.minute`  : Retrieve the current minute of the `datetime`. It will be `0` if no value has been set.
- `.second`  : Retrieve the current second of the `datetime`. It will be `0` if no value has been set.
- `.state_as_esptime()`  : Retrieve the current value of the datetime as a {{< apistruct "ESPTime" "ESPTime" >}} object.

```cpp
    // For example, create a custom log message when a value is received:
    ESP_LOGI("main", "Value of my datetime: %04d-%02d-%02d %0d:%02d:%02d",
             id(my_datetime).year, id(my_datetime).month, id(my_datetime).day,
             id(my_datetime).hour, id(my_datetime).minute, id(my_datetime).second);
```

## See Also

- {{< apiref "DateTimeBase" "datetime/datetime_base.h" >}}
- {{< apiref "DateEntity" "datetime/date_entity.h" >}}
- {{< apiref "DateCall" "datetime/date_entity.h" >}}
- {{< apiref "TimeEntity" "datetime/time_entity.h" >}}
- {{< apiref "TimeCall" "datetime/time_entity.h" >}}
- {{< apiref "DateTimeEntity" "datetime/datetime_entity.h" >}}
- {{< apiref "DateTimeCall" "datetime/datetime_entity.h" >}}
