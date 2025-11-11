---
description: "Instructions for setting up RGBWW lights."
title: "RGBWW Light"
params:
  seo:
    description: Instructions for setting up RGBWW lights.
    image: rgbw.png
---

The `rgbww` light platform creates an RGBWW (cold white + warm white)
light from 5 [float output components](/components/output#output) (one for each channel). The cold and warm
white channels can be controlled individually or together, see [Mixing](/components/light/cwww#cwww_mixing) for more information.

```yaml
# Example configuration entry
light:
  - platform: rgbww
    name: "Livingroom Lights"
    red: output_component1
    green: output_component2
    blue: output_component3
    cold_white: output_component4
    warm_white: output_component5
    cold_white_color_temperature: 6536 K
    warm_white_color_temperature: 2000 K
```

## Color Correction

It is often favourable to calibrate/correct the color produced by an LED strip light as the
perceived intensity of different colors will generally vary. This can be done by using
[max_power](/components/output#config-output) on individual output channels:

```yaml
# Example configuration entry
light:
  - platform: rgbw
    name: "Livingroom Lights"
    red: output_component1
    green: output_component2
    blue: output_component3
    white: output_component4

# Example output entry
output:
  - platform: ...
    id: output_component1
    max_power: 80%
```

> [!NOTE]
> Remember that `gamma_correct` is enabled by default (`γ=2.8`  ), and you may want take it into account for the calibration. For instance if you command a light to *50%* brightness and want it to be the new maximum: `max_PWM_power = max_light_power^2.8 = 0.5^2.8 = 0.144`, then you would set `max_power` to *14.4%*.

## Color Interlock

With some LED bulbs, it is not possible to enable the RGB leds at the same time as the white leds, or setting
the RGB channels to maximum whilst wanting a white light will have an undesired hue effect. For these cases a
configuration variable is available that prevents the RGB leds and white leds from being turned on at the same
time: `color_interlock`.

Setting this option to `true` will result in the light having two color modes available, `RGB` and `COLD_WARM_WHITE`.
When the `RGB` color mode is active, the white leds are turned off, and when the `COLD_WARM_WHITE` color mode is active,
the RGB leds are turned off. Switching between these modes can be done from the Home Assistant interface, or by using
the `color_mode` option of the [light control actions](/components/light#light-turn_on_action).

## Configuration variables

- **red** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the red channel.
- **green** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the green channel.
- **blue** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the blue channel.
- **cold_white** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the cold
  white channel.

- **warm_white** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the warm
  white channel.

- **cold_white_color_temperature** (*Optional*, float): The color temperature (in
  [mireds](https://en.wikipedia.org/wiki/Mired) or Kelvin) of the cold white channel. Note that when the color interlock
  is enabled, this option is required to control the light from Home Assistant.

- **warm_white_color_temperature** (*Optional*, float): The color temperature (in
  [mireds](https://en.wikipedia.org/wiki/Mired) or Kelvin) of the warm white channel. Note that when the color interlock
  is enabled, this option is required to control the light from Home Assistant.

- **constant_brightness** (*Optional*, boolean): When enabled, this will keep the overall brightness of the
  cold and warm white channels constant by limiting the combined output to 100% of a single channel. This
  reduces the possible overall brightness but is necessary for some power supplies that are not able to run
  both channels at full brightness at once. Defaults to `false`.

- **color_interlock** (*Optional*, boolean): When enabled, this will prevent white leds being on at the same
  time as RGB leds. See [Color Interlock](/components/light/rgbw#rgbw_color_interlock) for more information. Defaults to `false`.

- All other options from [Light](/components/light#config-light).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/light" >}}
- {{< docref "/components/light/rgb" >}}
- {{< docref "/components/light/rgbw" >}}
- {{< docref "/components/power_supply" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/pca9685" >}}
- {{< docref "/components/output/tlc59208f" >}}
- {{< docref "/components/output/my9231" >}}
- {{< docref "/components/output/sm16716" >}}
- {{< apiref "rgbww/rgbww_light_output.h" "rgbww/rgbww_light_output.h" >}}
