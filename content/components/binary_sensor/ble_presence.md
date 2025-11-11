---
description: "Instructions for setting up BLE binary sensors for the ESP32."
title: "ESP32 Bluetooth Low Energy Device"
params:
  seo:
    description: Instructions for setting up BLE binary sensors for the ESP32.
    image: bluetooth.svg
---

The `ble_presence` binary sensor platform lets you track the presence of a Bluetooth Low Energy device.

> [!WARNING]
> The BLE software stack on the ESP32 consumes a significant amount of RAM on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. Memory-intensive components such as {{< docref "/components/voice_assistant" >}} and other
> audio components are most likely to cause issues.

{{< img src="esp32_ble-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
esp32_ble_tracker:

binary_sensor:
  # Presence based on MAC address
  - platform: ble_presence
    mac_address: XX:XX:XX:XX:XX:XX
    name: "ESP32 BLE Tracker Google Home Mini"
    min_rssi: -80dB
  # Presence based on Identity Resolving Key (IRK)
  - platform: ble_presence
    irk: 1234567890abcdef1234567890abcdef
    name: "ESP32 BLE Tracker iPhone"
  # Presence based on BLE Service UUID
  - platform: ble_presence
    service_uuid: '11aa'
    name: "ESP32 BLE Tracker Test Service 16 bit"
    timeout: 45s
  # Presence based on iBeacon UUID
  - platform: ble_presence
    ibeacon_uuid: '68586f1e-89c2-11eb-8dcd-0242ac130003'
    name: "ESP32 BLE Tracker Test Service iBeacon"
```

> [!NOTE]
> Service UUID can be 16 bit long, as in the example, but it can also be 32 bit long
> like `1122aaff`, or 128 bit long like `11223344-5566-7788-99aa-bbccddeeff00`.

## Configuration variables

- **mac_address** (*Optional*, MAC Address): The MAC address to track for this
   binary sensor. Note that exactly one of `mac_address`, `irk`, `service_uuid` or `ibeacon_uuid`
   must be present.

- **irk** (*Optional*, 16 byte hex string): The Identity Resolving Key (IRK) to track for this
   binary sensor. Note that exactly one of `mac_address`, `irk`, `service_uuid` or `ibeacon_uuid`
   must be present.

- **service_uuid** (*Optional*, string): 16 bit, 32 bit, or 128 bit BLE Service UUID
   which can be tracked if the device randomizes the MAC address. Note that exactly one of
   `mac_address`, `irk`, `service_uuid` or `ibeacon_uuid` must be present.

- **ibeacon_uuid** (*Optional*, string): The [universally unique identifier](https://en.wikipedia.org/wiki/Universally_unique_identifier)
   to identify the beacon that needs to be tracked. Note that exactly one of `mac_address`,
   `irk`, `service_uuid` or `ibeacon_uuid` must be present.

- **ibeacon_major** (*Optional*, int): The iBeacon major identifier of the beacon that needs
   to be tracked. Usually used to group beacons, for example for grouping all beacons in the
   same building.

- **ibeacon_minor** (*Optional*, int): The iBeacon minor identifier of the beacon that needs
   to be tracked. Usually used to identify beacons within an iBeacon group.

- **min_rssi** (*Optional*, int): at which minimum RSSI level would the component report the device be present.
- **timeout** (*Optional*, [Time](/guides/configuration-types#time)): The delay after last detecting the device before publishing not present state.
   The default is 5 minutes.

- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

{{< anchor "esp32_ble_tracker-setting_up_devices" >}}

## Setting Up Devices

To set up binary sensors for specific BLE beacons you first have to know which MAC address
to track. Most devices show this screen in some settings menu. If you don't know the MAC address,
however, you can use the `esp32_ble_tracker` hub without any binary sensors attached and read through
the logs to see discovered Bluetooth Low Energy devices.

```yaml
# Example configuration entry for finding
# MAC addresses, Service UUIDs, iBeacon UUIDs, and identifiers
esp32_ble_tracker:
  on_ble_advertise:
    - then:

logger:
  level: VERY_VERBOSE
```

Using the configuration above, first, you should see a `Starting scan...` debug message at
boot-up. Then, when a BLE device is discovered, you should see messages like
`Parse Result:` together with some information about their MAC address, address type,
advertised name, Service UUIDs, iBeacon UUIDs, iBeacon major and minor identifiers,
BLE manufacturer ID and data, RSSI, and other data useful for debugging purposes.
You can find the [official list of manufacturer IDs](https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/company_identifiers/company_identifiers.yaml) to help find your device.
Note that this is useful only during set-up and a less verbose log level
should be specified afterwards. If you don't see these messages, your device is unfortunately
currently not supported.

Please note that devices that show a `RANDOM` address type in the logs probably use a privacy
feature called Resolvable Private Addresses to avoid BLE tracking. Since their MAC-address periodically
changes, they can't be tracked by the MAC address. However, if you know the devices "Identity Resolving
Key" (IRK), you can check if the generated private MAC address belongs to the device with the IRK.

There is no support to obtain the key with ESPHome. For now you will have to use one of the options
described in the ESPresense project: <https://espresense.com/beacons>

Alternatively you can:

- Create a BLE beacon, set a unique 16 bit, 32 bit or 128 bit Service UUID and track your device
   based on that. Make sure you don't pick a [GATT Service UUID](https://www.bluetooth.com/specifications/gatt/services/), otherwise generic services
   might give you incorrect tracking results.

- Create an iBeacon and track it based on its iBeacon UUID. You can also optionally specify
   major and minor numbers to match if additional filtering is required. ESPHome offers this
   functionality via the {{< docref "/components/esp32_ble_beacon" "ESP32 Bluetooth Low Energy Beacon" >}}
   component. Several iOS and Android applications, including the open source Home Assistant
   mobile application also provide means to create iBeacons.

## See Also

- {{< docref "/components/esp32_ble_tracker" >}}
- {{< docref "/components/esp32_ble_beacon" >}}
- {{< docref "/components/binary_sensor" >}}
- {{< apiref "ble_presence/ble_presence.h" "ble_presence/ble_presence.h" >}}
- [ESP32 BLE for Arduino](https://github.com/nkolban/ESP32_BLE_Arduino) by [Neil Kolban](https://github.com/nkolban).
