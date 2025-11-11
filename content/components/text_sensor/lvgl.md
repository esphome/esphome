---
description: "Instructions for setting up an LVGL Text Sensor."
title: "LVGL Text Sensor"
params:
  seo:
    description: Instructions for setting up an LVGL Text Sensor.
    image: ../images/lvgl_c_txt.png
---

The `lvgl` text sensor platform creates a Text Sensor from an LVGL textual widget
and requires {{< docref "/components/lvgl/index" "LVGL" >}} to be configured.

Supported widgets are [`label`](/components/lvgl/widgets#lvgl-widget-label) and [`textarea`](/components/lvgl/widgets#lvgl-widget-textarea). A single text sensor supports only a single widget; in other words, it's not possible to have multiple widgets associated with a single ESPHome text sensor component.

## Configuration variables

- **widget** (**Required**): The ID of a `textarea` widget configured in LVGL, which will reflect the state of the text sensor.
- All other variables from [Text Sensor](/components/text_sensor#config-text_sensor).

Example:

```yaml
text_sensor:
  - platform: lvgl
    widget: textarea_id
    name: "Textarea 1 text"
```

> [!NOTE]
> Widget-specific actions (`lvgl.label.update`, `lvgl.textarea.update`  ) will trigger correspponding component updates to be sent to Home Assistant.

## See Also

- {{< docref "/components/lvgl/index" "LVGL Main component" >}}
- [Label widget](/components/lvgl/widgets#lvgl-widget-label)
- [Textarea widget](/components/lvgl/widgets#lvgl-widget-textarea)
- {{< docref "/components/binary_sensor/lvgl" >}}
- {{< docref "/components/sensor/lvgl" >}}
- {{< docref "/components/number/lvgl" >}}
- {{< docref "/components/switch/lvgl" >}}
- {{< docref "/components/light/lvgl" >}}
- {{< docref "/components/select/lvgl" >}}
- {{< docref "/components/text/lvgl" >}}
