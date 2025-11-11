---
description: "Instructions for setting up switches that can remotely reboot the ESP in ESPHome into safe mode."
title: "Safe Mode Switch"
params:
  seo:
    description: Instructions for setting up switches that can remotely reboot the ESP in ESPHome into safe mode.
    image: restart.svg
---

The `safe_mode` switch allows you to remotely reboot your node into {{< docref "/components/safe_mode" >}}. This is useful in certain situations where a misbehaving component, or low memory state is preventing Over-The-Air updates from completing successfully.

This component requires {{< docref "/components/safe_mode" >}} to be configured.

{{< img src="safemode-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
switch:
  - platform: safe_mode
    name: "Living Room Restart (Safe Mode)"
```

## Configuration variables

- All options from [Switch](/components/switch#config-switch).

## See Also

- {{< docref "shutdown/" >}}
- {{< docref "restart/" >}}
- {{< docref "factory_reset/" >}}
- {{< docref "/components/button/safe_mode" >}}
- {{< docref "template/" >}}
- {{< apiref "safe_mode/safe_mode_switch.h" "safe_mode/safe_mode_switch.h" >}}
