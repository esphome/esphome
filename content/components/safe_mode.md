---
description: "Instructions for setting up ESPHome's Safe Mode to help recover from repeated boot failures."
title: "Safe Mode"
params:
  seo:
    description: Instructions for setting up ESPHome's Safe Mode to help recover from repeated boot failures.
    image: system-update.svg
---

Sometimes hardware and/or software doesn't behave as expected. ESPHome supports a "safe mode" to help recover from
repeated boot failures/reboot loops. After a specified number (the default is ten) of boot failures, the safe mode may
be invoked; in this mode, all components are disabled except serial logging, network (Wi-Fi or Ethernet) and the OTA
component(s). In most cases, this will temporarily mitigate the issue, allowing you a chance to correct it, perhaps by
uploading a new binary.

You can also force the invocation of safe mode by configuring a dedicated {{< docref "/components/button/safe_mode" "button" >}}
or {{< docref "/components/switch/safe_mode" "switch" >}} component and/or by repeatedly pressing the reset button on the board
for `num_attempts` times (see below).

```yaml
# Example configuration entry
safe_mode:
```

{{< anchor "safe_mode-configuration_variables" >}}

## Configuration variables

- **disabled** (*Optional*, boolean): Set to `true` to disable safe_mode. {{< docref "/components/ota" >}} automatically
   sets up safe mode; this allows disabling it if/when it is not wanted.

- **boot_is_good_after** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time after which the boot is considered successful.
   Defaults to `1min`.

- **num_attempts** (*Optional*, int): The number of failed boot attempts which must occur before invoking safe mode.
   Defaults to `10`.

- **reboot_timeout** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time to wait before rebooting when in safe mode.
   Defaults to `5min`.

- **on_safe_mode** (*Optional*, [Automation](/automations)): An action to be performed once when safe mode is invoked.

> [!WARNING]
> The `on_safe_mode` [automation](/automations) is intended for use by recovery actions **only**.
>
> As mentioned above, in safe mode, all components are disabled except serial logging, network (Wi-Fi or Ethernet)
> and OTA component(s).
>
> **All other components (for example, displays and sensors) are disabled and cannot be used.**

## See Also

- {{< apiref "safe_mode/safe_mode.h" "safe_mode/safe_mode.h" >}}
- {{< docref "/components/button/safe_mode" >}}
- {{< docref "/components/switch/safe_mode" >}}
- {{< docref "/guides/troubleshooting" >}} - Troubleshooting guide for debugging crashes and boot failures
