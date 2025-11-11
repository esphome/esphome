---
description: "Information for creating and sharing devices using ESPHome firmware."
title: "Sharing ESPHome devices"
params:
  seo:
    description: Information for creating and sharing devices using ESPHome firmware.
---

We have added configuration options to ESPHome to make it easier
to create, configure, install and distribute devices running ESPHome.

No part of a "template" configuration should have any references to secrets,
or have passwords pre-applied. ESPHome makes it easy for the end-user to add these
themselves after they adopt the device into their own ESPHome dashboard.

## Example configuration

```yaml
# These substitutions allow the end user to override certain values
substitutions:
  name: "project-template"
  friendly_name: "Project Template"

esphome:
  name: "${name}"
  # Friendly names are used where appropriate in Home Assistant
  friendly_name: "${friendly_name}"
  # Automatically add the mac address to the name
  # so you can use a single firmware for all devices
  name_add_mac_suffix: true

  # This will allow for (future) project identification,
  # configuration and updates.
  project:
    name: esphome.project-template
    version: "1.0"

# To be able to get logs from the device via serial and api.
logger:

# API is a requirement of the dashboard import.
api:

# OTA is required for Over-the-Air updating
ota:
  platform: esphome

# This should point to the public location of this yaml file.
dashboard_import:
  package_import_url: github://esphome/esphome-project-template/project-template-esp32.yaml@v6
  import_full_config: false # or true

wifi:
  # Set up a wifi access point
  ap:
    password: "12345678"

# In combination with the `ap` this allows the user
# to provision wifi credentials to the device.
captive_portal:

# Sets up Bluetooth LE (Only on ESP32) to allow the user
# to provision wifi credentials to the device.
esp32_improv:
  authorizer: none

# Sets up the improv via serial client for Wi-Fi provisioning
improv_serial:
  next_url: https://example.com/project-template/manual?ip={{ip_address}}&name={{device_name}}&version={{esphome_version}}
```

## Relevant Documentation

- `name_add_mac_suffix` - [Adding the MAC address as a suffix to the device name](/components/esphome#esphome-mac_suffix)
- `project` - [Project information](/components/esphome#esphome-creators_project)
- `esp32_improv` - {{< docref "/components/esp32_improv" >}}
- `captive_portal` - {{< docref "/components/captive_portal" >}}
- `wifi` -> `ap` allows you to flash a device that will not contain any
  credentials and they must be set by the user via either the `ap` + `captive_portal` or
  the `esp32_improv` / `improv_serial` components.

- `dashboard_import`

  > [!NOTE]
  > The [Project information](/components/esphome#esphome-creators_project) above is required for adoption to work in the Dashboard.

  - `package_import_url` - This should point to the public repository containing
    the configuration for the device so that the user's ESPHome dashboard can autodetect this device and
    create a minimal YAML using [Remote/Git Packages](/components/packages#config-git_packages).

  - `import_full_config` - This signals if ESPHome should download the entire YAML file as the user's config
    YAML instead of referencing the package. Set this to `true` if you are creating a tutorial to let users
    easily tweak the whole configuration or be able to uncomment follow-up tutorial steps.

- `improv_serial` - {{< docref "/components/improv_serial" >}}

## See Also
