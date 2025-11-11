---
description: "Instructions for setting up Color Temperature lights."
title: "Color Temperature Light"
params:
  seo:
    description: Instructions for setting up Color Temperature lights.
    image: brightness-medium.svg
---

The `color_temperature` light platform creates a Color Temperature
light from 2 [float output components](/components/output#output). One channel controls the LED temperature,
and the other channel controls the brightness.

```yaml
# Example configuration entry
light:
  - platform: color_temperature
    name: "Livingroom Lights"
    color_temperature: output_component1
    brightness: output_component2
    cold_white_color_temperature: 6536 K
    warm_white_color_temperature: 2000 K
```

## Configuration variables

- **color_temperature** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the color temperature. It returns a float from 0 to 1 in the mired scale. Hereby 0 corresponds to the cold white temperature and 1 to the warm white temperature.
- **brightness** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the brightness. It returns a float from 0 to 1.
- **cold_white_color_temperature** (**Required**, float): The coldest color temperature supported by this light. This
  is the lowest value when expressed in [mireds](https://en.wikipedia.org/wiki/Mired), or the highest value when
  expressed in Kelvin.

- **warm_white_color_temperature** (**Required**, float): The warmest color temperature supported by this light. This
  is the highest value when expressed in [mireds](https://en.wikipedia.org/wiki/Mired), or the lowest value when
  expressed in Kelvin.

- All other options from [Light](/components/light#config-light).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/light" >}}
- {{< docref "/components/light/cwww" >}}
- {{< docref "/components/light/rgb" >}}
- {{< docref "/components/light/rgbw" >}}
- {{< docref "/components/light/rgbww" >}}
- {{< docref "/components/light/rgbct" >}}
- {{< docref "/components/power_supply" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/pca9685" >}}
- {{< docref "/components/output/tlc59208f" >}}
- {{< apiref "color_temperature/ct_light_output.h" "color_temperature/ct_light_output.h" >}}
