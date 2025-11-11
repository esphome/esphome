---
description: "Instructions for setting up a hbridge light."
title: "H-bridge Light"
params:
  seo:
    description: Instructions for setting up a hbridge light.
    image: brightness-medium.svg
---

The `hbridge` light platform creates a dual color brightness controlled light from two
[float output component](/components/output#output).

{{< img src="hbridge-ui.png" alt="Image" width="40.0%" class="align-center" >}}

H-bridge lights are very common for Christmas lighting and they use 2 wires for a bunch of LEDs.
The pins are switched alternatively to allow two sets of lights to operate.

```yaml
# Example configuration entry
light:
  - platform: hbridge
    id: mainlight
    name: "Hbridge Lights"
    pin_a: pina
    pin_b: pinb
```

Internally, H-bridge lights are implemented as cold/warm white lights. This means that the brightness of the two colors
is mapped to the cold white and warm white values, even if the colors aren't actually white in reality. To individually
control the colors in the [light control actions](/components/light#light-turn_on_action), you need to use the `cold_white` and
`warm_white` options.

## Configuration variables

- **pin_a** (**Required**, [ID](/guides/configuration-types#id)): The id of the first float [Output Component](/components/output#output) to use for this light.
- **pin_b** (**Required**, [ID](/guides/configuration-types#id)): The id of the second float [Output Component](/components/output#output) to use for this light.
- All other options from [Light](/components/light#config-light).

> [!NOTE]
> As we are switching the H-bridge in software, the light may glitch every so often when other tasks run on the MCU.

## See Also

- {{< docref "/components/light" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< apiref "hbridge/light/hbridge_light.h" "hbridge/light/hbridge_light.h" >}}
