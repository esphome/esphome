---
description: "Instructions for setting up generic outputs in ESPHome"
title: "Output Component"
params:
  seo:
    description: Instructions for setting up generic outputs in ESPHome
    image: folder-open.svg
---

{{< anchor "output" >}}

Each platform of the `output` domain exposes some output to
ESPHome. These are grouped into two categories: `binary` outputs
(that can only be ON/OFF) and `float` outputs (like PWM, can output
any rational value between 0 and 1).

{{< anchor "config-output" >}}

## Base Output Configuration

Each output platform extends this configuration schema.

```yaml
# Example configuration entry
output:
  - platform: ...
    id: my_output_id
    power_supply: power_supply_id
    inverted: false
    min_power: 0.01
    max_power: 0.75
```

Configuration variables:

- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this output component.
- **power_supply** (*Optional*, [ID](/guides/configuration-types#id)): The {{< docref "/components/power_supply" "power supply" >}} to connect to
  this output. When the output is enabled, the power supply will
  automatically be switched on too.

- **inverted** (*Optional*, boolean): If the output should be treated
  as inverted. Defaults to `false`.

Float outputs only:

- **min_power** (*Optional*, float): Sets the minimum output value of this output platform.
  Must be in range from 0 to max_power. Defaults to `0`. If zero_means_zero is `false` this will be output value when the entity is turned off.

- **max_power** (*Optional*, float): Sets the maximum output value of this output platform.
  Must be in range from min_power to 1. Defaults to `1`.

- **zero_means_zero** (*Optional*, boolean): Sets the output to use actual 0 instead of `min_power`.
  Defaults to `false`.

> [!NOTE]
> The `min_power` and `max_power` values are automatically clamped to ensure `0.0 ≤ min_power ≤ max_power ≤ 1.0`.
> This prevents invalid configurations and ensures stable output behavior.

{{< anchor "output-turn_on_action" >}}

### `output.turn_on` Action

This action turns the output with the given ID on when executed.

```yaml
on_...:
  then:
    - output.turn_on: light_1
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> id(light_1).turn_on();
> ```

{{< anchor "output-turn_off_action" >}}

### `output.turn_off` Action

This action turns the output with the given ID off when executed.

```yaml
on_...:
  then:
    - output.turn_off: light_1
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> id(light_1).turn_off();
> ```

{{< anchor "output-set_level_action" >}}

### `output.set_level` Action

This action sets the float output to the given level when executed.

> [!NOTE]
> This only works with floating point outputs like {{< docref "/components/output/ac_dimmer" >}},
> {{< docref "/components/output/esp8266_pwm" >}}, {{< docref "/components/output/ledc" >}},
> {{< docref "/components/output/sigma_delta_output" >}}, {{< docref "/components/output/slow_pwm" >}}.

```yaml
on_...:
  then:
    - output.set_level:
        id: light_1
        level: 50%
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> // range is 0.0 (off) to 1.0 (on)
> id(light_1).set_level(0.5);
> ```

{{< anchor "output-set_min_power_action" >}}

### `output.set_min_power` Action

This action sets the minimum output power level for the specified float output platform.
It allows you to dynamically adjust the `min_power` configuration variable at runtime.

> [!NOTE]
> This only works with floating point outputs like {{< docref "/components/output/ac_dimmer" >}},
> {{< docref "/components/output/esp8266_pwm" >}}, {{< docref "/components/output/ledc" >}},
> {{< docref "/components/output/sigma_delta_output" >}}, {{< docref "/components/output/slow_pwm" >}}.

```yaml
on_...:
  then:
    - output.set_min_power:
        id: light_1
        min_power: 20%
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> // range is 0.0 (off) to 1.0 (on)
> id(light_1).set_min_power(0.2);
> ```

{{< anchor "output-set_max_power_action" >}}

### `output.set_max_power` Action

This action sets the maximum output power level for the specified float output platform.
It allows you to dynamically adjust the `max_power` configuration variable at runtime.

> [!NOTE]
> This only works with floating point outputs like {{< docref "/components/output/ac_dimmer" >}},
> {{< docref "/components/output/esp8266_pwm" >}}, {{< docref "/components/output/ledc" >}},
> {{< docref "/components/output/sigma_delta_output" >}}, {{< docref "/components/output/slow_pwm" >}}.

```yaml
on_...:
  then:
    - output.set_max_power:
        id: light_1
        max_power: 80%
```

> [!NOTE]
> This action can also be expressed in [lambdas](/automations/templates#config-lambda):
>
> ```cpp
> // range is 0.0 (off) to 1.0 (on)
> id(light_1).set_max_power(0.8);
> ```

## Full Output Index

- {{< docref "/components/switch/output" >}}
- {{< docref "/components/power_supply" >}}
- {{< docref "/components/light/binary" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< docref "/components/light/rgb" >}}
- {{< docref "/components/fan/binary" >}}
- {{< docref "/components/fan/speed" >}}
- {{< apiref "binary_output.h" "output/binary_output.h" >}},
  {{< apiref "float_output.h" "output/float_output.h" >}}
