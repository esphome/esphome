---
description: "Instructions for setting up Over-The-Air (OTA) updates via the ESPHome web interface."
title: "Web Server OTA Updates"
params:
  seo:
    description: Instructions for setting up Over-The-Air (OTA) updates via the ESPHome web interface.
    image: system-update.svg
---

{{< anchor "config-ota_web_server" >}}

The Web Server OTA platform allows you to upload new firmware binaries to your ESPHome devices directly through the
web interface. This provides a user-friendly way to update devices without needing command-line tools or the ESPHome
dashboard.

When enabled, an "OTA Update" section appears on the device's web interface where you can select and upload a
firmware file. This is particularly useful for devices that are deployed in the field or when you want to allow
non-technical users to perform updates.

> [!WARNING]
> Enabling OTA updates through the web interface without authentication allows anyone with network access to your
> device to upload new firmware. It is **strongly recommended** to enable authentication on the web server when
> using this feature.

```yaml
# Example configuration entry
web_server:
  port: 80
  auth:
    username: !secret web_server_username
    password: !secret web_server_password

ota:
  - platform: web_server
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- All [automations](/automations) supported by {{< docref "/components/ota" >}}.

> [!NOTE]
> This platform requires the {{< docref "/components/web_server" >}} component to be configured in your device.

## Migration from Legacy Configuration

Prior to ESPHome 2025.7.0, OTA functionality was built into the `web_server` component using the `ota` option.
This has been moved to a separate platform for consistency with other OTA methods.

**Old configuration:**

```yaml
web_server:
  port: 80
  ota: true  # or ota: false to disable
```

**New configuration:**

```yaml
web_server:
  port: 80

ota:
  - platform: web_server  # Add this to enable web OTA
```

If you previously had `ota: false` in your web_server configuration, simply remove that line and don't add the
web_server OTA platform.

## Example Configurations

Basic setup with web server OTA:

```yaml
# Basic configuration
web_server:
  port: 80

ota:
  - platform: web_server
```

Secure setup with authentication:

```yaml
# Recommended: with authentication
web_server:
  port: 80
  auth:
    username: admin
    password: !secret web_password

ota:
  - platform: web_server
```

## Using the Web Interface

1. Navigate to your device's web interface at `http://<device-ip>/` or `http://<device-name>.local/`
1. If authentication is enabled, enter your username and password
1. Scroll down to the "OTA Update" section
1. Click "Choose File" and select your firmware file (`firmware.bin`  )
1. Click "Update" to start the upload
1. Wait for the upload to complete - the device will automatically reboot with the new firmware

> [!WARNING]
>
> - Always use `firmware.bin` or `firmware.ota.bin` files for OTA updates, not `firmware.factory.bin` files
> - The web interface may become unresponsive during the update process - this is normal
> - Do not power off the device during an update

## See Also

- {{< apiref "ota/ota_component.h" "ota/ota_component.h" >}}
- {{< docref "/components/ota" >}}
- {{< docref "/components/ota/esphome" >}}
- {{< docref "/components/web_server" >}}
- {{< docref "/components/safe_mode" >}}
