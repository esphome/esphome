---
description: "Instructions for setting up RGBCT lights."
title: "RGBCT Light"
params:
  seo:
    description: Instructions for setting up RGBCT lights.
    image: rgbw.png
---

The `rgbct` light platform creates an RGBWT (color temperature + white brightness)
light from 5 [float output components](/components/output#output) (one for each channel).

```yaml
# Example configuration entry
light:
  - platform: rgbct
    name: "Livingroom Lights"
    red: output_component1
    green: output_component2
    blue: output_component3
    color_temperature: output_component4
    white_brightness: output_component5
    cold_white_color_temperature: 153 mireds
    warm_white_color_temperature: 500 mireds
```

## Configuration variables

- **red** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the red channel.
- **green** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the green channel.
- **blue** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the blue channel.
- **color_temperature** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the
  color temperature channel.

- **white_brightness** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the brightness
  of the white leds.

- **cold_white_color_temperature** (**Required**, float): The coldest color temperature supported by this light. This
  is the lowest value when expressed in [mireds](https://en.wikipedia.org/wiki/Mired), or the highest value when
  expressed in Kelvin.

- **warm_white_color_temperature** (**Required**, float): The warmest color temperature supported by this light. This
  is the highest value when expressed in [mireds](https://en.wikipedia.org/wiki/Mired), or the lowest value when
  expressed in Kelvin.

- **color_interlock** (*Optional*, boolean): When enabled, this will prevent white leds being on at the same
  time as RGB leds. See [Color Interlock](/components/light/rgbw#rgbw_color_interlock) for more information. Defaults to `false`.

- All other options from [Light](/components/light#config-light).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/light" >}}
- {{< docref "/components/light/rgb" >}}
- {{< docref "/components/light/rgbw" >}}
- {{< docref "/components/light/rgbww" >}}
- {{< docref "/components/power_supply" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/pca9685" >}}
- {{< docref "/components/output/tlc59208f" >}}
- {{< docref "/components/output/my9231" >}}
- {{< docref "/components/output/sm16716" >}}
- {{< apiref "rgbct/rgbct_light_output.h" "rgbct/rgbct_light_output.h" >}}
