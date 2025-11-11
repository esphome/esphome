---
description: "Instructions for setting up conditions that can remotely invalidate all ESPHome preferences stored in flash and reboot ESP."
title: "Factory Reset"
params:
  seo:
    description: Instructions for setting up conditions that can remotely invalidate all ESPHome preferences stored in flash and reboot ESP.
---

The `factory_reset` component allows you to invalidate (reset) all ESPHome [preferences](/components/esphome#preferences-flash_write_interval) stored in flash memory and reboot your node.
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

## Reset by Fast Power Cycling

The `factory_reset` component can be configured to clear stored preferences by repeatedly pressing the reset button or power cycling,
which can be useful to clear the data stored in non-volatile memory on devices that can't be
connected with a serial cable. The required number of power cycles and the maximum delay between them can be configured in the
`factory_reset` component configuration. Points to note:

- The maximum delay affects only the time when the device is powered on,
  not the time when it is powered off (this can't be measured).

- The reset count will be cleared to zero when any other kind of reset occurs,
  or if the device remains powered on and running for longer than the maximum delay.

- Not available on RP2040 and RP2350 as the reset cause is not able to be determined.
- On ESP8266 this feature requires the `restore_from_flash` feature to be enabled in the {{< docref "/components/esp8266" "ESP8266 platform" >}}.

```yaml
factory_reset:
  resets_required: 5
  max_delay: 10s
```

### Configuration variables

- **resets_required** (*Optional*, integer): The number of power cycles after which the device will be reset.
  No default, if not configured the power cycle reset feature will be disabled

- **max_delay** (*Optional*, [Time](/guides/configuration-types#time)): The maximum delay between power cycles. Default: 10s

## `on_increment` Trigger

A trigger is available that will be triggered whenever the current reset cycle count changes. This happens when the
`factory_reset` component detects a power cycle, or when the
cycle count is cleared to zero by timeout or a different type of reset. Arguments passed to the trigger are:

- `x`  : The current cycle count
- `target`  : The target cycle count

```yaml
factory_reset:
  resets_required: 5
  max_delay: 10s
  on_increment:
    - logger.log:
        format: "Fast power cycle count now %u, target %u"
        args: [x, target]
```

### See Also

- {{< docref "/components/switch/factory_reset" >}}
- {{< docref "/components/button/factory_reset" >}}
- {{< docref "/components/button/shutdown" >}}
- {{< docref "/components/button/restart" >}}
- {{< docref "/components/button/safe_mode" >}}
- {{< apiref "factory_reset/factory_reset.h" "factory_reset/factory_reset.h" >}}
