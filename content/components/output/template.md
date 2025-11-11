---
description: "Instructions for setting up template outputs with ESPHome."
title: "Template Output"
params:
  seo:
    description: Instructions for setting up template outputs with ESPHome.
    image: description.svg
---

The `template` output component can be used to create templated binary and float outputs in ESPHome.

```yaml
# Example configuration entry
output:
  - platform: template
    id: outputsplit
    type: float
    write_action:
      - output.set_level:
          id: output1
          level: !lambda return state;
      - output.set_level:
          id: output2
          level: !lambda return state;

  - platform: ...
    id: output1
  - platform: ...
    id: output2
```

## Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this output component.
- **type** (**Required**, string): The type of output. One of `binary` and `float`.
- **write_action** (**Required**, [Automation](/automations)): An automation to perform
  when the state of the output is updated.

- All other options from [Output](/components/output#config-output).

See {{< apiclass "output::BinaryOutput" "output::BinaryOutput" >}} and {{< apiclass "output::FloatOutput" "output::FloatOutput" >}}.

> [!WARNING]
> This is an **output component** and will not be visible from the frontend. Output components are intermediary
> components that can be attached to for example lights.

{{< anchor "output-template-on_write_action" >}}

## `write_action` Trigger

When the state for this output is updated, the `write_action` is triggered.
It is possible to access the state value inside Lambdas:

```yaml
- platform: template
    id: my_output
    type: float
    write_action:
      - if:
          condition:
            lambda: return ((state > 0) && (state < .4));
          then:
            - output.turn_on: button_off
            - delay: 500ms
            - output.turn_off: button_off
```

Complete example: [Sonoff Dual Light Switch](https://devices.esphome.io/devices/Sonoff-Dual-DIY-light).

## See Also

- {{< docref "/components/output" >}}
- [Automation](/automations)
