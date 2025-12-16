---
description: "Sigma-Delta Output"
title: "Sigma-Delta Output"
---

This component uses [sigma-delta modulation](https://en.wikipedia.org/wiki/Delta-sigma_modulation)
to output a floating-point value on a binary output. Unlike with {{< docref "/components/output/slow_pwm" >}},
it is possible to update the output value with each update cycle, not just at the end of a longer period.

{{< img src="sigma-delta-example.png" alt="Image" caption="Comparison between a *Slow PWM* with a period of 100s and a *sigma-delta output* with an update interval of 1s" width="65.0%" class="align-center" >}}

For example, if you choose to toggle the output at most once every 1 second and decide on a
PWM period of 10 seconds, for reasonably frequent updates, with {{< docref "/components/output/slow_pwm" >}}
there are only 10 possible levels, and for higher precision a longer update interval is needed,
restricting the update rate.

A *sigma-delta* output is updated during each cycle, thus a higher precision can be achieved, without
being constrained by a calculation timeframe (=period).

So instead of having to define a `period` where the width of the pulse determines the output level, here you
choose an `update_interval` which acts like a clock signal from where the pulse density determines the output level.

This component can be used as a drop-in replacement for {{< docref "/components/output/slow_pwm" >}} by changing the `platform` to
`sigma_delta_output` and changing `period` to `update_interval` (you usually want to set the *sigma-delta*'s
`update_interval` as a fraction of *Slow PWM*'s `period` for similar results)

```yaml
# Example configuration entry
output:
  - platform: sigma_delta_output
    update_interval: 10s
    id: sd_heater_output

    # Output to a pin
    pin: GPIOXX

    # Use the same output, but through automations
    turn_on_action:
      then:
        - output.turn_on: heater_relay
    turn_off_action:
      then:
        - output.turn_off: heater_relay

  - platform: gpio
    pin: GPIOXX
    id: heater_relay
```

Configuration variables:

- **update_interval** (**Required**, [Time](/guides/configuration-types#time)): The cycle interval at which the output is recalculated.
- **pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to pulse.
- **state_change_action** (*Optional*, [Automation](/automations)): An automation to perform when the load is switched. If a lambda is used the boolean `state` parameter holds the new status.
- **turn_on_action** (*Optional*, [Automation](/automations)): An automation to perform when the load is turned on. Can be used to control for example a switch or output component.
- **turn_off_action** (*Optional*, [Automation](/automations)): An automation to perform when the load is turned off. `turn_on_action` and `turn_off_action` must be configured together.
- All options from [Output](/components/output#config-output).

> [!NOTE]
>
> - If `pin` is defined, the GPIO pin state is written before any action is executed.
> - `state_change_action` and `turn_on_action`  /`turn_off_action` can be used together. `state_change_action` is called before `turn_on_action`  /`turn_off_action`. It's recommended to use either `state_change_action` or `turn_on_action`  /`turn_off_action` to change the state of an output. Using both automations together is only recommended for monitoring.

> [!NOTE]
> If the output must not be active for more than some fixed time before it has
> to be off for a while to e.g. cool down, {{< docref "/components/output/slow_pwm" >}}
> should be used with a `max_power` setting to better control the duty
> cycle.

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/slow_pwm" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< docref "/components/fan/speed" >}}
- {{< docref "/components/power_supply" >}}
- [Sigma-Delta](https://en.wikipedia.org/wiki/Delta-sigma_modulation)
- {{< apiref "sigma_delta_output/sigma_delta_output.h" "sigma_delta_output/sigma_delta_output.h" >}}
