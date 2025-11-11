---
description: "Instructions for setting up RSSI sensors for the ESP32 BLE."
title: "ESP32 Bluetooth Low Energy RSSI Sensor"
params:
  seo:
    description: Instructions for setting up RSSI sensors for the ESP32 BLE.
    image: bluetooth.svg
---

The `ble_rssi` sensor platform lets you track the RSSI value or signal strength of a
BLE device. See [the binary sensor setup](/components/binary_sensor/ble_presence#esp32_ble_tracker-setting_up_devices) for
instructions for setting up this platform.

> [!WARNING]
> The BLE software stack on the ESP32 consumes a significant amount of RAM on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. Memory-intensive components such as {{< docref "/components/voice_assistant" >}} and other
> audio components are most likely to cause issues.

```yaml
# Example configuration entry
esp32_ble_tracker:

sensor:
  # RSSI based on MAC address
  - platform: ble_rssi
    mac_address: XX:XX:XX:XX:XX:XX
    name: "BLE Google Home Mini RSSI value"
  # RSSI based on Identity Resolving Key (IRK)
  - platform: ble_rssi
    irk: 1234567890abcdef1234567890abcdef
    name: "BLE Tracker iPhone"
  # RSSI based on Service UUID
  - platform: ble_rssi
    service_uuid: '11aa'
    name: "BLE Test Service 16 bit RSSI value"
  # RSSI based on iBeacon UUID
  - platform: ble_rssi
    ibeacon_uuid: '68586f1e-89c2-11eb-8dcd-0242ac130003'
    name: "BLE Test Service iBeacon RSSI value"
```

> [!NOTE]
> Service UUID can be 16 bit long, as in the example, but it can also be 32 bit long
> like `1122aaff`, or 128 bit long like `11223344-5566-7788-99aa-bbccddeeff00`.

## Configuration variables

- **mac_address** (*Optional*, MAC Address): The MAC address to track for this
  sensor. Note that exactly one of `mac_address`, `irk`, `service_uuid` or `ibeacon_uuid`
  must be present.

- **irk** (*Optional*, 16 byte hex string): The Identity Resolving Key (IRK) to track for this
  sensor. Note that exactly one of `mac_address`, `irk`, `service_uuid` or `ibeacon_uuid`
  must be present.

- **service_uuid** (*Optional*, 16 bit, 32 bit, or 128 bit BLE Service UUID): The BLE
  Service UUID which can be tracked if the device randomizes the MAC address. Note that exactly one of
  `mac_address`, `irk`, `service_uuid` or `ibeacon_uuid` must be present.

- **ibeacon_uuid** (*Optional*, string): The [universally unique identifier](https://en.wikipedia.org/wiki/Universally_unique_identifier)
  to identify the beacon that needs to be tracked. Note that exactly one of `mac_address`,
  `irk`, `service_uuid` or `ibeacon_uuid` must be present.

- **ibeacon_major** (*Optional*, int): The iBeacon major identifier of the beacon that needs
  to be tracked. Usually used to group beacons, for example for grouping all beacons in the
  same building.

- **ibeacon_minor** (*Optional*, int): The iBeacon minor identifier of the beacon that needs
  to be tracked. Usually used to identify beacons within an iBeacon group.

- All other options from [Sensor](/components/sensor).

## See Also

- {{< docref "/components/esp32_ble_tracker" >}}
- {{< docref "/components/sensor" >}}
- {{< apiref "ble_rssi/ble_rssi_sensor.h" "ble_rssi/ble_rssi_sensor.h" >}}
