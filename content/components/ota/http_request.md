---
description: "Instructions for setting up Over-The-Air (OTA) updates for ESPs to download firmwares remotely by HTTP."
title: "OTA Update via HTTP Request"
params:
  seo:
    description: Instructions for setting up Over-The-Air (OTA) updates for ESPs to download firmwares remotely by HTTP.
    image: system-update.svg
---

The OTA (Over The Air) via HTTP Request update component allows your devices to install updated firmware on their own.
To use it, in your device's configuration, you specify a URL from which the device will download the binary
file (firmware). To trigger the update, an ESPHome [action](/automations/actions#all-actions) is used which initiates the
download and installation of the new firmware. Once complete, the device is rebooted, invoking the new firmware.

Since the device functions as an HTTP(S) client, it can be on a foreign network or behind a firewall. This mechanism
is primarily useful with either standalone or MQTT-only devices.

To use this platform, the {{< docref "http_request/" >}} component must be present in your configuration.

```yaml
# Example configuration entry
ota:
  - platform: http_request
```

## Configuration variables

- All [automations](/automations) supported by {{< docref "/components/ota" >}}.

{{< anchor "ota_http_request-flash_action" >}}

## `ota.http_request.flash` Action

This action triggers the download and installation of the updated firmware from the configured URL. As it's an
ESPHome [action](/automations/actions#all-actions), it may be used in any ESPHome [automation(s)](/automations).

```yaml
on_...:
  then:
    - ota.http_request.flash:
        md5_url: http://example.com/firmware.md5
        url: https://example.com/firmware.ota.bin
    - logger.log: "This message should be not displayed because the device reboots"
```

### Configuration variables

- **md5** (*Optional*, string, [templatable](/automations/templates)): The
  [MD5sum](https://en.wikipedia.org/wiki/Md5sum) of the firmware file pointed to by `url` (below). May not be used
  with `md5_url` (below); must be specified if `md5_url` is not.

- **md5_url** (*Optional*, string, [templatable](/automations/templates)): The URL of the file containing an
  [MD5sum](https://en.wikipedia.org/wiki/Md5sum) of the firmware file pointed to by `url` (below). May not be used
  with `md5` (above); must be specified if `md5` is not.

- **url** (**Required**, string, [templatable](/automations/templates)): The URL of the binary file containing the
  (new) firmware to be installed.

- **username** (*Optional*, string, [templatable](/automations/templates)): The username to use for HTTP basic
  authentication.

- **password** (*Optional*, string, [templatable](/automations/templates)): The password to use for HTTP basic
  authentication.

> [!NOTE]
>
> - You can obtain the `firmware.ota.bin` file from either:
>
>   - **ESPHome dashboard** (HA add-on): download in *"OTA format"* (formerly "legacy format")
>   - **ESPHome CLI**: the directory `.esphome/build/project/.pioenvs/project/firmware.ota.bin`
>
>     ...where *"project"* is the name of your ESPHome device/project.
>
>   You **cannot** use `firmware.factory.bin` or *"Factory format"* (formerly "Modern format") with this component.
>
> - `username` and `password` must be [URL-encoded](https://en.wikipedia.org/wiki/Percent-encoding) if they
>   include special characters.
>
> - The [MD5sum](https://en.wikipedia.org/wiki/Md5sum) of the firmware binary file is an ASCII file (also known
>   as "plain text", typically found in files with a `.txt` extension) consisting of 32 lowercase hexadecimal
>   characters. It can be obtained and saved to a file with the following command(s):
>
>   - On macOS:
>
> ```shell
>         md5 -q firmware.ota.bin > firmware.md5
> ```
>
> - On most Linux distributions:
>
> ```shell
>         md5sum firmware.ota.bin > firmware.md5
> ```
>
> - On Windows/PowerShell:
>
> ```shell
>         (Get-FileHash -Path firmware.ota.bin -Algorithm md5).Hash.ToLower() | Out-File -FilePath firmware.md5 -Encoding ASCII
> ```
>
> This will generate the MD5 hash of the `firmware.ota.bin` file and write the resulting hash value to the
> `firmware.md5` file. The `md5_url` configuration variable should point to this file on the web server.
> It is used by the OTA updating mechanism to ensure the integrity of the (new) firmware as it is installed.
>
> **If, for any reason, the MD5sum provided does not match the MD5sum computed as the firmware is installed, the
> device will continue to use the original firmware and the new firmware is discarded.**

## See Also

- {{< apiref "ota/ota_component.h" "ota/ota_component.h" >}}
- {{< docref "/components/ota" >}}
- {{< docref "/components/ota/esphome" >}}
- {{< docref "/components/safe_mode" >}}
