---
description: "Home Assistant Time Source"
title: "Home Assistant Time Source"
---

The preferred way to get time in ESPHome is using Home Assistant.
With the `homeassistant` time platform, the {{< docref "/components/api" "native API" >}} connection
to Home Assistant will be used to periodically synchronize the current time.

> [!NOTE]
> Although you might not plan to *export* states from the node and you do not need an entity of the node
> in Home Assistant, this component still requires you to register the node under Home Assistant. See:
> [Connecting your device to Home Assistant](/guides/getting_started_hassio#connecting-your-device-to-home-assistant).

```yaml
# Example configuration entry
time:
  - platform: homeassistant
    id: homeassistant_time
```

## Configuration variables

- All options from [Base Time Configuration](/components/time#base_time_config).

## See Also
