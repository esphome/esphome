---
description: "Instructions for setting up template texts with ESPHome."
title: "Template Text"
params:
  seo:
    description: Instructions for setting up template texts with ESPHome.
    image: description.svg
---

The `template` text platform allows you to create a text with templated values
using [lambdas](/automations/templates#config-lambda).

```yaml
# Example configuration entry
text:
  - platform: template
    name: "Template text"
    optimistic: true
    min_length: 0
    max_length: 100
    mode: text
```

## Configuration variables

- **min_length** (*Optional*, int): The minimum length this text can be. Defaults to `0`.
- **max_length** (*Optional*, int): The maximum length this text can be. Defaults to `255`.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated every update interval to get the current value of the text.

- **set_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should
  be performed when the remote (like Home Assistant's frontend) requests to set the
  text value. The new value is available to lambdas in the `x` variable.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval on which to update the text
  by executing the `lambda`. Defaults to `60s`.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode,
  any command sent to the template text will immediately update the reported state.
  Cannot be used with `lambda`. Defaults to `false`.

- **restore_value** (*Optional*, boolean): Saves and loads the state to RTC/Flash.
  Cannot be used with `lambda`. Defaults to `false`.

- **initial_value** (*Optional*, String): The value to set the state to on setup if not
  restored with `restore_value`.
  Cannot be used with `lambda`.
  Defaults to the empty string.

- All other options from [Text](/components/text#config-text).

## See Also

- [Automation](/automations)
- {{< apiref "template/text/template_text.h" "template/text/template_text.h" >}}
