---
description: "Instructions for using the ESP32 Hosted update platform to manage co-processor firmware updates."
title: "ESP32 Hosted Co-processor Update"
params:
  seo:
    description: Instructions for using the ESP32 Hosted update platform to manage co-processor firmware updates.
    image: system-update.svg
---

This platform allows you to update the firmware of an ESP32 co-processor connected via the
[ESP32 Hosted](/components/esp32_hosted) component. Two update modes are supported:

- **Embedded mode**: The firmware binary is embedded into your device's flash at compile time
- **HTTP mode**: The firmware is fetched from a remote URL at runtime

The component automatically detects the current co-processor firmware version and compares it to the
available version. If the versions differ, an update becomes available in Home Assistant
or through the ESPHome API.

## Embedded Mode

In embedded mode, the firmware binary is compiled into your device's flash. This is useful when you want
to bundle a specific firmware version with your device.

```yaml
# Example configuration entry for embedded mode
update:
  - platform: esp32_hosted
    type: embedded
    path: coprocessor-firmware.bin
    sha256: 1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef
```

## HTTP Mode

In HTTP mode, the firmware is fetched from a remote manifest URL. This allows automatic updates
without recompiling your ESPHome configuration. The component will periodically check for updates
and select the best compatible version based on the host library version.

```yaml
# Example configuration entry for HTTP mode
http_request:

update:
  - platform: esp32_hosted
    type: http
    source: https://esphome.github.io/esp-hosted-firmware/manifest/esp32c6.json
    update_interval: 6h
```

### Manifest Format

The HTTP mode requires a JSON manifest file with the following format:

```json
{
  "versions": [
    {
      "version": "2.7.0",
      "url": "https://example.com/firmware/esp32c6-2.7.0.bin",
      "sha256": "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"
    },
    {
      "version": "2.6.0",
      "url": "https://example.com/firmware/esp32c6-2.6.0.bin",
      "sha256": "fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321"
    }
  ]
}
```

The component will automatically select the highest version that is compatible with the host library
(i.e., the firmware version must be less than or equal to the ESP-Hosted library version compiled
into ESPHome).

{{< anchor "update_esp32_hosted-configuration_variables" >}}

## Configuration variables

- **type** (**Required**, string): The update mode to use. Must be either `embedded` or `http`.

### Embedded mode options

- **path** (**Required**, string): Path to the co-processor firmware binary file (`.bin`).
  The path is relative to your ESPHome configuration file.

- **sha256** (**Required**, string): SHA256 hash of the firmware binary file. This is used to verify
  the integrity of the firmware both at compile time and at runtime before flashing to the co-processor.

### HTTP mode options

- **source** (**Required**, url): URL to the JSON manifest file containing available firmware versions.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): How often to check for updates.
  Defaults to `6h`.

### Common options

- All other options from [Update](/components/update#config-update).

## Platform requirements

This update platform requires:

- **Host device** (running ESPHome): `ESP32-H2` or `ESP32-P4`
- **Co-processor** (being updated): Any ESP32 variant supported by ESP-Hosted (e.g., `ESP32-C6` as shown in the example)

For embedded mode, the host device must have sufficient flash space to store the co-processor firmware binary.

## Co-processor firmware

Pre-built firmware binaries and manifests for supported co-processor variants are available at
[esphome.github.io/esp-hosted-firmware](https://esphome.github.io/esp-hosted-firmware/).
These can be used directly with HTTP mode, or downloaded for use with embedded mode.

For instructions on building custom firmware, see the [esp-hosted-firmware](https://github.com/esphome/esp-hosted-firmware) repository.

## See Also

- [ESP32 Hosted](/components/esp32_hosted)
- [Update](/components/update)
- [HTTP Request](/components/http_request)
- [ESP-Hosted Firmware](https://esphome.github.io/esp-hosted-firmware/)
- [ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu) by [Espressif Systems](https://www.espressif.com/)
- {{< apiref "esp32_hosted/update/esp32_hosted_update.h" "esp32_hosted/update/esp32_hosted_update.h" >}}
