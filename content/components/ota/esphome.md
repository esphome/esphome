---
description: "Instructions for setting up ESPHome's Over-The-Air (OTA) platform to allow remote updating of devices."
title: "ESPHome OTA Updates"
params:
  seo:
    description: Instructions for setting up ESPHome's Over-The-Air (OTA) platform to allow remote updating of devices.
    image: system-update.svg
---

{{< anchor "config-ota_esphome" >}}

ESPHome's Over-The-Air (OTA) platform allows you to remotely install modified/updated firmware binaries onto your
ESPHome devices over their network interface (Wi-Fi / Ethernet / Thread).

This platform is used by both the ESPHome dashboard as well as the command line interface (CLI) (via
`esphome run ...`  ) to install firmware onto supported devices.

In addition to OTA updates, ESPHome also supports a "safe mode" to help with recovery if/when updates don't work as
expected. This is automatically enabled by this component, but it may be disabled if desired. See
{{< docref "/components/safe_mode" >}} for details.

```yaml
# Example configuration entry
ota:
  - platform: esphome
    password: !secret ota_password
```

## Configuration variables

- **password** (*Optional*, string): The password to use for updates.

> [!IMPORTANT]
> Always use strong, unique passwords for OTA updates. See the [Security Best Practices](/guides/security_best_practices#3-ota-password-protection) guide for more information.

- **port** (*Optional*, int): The port to use for OTA updates. Defaults:

  - `3232` for the ESP32
  - `8266` for the ESP8266
  - `2040` for the RP2040
  - `8892` for Beken chips
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **version** (*Optional*, int): Version of OTA protocol to use. Version 2 is more stable. To downgrade to legacy
   ESPHome, the device should be updated with OTA version 1 first. Defaults to `2`.

- All [automations](/automations) supported by {{< docref "/components/ota" >}}.

> [!NOTE]
> After a serial upload, ESP8266 modules must be reset before OTA updates will work. If you attempt to perform an OTA
> update and receive the error message `Bad Answer: ERR: ERROR[11]: Invalid bootstrapping`, the ESP module/board
> must be power-cycled.

## Updating the Password

### Changing an Existing Password

Since the configured password is used for both compiling and uploading, the regular `esphome run <file>` command
won't work. This issue can be worked around by executing the operations separately with an `on_boot` trigger:

```yaml
esphome:
  on_boot:
    - lambda: |-
        id(my_ota).set_auth_password("New password");

ota:
  - platform: esphome
    id: my_ota
    password: "Old password"
```

The "id: my_ota" in the OTA block is important. This is referenced in the lambda.
After this trick has been used to change the password, the `on_boot` trigger may be removed and the old password
replaced with the new password in the `ota:` section.

### Adding a Password

If OTA is already enabled without a password, simply add a `password:` line to the existing `ota:` config block.

### Removing a Password

- If you know your password but want to remove it, enter an empty string: `id(my_ota).set_auth_password("");` instead of changing.
- If you no longer know your password and the web server has been activated:

  - Remove the OTA password from the configuration
  - Build a new image locally.
  - Execute the OTA update directly via the ESP web server.

## See Also

- {{< apiref "ota/ota_component.h" "ota/ota_component.h" >}}
- {{< docref "/components/ota" >}}
- {{< docref "/components/ota/http_request" >}}
- {{< docref "/components/safe_mode" >}}
