---
description: "Instructions for setting up RGB lights in ESPHome."
title: "RGB Light"
params:
  seo:
    description: Instructions for setting up RGB lights in ESPHome.
    image: rgb.png
---

The `rgb` light platform creates an RGB light from 3 [float output components](/components/output#output)
(one for each color channel).

{{< img src="rgb-strip.jpg" alt="Image" caption="Example of an RGB LED strip that can be used with this component." width="75.0%" class="align-center" >}}

{{< img src="rgb-ui.png" alt="Image" width="40.0%" class="align-center" >}}

```yaml
# Example configuration entry
light:
  - platform: rgb
    name: "Living Room Lights"
    red: output_component1
    green: output_component2
    blue: output_component3
```

## Color Correction

It is often favourable to calibrate/correct the color produced by an LED strip light as the
perceived intensity of different colors will generally vary. This can be done by using
[max_power](/components/output#config-output) on individual output channels:

```yaml
# Example configuration entry
light:
  - platform: rgb
    name: "Living Room Lights"
    red: output_component1
    green: output_component2
    blue: output_component3

# Example output entry
output:
  - platform: ...
    id: output_component1
    max_power: 80%
  # Repeat for green and blue output
```

> [!NOTE]
> Remember that `gamma_correct` is enabled by default (`γ=2.8`  ), and you may want take it into account for the calibration. For instance if you command a light to *50%* brightness and want it to be the new maximum: `max_PWM_power = max_light_power^2.8 = 0.5^2.8 = 0.144`, then you would set `max_power` to *14.4%*.

## Configuration variables

- **red** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the red channel.
- **green** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the green channel.
- **blue** (**Required**, [ID](/guides/configuration-types#id)): The id of the float [Output Component](/components/output#output) to use for the blue channel.
- All other options from [Light](/components/light#config-light).

## See Also

{{< img src="rgb-detail.jpg" alt="Image" width="75.0%" class="align-center" >}}

- {{< docref "/components/output" >}}
- {{< docref "/components/light" >}}
- {{< docref "/components/light/rgbw" >}}
- {{< docref "/components/light/rgbww" >}}
- {{< docref "/components/light/rgbct" >}}
- {{< docref "/components/power_supply" >}}
- {{< docref "/components/output/ledc" >}}
- {{< docref "/components/output/esp8266_pwm" >}}
- {{< docref "/components/output/pca9685" >}}
- {{< docref "/components/output/tlc59208f" >}}
- {{< docref "/components/output/my9231" >}}
- {{< docref "/components/output/sm16716" >}}
- {{< apiref "rgb/rgb_light_output.h" "rgb/rgb_light_output.h" >}}
