---
description: "Instructions for setting up a Tuya dimmer switch."
title: "Tuya Dimmer"
params:
  seo:
    description: Instructions for setting up a Tuya dimmer switch.
    image: brightness-medium.svg
---

The `tuya` light platform creates a simple brightness-only light from a
tuya serial component.

> [!WARNING]
> Some of these dimmers have no way of serial flashing without destroying them.
> Make sure you have some way to OTA upload configured before flashing. This means you need
> to have working wifi, ota, and maybe api sections in the config.
> The dimmer switch I got would hang if the logger was configured to use the serial port
> which meant it was bricked until I cut it open.

This requires the {{< docref "/components/tuya" >}} component to be set up before you can use this platform.

Here is an example output for a Tuya dimmer:

```text
[21:50:28][C][tuya:024]: Tuya:
[21:50:28][C][tuya:031]:   Datapoint 3: int value (value: 139)
[21:50:28][C][tuya:029]:   Datapoint 1: switch (value: OFF)
```

On this dimmer, the toggle switch is datapoint 1 and the dimmer value is datapoint 3.
Now you can create the light.

```yaml
# Create a light using the dimmer
light:
  - platform: "tuya"
    name: "dim1"
    dimmer_datapoint: 3
    min_value_datapoint: 2
    switch_datapoint: 1
```

## Configuration variables

- **dimmer_datapoint** (*Optional*, int): The datapoint id number of the dimmer value.
- **min_value_datapoint** (*Optional*, int): The datapoint id number of the MCU minimum value
  setting. If this is set then ESPHome will sync the **min_value** to the MCU on startup.

- **switch_datapoint** (*Optional*, int): The datapoint id number of the power switch. My dimmer
  required this to be able to turn the light on and off. Without this you would only be able to
  change the brightness and would have to toggle the light using the physical buttons.

- **color_temperature_datapoint** (*Optional*, int): The datapoint id number of the color
  temperature value.

- **color_datapoint** (*Optional*, int): The datapoint id number of the color value.
  If this is set, along with **color_type**, then ESPHome will set the color value formatted
  based on the **color_type**.

- **color_type** (*Optional*, enum): The color type to use when setting the **color_datapoint**.
  If this is set, along with **color_datapoint**, then ESPHome will use this value to format
  the color sent to **color_datapoint**.

  - `rgb`  : Use a 6 digit hex RGB value
  - `hsv`  : Use a 12 digit hex HSV value
  - `rgbhsv`  : Use a 14 digit hex RGBHSV value

- **min_value** (*Optional*, int): The lowest dimmer value allowed. My dimmer had a
  minimum of 25 and wouldn't even accept anything lower, but this option is available if necessary.
  Defaults to 0.

- **max_value** (*Optional*, int): The highest dimmer value allowed. Most dimmers have a
  maximum of 255, but dimmers with a maximum of 1000 can also be found. Try what works best.
  Defaults to 255.

- **color_temperature_max_value** (*Optional*, int): The highest color temperature
  value allowed. Some ceiling fans have a value of 100 (also for `max_value`  ). Defaults to 255.

- **color_temperature_invert** (*Optional*, boolean): Control how color temperature values are
  sent to the MCU. If this is set to true ESPHome will treat 0 as warm white and
  **color_temperature_max_value** as cool white when setting **color_temperature_datapoint**.
  Defaults to false.

- **cold_white_color_temperature** (*Optional*, float): The color temperature (in [mireds](https://en.wikipedia.org/wiki/Mired) or Kelvin) of the cold white channel.
- **warm_white_color_temperature** (*Optional*, float): The color temperature (in [mireds](https://en.wikipedia.org/wiki/Mired) or Kelvin) of the warm white channel.
- All other options from [Light](/components/light#config-light).
- At least one of *dimmer_datapoint*, *switch_datapoint*, *rgb_datapoint*, or *hsv_datapoint* must be provided.
- Only one of *rgb_datapoint* or *hsv_datapoint* can be provided for one light.

> [!NOTE]
> The MCU on the Tuya dimmer handles transitions and gamma correction on its own.
> Therefore the `gamma_correct` setting default is `1.0` and the
> `default_transition_length` parameter is `0s` by default.

## See Also

- {{< docref "/components/tuya" >}}
- {{< docref "/components/light" >}}
- {{< apiref "tuya/light/tuya_light.h" "tuya/light/tuya_light.h" >}}
