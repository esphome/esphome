---
description: "Instructions for using ESPHome's http_request update platform to manage updates on your devices."
title: "Managed Updates via HTTP Request"
params:
  seo:
    description: Instructions for using ESPHome's http_request update platform to manage updates on your devices.
    image: system-update.svg
---

This platform allows you to manage the deployment of updates to your ESPHome devices. It works by reading a
[JSON manifest file](#update_http_request-manifest_format) and using it to determine the presence of an update.

To use it, the following components are required in your device's configuration:

- {{< docref "/components/http_request" >}}
- {{< docref "/components/ota/http_request" >}}

```yaml
# Example configuration entry
update:
  - platform: http_request
    name: Firmware Update
    source: http://example.com/manifest.json
```

{{< anchor "update_http_request-configuration_variables" >}}

## Configuration variables

- **source** (**Required**, string): The URL of the YAML manifest file containing the firmware metadata.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval at which to check for (**not install**) updates.
  Defaults to 6 hours.

- All other options from [Update](/components/update#config-update).

{{< anchor "update_http_request-manifest_format" >}}

## Update Manifest Format

This component expects the [ESP-Web-Tools manifest](https://github.com/esphome/esp-web-tools) with an extension in
the `ota` block that is structured as follows:

```json
{
  "name": "My ESPHome Project",
  "version": "2024.6.1",
  "builds": [
    {
      "chipFamily": "ESP32-C3",
      "ota": {
        "md5": "1234567890abcdef1234567890abcdef",
        "path": "/local/esp32c3/firmware.bin",
        "release_url": "http://example.com/releases/10",
        "summary": "Another update",
      }
    }
  ]
}
```

While `release_url` and `summary` are optional, all other fields shown here are required.

If `path` begins with:

- `http` or `https`  : `path` is treated as full URL which will be used to obtain the firmware binary.
- A forward slash (`/`  ): `path` will be appended to the hostname (an "absolute" path) specified for `source` (as above).
- Any other character: `path` will be appended to `source` (as specified above) after trimming the manifest file name.

Note that there may be multiple `builds` specified within a single JSON file.

## See Also

- {{< docref "http_request/" >}}
- {{< docref "/components/ota/http_request" >}}
- {{< docref "/components/ota" >}}
- {{< apiref "update/update_entity.h" "update/update_entity.h" >}}
