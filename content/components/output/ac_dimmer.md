---
description: "Instructions for setting up AC Dimmer component in ESPHome."
title: "AC Dimmer Component"
params:
  seo:
    description: Instructions for setting up AC Dimmer component in ESPHome.
    image: ac_dimmer.svg
---

The `ac_dimmer` component allows you to connect a dimmable light or other load
which supports phase control dimming to your ESPHome project.

There are several already made boards which are compatible with this component, such as the
[RobotDyn dimmer](https://robotdyn.com/ac-light-dimmer-module-1-channel-3-3v-5v-logic-ac-50-60hz-220v-110v.html).

{{< img src="robotdyn_dimmer.jpg" alt="Image" caption="RobotDyn Module. Image by RobotDyn" width="50.0%" class="align-center" >}}

```yaml
# Example configuration entry
output:
  - platform: ac_dimmer
    id: dimmer1
    gate_pin: GPIOXX
    zero_cross_pin:
      number: GPIOXX
      mode:
        input: true
      inverted: yes

light:
  - platform: monochromatic
    output: dimmer1
    name: Dimmable Light
```

## Configuration variables

- **gate_pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin used to control the Triac or
  Mosfet.

- **zero_cross_pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin used to sense the AC
  Zero cross event, you can have several dimmers controlled with the same zero cross
  detector, in such case duplicate the `zero_cross_pin` config on each output. When
  doing so, `allow_other_uses` pin schema option **must** be set to `true` to
  avoid configuration errors due to pin reuse.

- **method** (*Optional*): Set the method for dimming, can be:

  - `leading pulse`  : (default) a short pulse to trigger a triac.
  - `leading`  : gate pin driven high until the zero cross is detected
  - `trailing`  : gate pin driven high from zero cross until dim period, this method
    is suitable for mosfet dimmers only.

- **init_with_half_cycle** (*Optional*, boolean): Will send the first full half AC cycle
  Try to use this for dimmable LED lights, it might help turning on at low brightness
  levels. On Halogen lamps it might show at initial flicker. Defaults to `false`.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- All other options from [Output](/components/output#config-output).

Dimming lights with phase control can be tricky, the minimum level your light turns on
might be different from other lights, also the perceived light level might not correlate
to the percentage output set to the light, to try to minimize these behaviors you can
tweak the values `min_power` from this output component and also `gamma_correct` from
the monochromatic light.

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/light/monochromatic" >}}
- {{< apiref "ac_dimmer/ac_dimmer.h" "ac_dimmer/ac_dimmer.h" >}}
