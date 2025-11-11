---
description: "Instructions for setting up switches that can remotely invalidate all ESPHome preferences stored in flash and reboot ESP."
title: "Factory Reset Switch"
params:
  seo:
    description: Instructions for setting up switches that can remotely invalidate all ESPHome preferences stored in flash and reboot ESP.
    image: restart.svg
---

The `factory_reset` switch allows you to remotely invalidate (reset) all ESPHome [preferences](/components/esphome#preferences-flash_write_interval) stored in flash memory and reboot your node.
After reboot all states, parameters and variables will be reinitialized with their default values. This is useful:

- for devices preflashed with ESPHome to reset behavior back to factory state
- in case of moving a device to a new environment or starting a new use-case (e.g. reset counters or state)
- for privacy concerns when giving away a device

> [!NOTE]
> **USE WITH GREAT CAUTION!** All credentials, global variables, counters and saved states stored in non-volatile memory will be lost with no chance of recovering them.
> Even raw reading of flash memory with `esptool` will not help, since data is physically erased from flash memory.
>
> For devices configured using {{< docref "/components/captive_portal" "captive portal" >}}, this will reset WiFi settings as well, thus making such devices offline.
> You'll need to be in close proximity to your device to configure it again using a built-in WiFi access point and captive portal.

{{< img src="factory-rst-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
switch:
  - platform: factory_reset
    name: Restart with Factory Default Settings
```

## Configuration variables

- All options from [Switch](/components/switch#config-switch).

## See Also

- {{< docref "/components/factory_reset" >}}
- {{< docref "shutdown/" >}}
- {{< docref "restart/" >}}
- {{< docref "safe_mode/" >}}
- {{< docref "/components/button/factory_reset" >}}
- {{< docref "template/" >}}
- {{< apiref "factory_reset/factory_reset_switch.h" "factory_reset/factory_reset_switch.h" >}}
