---
description: "Instructions for setting up template buttons that can execute arbitrary actions when pressed."
title: "Template Button"
params:
  seo:
    description: Instructions for setting up template buttons that can execute arbitrary actions when pressed.
    image: description.svg
---

The `template` button platform allows you to create simple buttons out of just actions. Once defined,
it will automatically appear in Home Assistant as a button and can be controlled through the frontend.

```yaml
# Example configuration entry
button:
  - platform: template
    name: "Template Button"
    on_press:
      - logger.log: Button Pressed
```

## Configuration variables

- All options from [Button](/components/button#config-button).

## See Also

- {{< docref "/automations" >}}
- {{< docref "/components/button" >}}
