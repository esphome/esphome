---
description: "Instructions for setting up template datetime with ESPHome."
title: "Template Datetime"
params:
  seo:
    description: Instructions for setting up template datetime with ESPHome.
    image: description.svg
---

The `template` datetime platform allows you to create a datetime with templated values
using [lambdas](/automations/templates#config-lambda).

```yaml
datetime:
  # Example Date
  - platform: template
    id: my_datetime_date
    type: date
    name: Pick a Date
    optimistic: yes
    initial_value: "2024-01-30"
    restore_value: true

  # Example Time
  - platform: template
    id: my_datetime_time
    type: time
    name: Pick a Time
    optimistic: yes
    initial_value: "12:34:56"
    restore_value: true

  # Example DateTime
  - platform: template
    id: my_datetime
    type: datetime
    name: Pick a DateTime
    optimistic: yes
    initial_value: "2024-12-31 12:34:56"
    restore_value: true
```

## Configuration variables

- **type** (**Required**, enum): The type of the datetime. Can be one of `date` or `time`.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated every update interval to get the current value of the datetime.

- **set_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should
  be performed when the remote (like Home Assistant's frontend) requests to set the
  dateime value. The new value is available to lambdas in the `x` variable.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval on which to update the datetime
  by executing the `lambda`. Defaults to `60s`.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode,
  any command sent to the template datetime will immediately update the reported state.
  Cannot be used with `lambda`. Defaults to `false`.

- **restore_value** (*Optional*, boolean): Saves and loads the state to RTC/Flash.
  Cannot be used with `lambda`. Defaults to `false`.

- **initial_value** (*Optional*, string): The value to set the state to on setup if not
  restored with `restore_value`. Can be one of:

  - For `type: date`  :

    - A string in the format `%Y-%m-%d`, eg: `"2023-12-04"`.
    - An object including `year`, `month`, `day`.

```yaml
        initial_value:
          year: 2023
          month: 12
          day: 4
```

- For `type: time`  :

  - A string in the format `%H:%M:%S`, eg: `"12:34:56"`.
  - An object including `hour`, `minute`, `second`.

```yaml
        initial_value:
          hour: 12
          minute: 34
          second: 56
```

- For `type: datetime`  :

  - A string in the format `%Y-%m-%d %H:%M:%S`, eg: `"2023-12-04 12:34:56"`.
  - An object including `year`, `month`, `day`, `hour`, `minute`, `second`.

```yaml
        initial_value:
          year: 2023
          month: 12
          day: 4
          hour: 12
          minute: 34
          second: 56
```

- All other options from [Datetime](/components/datetime#config-datetime).

## See Also

- [Automation](/automations)
- {{< apiref "template/datetime/template_date.h" "template/datetime/template_date.h" >}}
