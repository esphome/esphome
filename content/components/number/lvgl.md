---
description: "Instructions for setting up an LVGL widget number component."
title: "LVGL Number"
params:
  seo:
    description: Instructions for setting up an LVGL widget number component.
    image: ../images/lvgl_c_num.png
---

The `lvgl` number platform creates a number component from an LVGL widget
and requires {{< docref "/components/lvgl/index" "LVGL" >}} to be configured.

Supported widgets are [`arc`](/components/lvgl/widgets#lvgl-widget-arc), [`bar`](/components/lvgl/widgets#lvgl-widget-bar), [`slider`](/components/lvgl/widgets#lvgl-widget-slider) and [`spinbox`](/components/lvgl/widgets#lvgl-widget-spinbox). A single number supports only a single widget; in other words, it's not possible to have multiple widgets associated with a single ESPHome number component.

## Configuration variables

- **widget** (**Required**): The ID of a supported widget configured in LVGL, which will reflect the state of the number.
- **animated** (*Optional*, boolean): Whether to set the value of the widget with an animation (if supported by the widget). Defaults to `true`.
- **update_on_release** (*Optional*, boolean): By default the number will publish a new value each time the value of the associated widget changes. If this option is `true` then the value will only be published when touch is released.
- **restore_value**: (*Optional*, bool) Restore the value of the number from non-volatile memory when the device is restarted. Defaults to `false`.
- All other variables from [Number](/components/number#config-number).

Example:

```yaml
number:
  - platform: lvgl
    widget: slider_id
    name: LVGL Slider
```

> [!NOTE]
> Widget-specific actions (`lvgl.arc.update`, `lvgl.bar.update`, `lvgl.slider.update`, `lvgl.spinbox.update`, `lvgl.spinbox.decrement`, `lvgl.spinbox.increment`  ) will trigger correspponding component updates to be sent to Home Assistant.

## See Also

- {{< docref "/components/lvgl/index" "LVGL Main component" >}}
- [Arc widget](/components/lvgl/widgets#lvgl-widget-arc)
- [Bar widget](/components/lvgl/widgets#lvgl-widget-bar)
- [Slider widget](/components/lvgl/widgets#lvgl-widget-slider)
- [Spinbox widget](/components/lvgl/widgets#lvgl-widget-spinbox)
- {{< docref "/components/binary_sensor/lvgl" >}}
- {{< docref "/components/sensor/lvgl" >}}
- {{< docref "/components/switch/lvgl" >}}
- {{< docref "/components/select/lvgl" >}}
- {{< docref "/components/light/lvgl" >}}
- {{< docref "/components/text/lvgl" >}}
- {{< docref "/components/text_sensor/lvgl" >}}
