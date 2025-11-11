---
description: "Instructions for setting up template valves in ESPHome."
title: "Template Valve"
params:
  seo:
    description: Instructions for setting up template valves in ESPHome.
    image: description.svg
---

The `template` valve platform allows you to create simple valves out of just a few actions and a value lambda. Once
defined, it will automatically appear in Home Assistant as a valve and can be controlled through the frontend.

{{< img src="valve-ui.png" alt="Image" class="align-center" >}}

```yaml
# Example configuration entry
valve:
  - platform: template
    name: "Template Valve"
    lambda: |-
      if (id(top_end_stop).state) {
        return VALVE_OPEN;
      } else {
        return VALVE_CLOSED;
      }
    open_action:
      - switch.turn_on: open_valve_switch
    close_action:
      - switch.turn_on: close_valve_switch
    stop_action:
      - switch.turn_on: stop_valve_switch
    optimistic: true
```

Possible return values for the optional lambda:

- `return VALVE_OPEN;` if the valve should be reported as OPEN.
- `return VALVE_CLOSED;` if the valve should be reported as CLOSED.
- `return {};` if the last state should be repeated.

## Configuration variables

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated repeatedly to get the current state of the valve.

- **open_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should be performed when the remote
  (like Home Assistant's frontend) requests the valve to be opened.

- **close_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should be performed when the remote
  requests the valve to be closed.

- **stop_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should be performed when the remote
  requests the valve to be stopped.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode, any command sent to
  the template valve will immediately update the reported state and no lambda needs to be used. Defaults to `false`.

- **restore_mode** (*Optional*, enum): Control how the valve attempts to restore state on bootup.

  - `NO_RESTORE` (Default): Do not save or restore state.
  - `RESTORE`  : Attempts to restore the state on startup, but doesn't instruct the valve to return to that state.
  - `RESTORE_AND_CALL`  : Attempts to restore the state on startup and instructs the valve to return to the restored state.

- **assumed_state** (*Optional*, boolean): Whether the true state of the valve is not known. This will make the Home
  Assistant frontend show buttons for both OPEN and CLOSE actions, instead of hiding one of them. Defaults to `false`.

- **has_position** (*Optional*, boolean): Whether this valve will publish its position as a floating point number.
  By default (`false`  ), the valve only publishes OPEN/CLOSED position.

- **position_action** (*Optional*, [Action](/automations/actions#all-actions)): The action that should be performed when the remote
  (like Home Assistant's frontend) requests the valve be set to a specific position. The desired position is available
  in the lambda in the `pos` variable. Requires `has_position` (above) to be set to `true`.

- All other options from [Valve](/components/valve#config-valve).

{{< anchor "valve-template-publish_action" >}}

## `valve.template.publish` Action

You can also publish a state to a template valve from elsewhere in your YAML filewith the `valve.template.publish` action.

```yaml
# Example configuration entry
valve:
  - platform: template
    name: "Template Valve"
    id: my_template_valve

# in some trigger
on_...:
  - valve.template.publish:
      id: my_template_valve
      state: OPEN

  # Templated
  - valve.template.publish:
      id: my_template_valve
      state: !lambda 'return VALVE_OPEN;'
```

Configuration options:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the template valve.
- **state** (*Optional*, [templatable](/automations/templates)):
  The state to publish. One of `OPEN`, `CLOSED`. If using a lambda, use `VALVE_OPEN` or `VALVE_CLOSED`.

- **position** (*Optional*, [templatable](/automations/templates), float):
  The position to publish, from 0 (CLOSED) to 1.0 (OPEN)

- **current_operation** (*Optional*, [templatable](/automations/templates), string):
  The current operation mode to publish. One of `IDLE`, `OPENING` and `CLOSING`. If using a lambda, use
  `VALVE_OPERATION_IDLE`, `VALVE_OPERATION_OPENING`, and `VALVE_OPERATION_CLOSING`.

> [!NOTE]
> This action can also be written in lambdas:
>
> ```cpp
> id(my_template_valve).position = VALVE_OPEN;
> id(my_template_valve).publish_state();
> ```

## See Also

- {{< docref "/components/valve" >}}
- [Automation](/automations)
- {{< docref "/cookbook/garage-door" >}}
- {{< apiref "template/valve/template_valve.h" "template/valve/template_valve.h" >}}
