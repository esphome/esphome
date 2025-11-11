---
description: "Instructions for setting up buttons that can remotely reboot the ESP in ESPHome."
title: "Restart Button"
params:
  seo:
    description: Instructions for setting up buttons that can remotely reboot the ESP in ESPHome.
    image: restart.svg
---

The `restart` button platform allows you to restart your node remotely
through Home Assistant.

```yaml
# Example configuration entry
button:
  - platform: restart
    name: "Living Room Restart"
```

## Configuration variables

- All options from [Button](/components/button#config-button).

## See Also

- {{< docref "shutdown/" >}}
- {{< docref "safe_mode/" >}}
- {{< docref "factory_reset/" >}}
- {{< docref "/components/switch/restart" >}}
- {{< docref "template/" >}}
- {{< apiref "restart/button/restart_button.h" "restart/button/restart_button.h" >}}
