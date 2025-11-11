---
description: "Instructions for setting up an LVGL widget binary sensor."
title: "LVGL Binary Sensor"
params:
  seo:
    description: Instructions for setting up an LVGL widget binary sensor.
    image: ../images/lvgl_c_bns.png
---

The `lvgl` binary sensor platform creates a binary sensor from an LVGL widget
and requires {{< docref "/components/lvgl/index" "LVGL" >}} to be configured.

Supported widget is [`button`](/components/lvgl/widgets#lvgl-widget-button). A single binary sensor supports only a single widget; in other words, it's not possible to have multiple widgets associated with a single ESPHome binary sensor component.

## Configuration variables

- **widget** (**Required**): The ID of a supported widget configured in LVGL, which will reflect the state of the binary sensor.
- All other variables from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

Example:

```yaml
binary_sensor:
  - platform: lvgl
    widget: btn_id
    name: LVGL push button
```

## See Also

- {{< docref "/components/lvgl/index" "LVGL Main component" >}}
- [Button widget](/components/lvgl/widgets#lvgl-widget-button)
- {{< docref "/components/sensor/lvgl" >}}
- {{< docref "/components/number/lvgl" >}}
- {{< docref "/components/switch/lvgl" >}}
- {{< docref "/components/select/lvgl" >}}
- {{< docref "/components/light/lvgl" >}}
- {{< docref "/components/text/lvgl" >}}
- {{< docref "/components/text_sensor/lvgl" >}}
