---
description: "Instructions for setting up an LVGL widget sensor component."
title: "LVGL Sensor"
params:
  seo:
    description: Instructions for setting up an LVGL widget sensor component.
    image: ../images/lvgl_c_num.png
---

The `lvgl` sensor platform creates a sensor component from an LVGL widget
and requires {{< docref "/components/lvgl/index" "LVGL" >}} to be configured.

Supported widgets are [`arc`](#lvgl-widget-arc), [`bar`](#lvgl-widget-bar), [`slider`](#lvgl-widget-slider) and [`spinbox`](#lvgl-widget-spinbox). A single sensor supports only a single widget; in other words, it's not possible to have multiple widgets associated with a single ESPHome sensor.

## Configuration variables

- **widget** (**Required**): The ID of a supported widget configured in LVGL, which will reflect the state of the sensor.
- All other variables from [Sensor](/components/sensor).

Example:

```yaml
sensor:
  - platform: lvgl
    widget: slider_id
    name: LVGL Slider
```

> [!NOTE]
> Widget-specific actions (`lvgl.arc.update`, `lvgl.bar.update`, `lvgl.slider.update`, `lvgl.spinbox.update`, `lvgl.spinbox.decrement`, `lvgl.spinbox.increment`  ) will trigger corresponding component updates to be sent to Home Assistant.

## See Also

- {{< docref "/components/lvgl/index" "LVGL Main component" >}}
- [Arc widget](#lvgl-widget-arc)
- [Bar widget](#lvgl-widget-bar)
- [Slider widget](#lvgl-widget-slider)
- [Spinbox widget](#lvgl-widget-spinbox)
- {{< docref "/components/binary_sensor/lvgl" >}}
- {{< docref "/components/switch/lvgl" >}}
- {{< docref "/components/select/lvgl" >}}
- {{< docref "/components/light/lvgl" >}}
- {{< docref "/components/number/lvgl" >}}
- {{< docref "/components/text/lvgl" >}}
- {{< docref "/components/text_sensor/lvgl" >}}
