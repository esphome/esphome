---
description: "Instructions for setting up Template Select(s) with ESPHome."
title: "Template Select"
params:
  seo:
    description: Instructions for setting up Template Select(s) with ESPHome.
    image: description.svg
---

The `template` Select platform allows you to create a Select with templated values
using [lambdas](/automations/templates#config-lambda).

```yaml
# Example configuration entry
select:
  - platform: template
    name: "Template select"
    optimistic: true
    options:
      - one
      - two
      - three
    initial_option: two
```

## Configuration variables

- **options** (**Required**, list): The list of options this Select has.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated every update interval to get the current option of the select.

- **set_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should
  be performed when the remote (like Home Assistant's frontend) requests to set the Select option.
  The new option is available to lambdas in the `x` variable.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval on which to update the select
  by executing the `lambda`. Defaults to `60s`.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode,
  any command sent to the Template Select will immediately update the reported state.
  Cannot be used with `lambda`. Defaults to `false`.

- **restore_value** (*Optional*, boolean): Saves and loads the state to RTC/Flash.
  Cannot be used with `lambda`. Defaults to `false`.

- **initial_option** (*Optional*, string): The option to set the option to on setup if not
  restored with `restore_value`.
  Cannot be used with `lambda`. Defaults to the first option in the `options` list.

- All other options from [Select](/components/select#config-select).

> [!NOTE]
> If you don't set a `lambda` and `optimistic` is `false` (default), updates to the select component state will need to be taken care of as part of your `set_action` using `id(my_select).publish_state(x);` (in a lambda). Do not use [`select.set` Action](/components/select#select-set_action) here, as this would generate a loop.

## `select.set` Action

You can also set an option for the template select from elsewhere in your YAML file
with the [`select.set` Action](/components/select#select-set_action).

## See Also

- [Automation](/automations)
- {{< apiref "template/select/template_select.h" "template/select/template_select.h" >}}
