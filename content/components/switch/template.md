---
description: "Instructions for setting up template switches that can execute arbitrary actions when turned on or off."
title: "Template Switch"
params:
  seo:
    description: Instructions for setting up template switches that can execute arbitrary actions when turned on or off.
    image: description.svg
---

The `template` switch platform allows you to create simple switches out of just actions and
an optional value lambda. Once defined, it will automatically appear in Home Assistant
as a switch and can be controlled through the frontend.

```yaml
# Example configuration entry
switch:
  - platform: template
    name: "Template Switch"
    lambda: |-
      if (id(some_binary_sensor).state) {
        return true;
      } else {
        return false;
      }
    turn_on_action:
      - switch.turn_on: switch2
    turn_off_action:
      - switch.turn_on: switch1
```

Possible return values for the optional lambda:

- `return true;` if the switch should be reported as ON.
- `return false;` if the switch should be reported as OFF.
- `return {};` if the last state should be repeated.

## Configuration variables

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated repeatedly to get the current state of the switch.

- **turn_on_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should
  be performed when the remote (like Home Assistant's frontend) requests the switch to be turned on.

- **turn_off_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should
  be performed when the remote (like Home Assistant's frontend) requests the switch to be turned off.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode,
  any command sent to the template switch will immediately update the reported state.
  Defaults to `false`.

- **assumed_state** (*Optional*, boolean): Whether the true state of the switch is not known.
  This will make the Home Assistant frontend show buttons for both ON and OFF actions, instead
  of hiding one of them when the switch is ON/OFF. Defaults to `false`.

- All other options from [Switch](/components/switch#config-switch).

{{< anchor "switch-template-publish_action" >}}

## `switch.template.publish` Action

You can also publish a state to a template switch from elsewhere in your YAML file
with the `switch.template.publish` action.

```yaml
# Example configuration entry
switch:
  - platform: template
    name: "Template Switch"
    id: template_swi

# in some trigger
on_...:
  - switch.template.publish:
      id: template_swi
      state: ON

  # Templated
  - switch.template.publish:
      id: template_swi
      state: !lambda 'return true;'
```

Configuration options:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the template switch.
- **state** (**Required**, boolean, [templatable](/automations/templates)):
  The state to publish.

> [!NOTE]
> This action can also be written in lambdas, the parameter of the `publish_state` method denotes if
> the switch is currently on or off:
>
> ```cpp
> id(template_swi).publish_state(false);
> ```

## See Also

- {{< docref "/automations" >}}
- {{< docref "/components/switch" >}}
- {{< docref "/components/binary_sensor" >}}
- {{< apiref "template/switch/template_switch.h" "template/switch/template_switch.h" >}}
