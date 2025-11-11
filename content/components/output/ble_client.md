---
description: "Writes a binary value to a BLE device."
title: "BLE Client Binary Output"
params:
  seo:
    description: Writes a binary value to a BLE device.
    image: bluetooth.svg
---

The `ble_client` component is a output that can write a binary value to service characteristics of BLE devices.

For more information on BLE services and characteristics, see {{< docref "/components/ble_client" >}}.

> [!WARNING]
> The BLE software stack on the ESP32 consumes a significant amount of RAM on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. Memory-intensive components such as {{< docref "/components/voice_assistant" >}} and other
> audio components are most likely to cause issues.

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: itag_black

output:
  - platform: ble_client
    ble_client_id: itag_black
    service_uuid: "10110000-5354-4F52-5A26-4249434B454C"
    characteristic_uuid: "10110013-5354-4f52-5a26-4249434b454c"
    require_response: false
```

## Configuration variables

- **ble_client_id** (**Required**, [ID](/guides/configuration-types#id)): ID of the associated BLE client.
- **service_uuid** (**Required**, UUID): UUID of the service on the device.
- **characteristic_uuid** (**Required**, UUID): UUID of the service's characteristic to write to.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID to use for code generation, and for reference by dependent components.
- **require_response** (*Optional*, boolean): Control whether to require a remote response from the device when writing.
  Whether or not this is required will vary by device. Defaults to `false`

- All other options from [Output](/components/output#config-output).

## See Also

- {{< docref "/components/output" >}}
- {{< docref "/components/ble_client" >}}
