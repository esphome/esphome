---
description: "Instructions for setting up power supplies which will automatically turn on together with outputs."
title: "Power Supply Component"
params:
  seo:
    description: Instructions for setting up power supplies which will automatically turn on together with outputs.
    image: power.svg
---

The `power_supply` component allows you to have a high power mode for
certain outputs. For example, if you're using an [ATX powersupply](https://en.wikipedia.org/wiki/ATX) to power your LED strips,
you usually don't want to have the power supply on all the time while
the output is not on. The power supply component can be attached to any
[Output Component](/components/output#output) and
will automatically switch on if any of the outputs are on. Furthermore,
it also has a cooldown time that keeps the power supply on for a while
after the last output has been disabled.

```yaml
# Example configuration entry
power_supply:
  - id: 'power_supply1'
    pin: GPIOXX
```

## Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The id of the
  power supply so that it can be used by the outputs.

- **pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The
  GPIO pin to control the power supply on.

- **enable_time** (*Optional*, [Time](/guides/configuration-types#time)): The time
  that the power supply needs for startup. The output component will
  wait for this period of time after turning on the PSU and before
  switching the output on. Defaults to `20ms`. Maximum of less than `5s`.

- **keep_on_time** (*Optional*, [Time](/guides/configuration-types#time)): The time the
  power supply should be kept enabled after the last output that used
  it has been switch off. Defaults to `10s`.

- **enable_on_boot** (*Optional*, bool): If the power supply should be enabled when the power supply
  component is setup. Defaults to false. The startup delay will be applied (other component setup will be blocked
  until the delay has elapsed.) This is useful for power supplies that will never be turned off and avoids the need
  to specifically configure the power supply in a different component.

See the [output component base configuration](/components/output#config-output)
for information on how to apply the power supply for a specific output.

## ATX Power Supplies

{{< img src="power_supply-atx.jpg" alt="Image" width="80.0%" class="align-center" >}}

The power supply component will default to pulling the specified GPIO
pin up when high power mode is needed. Most ATX power supplies however
operate with an active-low configuration. Therefore their output needs
to be inverted.

```yaml
power_supply:
  - id: 'atx_power_supply'
    pin:
      number: 13
      inverted: true
```

Then simply connect the green control wire from the ATX power supply to
your specified pin. It's recommended to put a small resistor (about 1kΩ)
in between to protect the ESP board.

## See Also

- {{< docref "output/" >}}
- {{< apiref "power_supply/power_supply.h" "power_supply/power_supply.h" >}}
