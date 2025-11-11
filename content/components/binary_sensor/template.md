---
description: "Instructions for setting up template binary sensors."
title: "Template Binary Sensor"
params:
  seo:
    description: Instructions for setting up template binary sensors.
    image: description.svg
---

The `template` binary sensor platform allows you to define a boolean condition and use it to provide a binary sensor.
The condition may be expressed as a C++ lambda, or as a [YAML expression](/automations/actions#all-conditions).
The condition expression will be evaluated continually, on each call to the component's `loop()` method, which is typically every 16ms.

The example below polls an analog sensor and yields a value dependent on whether the sensor value is above a threshold.

```yaml
# Example configuration entry
binary_sensor:
  - platform: template
    name: "Garage Door Open"
    lambda: |-
      if (id(ultrasonic_sensor1).state > 30) {
        // Garage Door is open.
        return true;
      } else {
        // Garage Door is closed.
        return false;
      }
```

Possible return values of the lambda:

- `return true;` if the binary sensor should be ON.
- `return false;` if the binary sensor should be OFF.
- `return {};` if the state is not known. The last known state will be maintained.

As an alternative to using a lambda you may use ESPHome [condition expressions](/automations/actions#all-conditions):

```yaml
# Example configuration entry
binary_sensor:
  - platform: template
    id: engine_running
    condition:
      sensor.in_range:
        id: engine_rpm
        above: 300.0
```

## Configuration variables

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
   C++ Lambda to be evaluated repeatedly to get the current state of the binary sensor.

- **condition** (*Optional*, [Condition](/automations/actions#all-conditions)): The condition to check to determine the value of the binary sensor. `lambda` and `condition` may not both be present in the configuration.
- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

{{< anchor "binary_sensor-template-publish_action" >}}

## `binary_sensor.template.publish` Action

You can also publish a state to a template binary sensor from elsewhere in your YAML file
with the `binary_sensor.template.publish` action.

```yaml
# Example configuration entry
binary_sensor:
  - platform: template
    name: "Garage Door Open"
    id: template_bin

# in some trigger
on_...:
  - binary_sensor.template.publish:
      id: template_bin
      state: ON

  # Templated
  - binary_sensor.template.publish:
      id: template_bin
      state: !lambda 'return id(some_sensor).state > 30;'
```

Configuration options:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the template binary sensor.
- **state** (**Required**, boolean, [templatable](/automations/templates)):
  The state to publish.

> [!NOTE]
> This action can also be written in lambdas:
>
> ```cpp
> id(template_bin).publish_state(true);
> ```

## See Also

- {{< docref "/components/binary_sensor" >}}
- {{< docref "/components/sensor/template" >}}
- [Automation](/automations)
- {{< apiref "template/binary_sensor/template_binary_sensor.h" "template/binary_sensor/template_binary_sensor.h" >}}
