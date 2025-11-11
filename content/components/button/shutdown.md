---
description: "Instructions for setting up buttons that can remotely shut down the ESP."
title: "Shutdown Button"
params:
  seo:
    description: Instructions for setting up buttons that can remotely shut down the ESP.
    image: power_settings.svg
---

The `shutdown` button platform allows you to shutdown your node remotely
through Home Assistant. It does this by putting the node into deep sleep mode with no
wakeup source selected. After enabling, the only way to startup the ESP again is by
pressing the reset button or restarting the power supply.

{{< img src="shutdown-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
button:
  - platform: shutdown
    name: "Living Room Shutdown"
```

## Configuration variables

- All options from [Button](/components/button#config-button).

## See Also

- {{< docref "restart/" >}}
- {{< docref "safe_mode/" >}}
- {{< docref "factory_reset/" >}}
- {{< docref "/components/switch/shutdown" >}}
- {{< docref "template/" >}}
- {{< apiref "shutdown/shutdown_button.h" "shutdown/shutdown_button.h" >}}
