---
description: "Instructions for setting up switches that can remotely reboot the ESP in ESPHome."
title: "Restart Switch"
params:
  seo:
    description: Instructions for setting up switches that can remotely reboot the ESP in ESPHome.
    image: restart.svg
---

The `restart` switch platform allows you to restart your node remotely
through Home Assistant.

{{< img src="restart-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
switch:
  - platform: restart
    name: "Living Room Restart"
```

## Configuration variables

- All options from [Switch](/components/switch#config-switch).

## See Also

- {{< docref "shutdown/" >}}
- {{< docref "safe_mode/" >}}
- {{< docref "factory_reset/" >}}
- {{< docref "/components/button/restart" >}}
- {{< docref "template/" >}}
- {{< apiref "restart/switch/restart_switch.h" "restart/switch/restart_switch.h" >}}
