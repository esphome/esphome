---
description: "Instructions for setting up an LVGL widget select."
title: "LVGL Select"
params:
  seo:
    description: Instructions for setting up an LVGL widget select.
    image: ../images/lvgl_c_sel.png
---

The `lvgl` select platform creates a select from an LVGL widget
and requires {{< docref "/components/lvgl/index" "LVGL" >}} to be configured.

Supported widgets are [`dropdown`](/components/lvgl/widgets#lvgl-widget-dropdown) and [`roller`](/components/lvgl/widgets#lvgl-widget-roller). A single select supports only a single widget; in other words, it's not possible to have multiple widgets associated with a single ESPHome select component.

## Configuration variables

- **widget** (**Required**): The ID of a supported widget configured in LVGL, which will reflect the state of the select.
- **restore_value**: (*Optional*, bool) Restore the value of the select from non-volatile memory when the device is restarted. Defaults to `false`.
- All other variables from [Select](/components/select#config-select).

Example:

```yaml
select:
  - platform: lvgl
    widget: dropdown_id
    name: LVGL Dropdown
```

> [!NOTE]
> Widget-specific actions (`lvgl.dropdown.update`, `lvgl.roller.update`  ) will trigger correspponding component updates to be sent to Home Assistant.

## See Also

- {{< docref "/components/lvgl/index" "LVGL Main component" >}}
- [Roller widget](/components/lvgl/widgets#lvgl-widget-roller)
- [Dropdown widget](/components/lvgl/widgets#lvgl-widget-dropdown)
- {{< docref "/components/binary_sensor/lvgl" >}}
- {{< docref "/components/sensor/lvgl" >}}
- {{< docref "/components/number/lvgl" >}}
- {{< docref "/components/switch/lvgl" >}}
- {{< docref "/components/light/lvgl" >}}
- {{< docref "/components/text/lvgl" >}}
- {{< docref "/components/text_sensor/lvgl" >}}
