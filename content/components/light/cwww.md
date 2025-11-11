---
description: "Instructions for setting up Cold White + Warm White lights."
title: "Cold White + Warm White Light"
params:
  seo:
    description: Instructions for setting up Cold White + Warm White lights.
    image: brightness-medium.svg
---

The `cwww` light platform creates a cold white + warm white light from 2
[float output components](/components/output#output) (one for each channel). The two channels
can be controlled individually or together.

```yaml
# Example configuration entry
light:
  - platform: cwww
    name: "Livingroom Lights"
    cold_white: output_component1
    warm_white: output_component2
    cold_white_color_temperature: 6536 K
    warm_white_color_temperature: 2000 K
    constant_brightness: true
```

{{< anchor "cwww_mixing" >}}

## Mixing

The two channels of this light can be controlled individually by using the `cold_white` and `warm_white` options of
the [light control actions](/components/light#light-turn_on_action).

If the color temperature of both lights is supplied, it is also possible to control the two channels together by
setting a color temperature, using the `white` (interpreted as brightness) and `color_temperature` options. This
calculation assumes that both lights have the same illuminance, which might not always be accurate.

## Configuration variables

- **cold_white** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the cold white channel.
- **warm_white** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the warm white channel.
- **cold_white_color_temperature** (*Optional*, float): The color temperature (in [mireds](https://en.wikipedia.org/wiki/Mired) or Kelvin)
  of the cold white channel. Note that this option is required to control the mixing from Home Assistant.

- **warm_white_color_temperature** (*Optional*, float): The color temperature (in [mireds](https://en.wikipedia.org/wiki/Mired) or Kelvin)
  of the warm white channel. Note that this option is required to control the mixing from Home Assistant.

- **constant_brightness** (*Optional*, boolean): When enabled, this will keep the overall brightness of the cold and warm white channels constant by limiting the combined output to 100% of a single channel. This reduces the possible overall brightness but is necessary for some power supplies that are not able to run both channels at full brightness at once. Defaults to `false`.
- All other options from [Light](/components/light#config-light).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/light" >}}
- {{< docref "/components/light/rgb" >}}
- {{< docref "/components/light/rgbw" >}}
- {{< docref "/components/light/rgbww" >}}
- {{< docref "/components/light/rgbct" >}}
- {{< docref "/components/light/color_temperature" >}}
- {{< docref "/components/power_supply" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/pca9685" >}}
- {{< docref "/components/output/tlc59208f" >}}
- {{< apiref "cwww/cwww_light_output.h" "cwww/cwww_light_output.h" >}}
