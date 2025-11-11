---
description: "Control the state of BLE clients."
title: "BLE Client Switch"
params:
  seo:
    description: Control the state of BLE clients.
    image: bluetooth.svg
---

The `ble_client` component is a switch platform that is used to enable and disable a `ble_client`. This has
several uses, such as minimizing battery usage or for allowing other clients (Eg phone apps) to connect to the device.

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

switch:
  - platform: ble_client
    ble_client_id: itag_black
    name: "Enable iTag"
```

## Configuration variables

- **ble_client_id** (**Required**, [ID](/guides/configuration-types#id)): ID of the associated BLE client.
- All other options from [Switch](/components/switch#config-switch).

## See Also

- {{< docref "/components/ble_client" >}}
- {{< apiref "ble_client/switch/ble_switch.h" "ble_client/switch/ble_switch.h" >}}
