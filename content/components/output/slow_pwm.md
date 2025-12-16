---
description: "Instructions for setting up slow pwm outputs for GPIO pins."
title: "Slow PWM Output"
params:
  seo:
    description: Instructions for setting up slow pwm outputs for GPIO pins.
    image: pwm.png
---

Similar to PWM, the Slow PWM Output platform allows you to control GPIO pins by
pulsing them on/off over a longer time period. It could be used to control a
heating element through a relay where a fast PWM update cycle would not be appropriate.

> [!NOTE]
> This is for **slow** PWM output. For fast-switching PWM outputs (for example,
> lights), see these outputs:
>
> - ESP32: {{< docref "ledc/" >}}
> - ESP8266: {{< docref "esp8266_pwm/" >}}

```yaml
# Example configuration entry
output:
  - platform: slow_pwm
    pin: GPIOXX
    id: my_slow_pwm
    period: 15s
```

## Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this output component.
- **period** (**Required**, [Time](/guides/configuration-types#time)): The duration of each cycle. (i.e. a 10s
  period at 50% duty would result in the pin being turned on for 5s, then off for 5s)

- **pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to pulse.
- **state_change_action** (*Optional*, [Automation](/automations)): An automation to perform when the load is switched. If a lambda is used the boolean `state` parameter holds the new status.
- **turn_on_action** (*Optional*, [Automation](/automations)): An automation to perform when the load is turned on. Can be used to control for example a switch or output component.
- **turn_off_action** (*Optional*, [Automation](/automations)): An automation to perform when the load is turned off. `turn_on_action` and `turn_off_action` must be configured together.
- **restart_cycle_on_state_change** (*Optional*, boolean): Restart a timer of a cycle
  when new state is set. Defaults to `false`.

- All other options from [Output](/components/output#config-output).

> [!NOTE]
>
> - If `pin` is defined the GPIO pin state is written before any action is executed.
> - `state_change_action` and `turn_on_action`  /`turn_off_action` can be used together. `state_change_action` is called before `turn_on_action`  /`turn_off_action`. It's recommended to use either `state_change_action` or `turn_on_action`  /`turn_off_action` to change the state of an output. Using both automations together is only recommended for monitoring.

## Example

```yaml
output:
  - platform: slow_pwm
    id: my_slow_pwm
    period: 15s
    turn_on_action:
      - lambda: |-
          auto *out1 = id(output1);
          out1->turn_on();
    turn_off_action:
      - output.turn_off: output1
```

> [!NOTE]
> If the duty cycle is not constrained to a maximum value, the
> {{< docref "/components/output/sigma_delta_output" >}} component offers faster updates and
> greater control over the switching frequency. This is better for loads that
> need some time to fully change between on and off, like eletric thermal
> actuator heads or fans.

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/sigma_delta_output" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< docref "/components/fan/speed" >}}
- {{< docref "/components/power_supply" >}}
- {{< apiref "slow_pwm/slow_pwm_output.h" "slow_pwm/slow_pwm_output.h" >}}
