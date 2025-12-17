---
description: "Instructions for setting up the Bluetooth Proxy in ESPHome."
title: "Bluetooth Proxy"
params:
  seo:
    description: Instructions for setting up the Bluetooth Proxy in ESPHome.
    image: bluetooth.svg
---

Home Assistant can expand its Bluetooth reach by communicating through the Bluetooth proxy component in ESPHome.
The individual device integrations in Home Assistant (such as BTHome) will receive the data from the Bluetooth
Integration in Home Assistant which automatically aggregates all ESPHome Bluetooth proxies with any USB Bluetooth
Adapters you might have. This exceptional feature offers fault tolerant connection between the Bluetooth devices
and Home Assistant.

Note that while this component is named `bluetooth_proxy`, only BLE devices (and their Home Assistant integrations)
are supported.

If you'd like to buy a ready-made Bluetooth proxy or flash your own device, see
[ESPHome projects with Bluetooth proxy support](/projects/?type=bluetooth).

## Configuration

```yaml
bluetooth_proxy:
  # Active connections are now enabled by default
  # To disable active connections (previous default behavior), use:
  # active: false
```

- **active** (*Optional*, boolean): Enables proxying active GATT connections to BLE devices.
  This is separate from active *scanning* (configured in [ESP32 BLE Tracker](/components/esp32_ble_tracker)).
  Defaults to `true`.
- **cache_services** (*Optional*, boolean): Enables caching GATT services in NVS flash storage which
  significantly speeds up active connections. Defaults to `true`.
- **connection_slots** (*Optional*, int): The maximum number of BLE connection slots to use.
  Each configured slot consumes ~1KB of RAM, with a maximum of `9`. It is recommended not to exceed `5`
  connection slots to avoid stability and memory issues. Defaults to `3`.
  Ethernet-based proxies can generally handle `4` connection slots reliably.
  The value must not exceed the total configured `max_connections`
  for [ESP32 BLE](/components/esp32_ble).

The Bluetooth proxy depends on [ESP32 BLE Tracker](/components/esp32_ble_tracker) so make sure to add that
to your configuration.

### How Active Connections Work

The Bluetooth proxy provides Home Assistant with a limited number of simultaneous active GATT connections
(configured via `connection_slots`). The default is 3 slots. Ethernet-based proxies can generally handle
4 slots reliably since they don't share the radio with WiFi traffic. Set `connection_slots: 4` if you need
more connections (each slot uses additional RAM).

Devices that stay connected continuously (like some locks or thermostats) use one slot the entire time.
Devices that connect briefly to exchange data and then disconnect (like many sensors) free up the slot
for other devices, so you can use more devices than you have slots.

Passively broadcasted sensor data (advertised by devices without requiring active connections, such as
many BTHome sensors) is received separately and is not limited by the number of connection slots.

## Improving reception performance

Use a board with an Ethernet connection to the network to offload ESP32's radio module
from WiFi traffic, which improves Bluetooth performance. For best results, use a board
with an external antenna (e.g., Olimex ESP32-PoE-ISO-EA over Olimex ESP32-PoE-ISO).

> [!NOTE]
> The default scan parameters are recommended for most users. Changing `interval` or
> `window` from their defaults typically provides no meaningful benefit while increasing
> CPU usage and network traffic. Aggressive scan settings can cause overheating on
> PoE-based proxies and WiFi instability on WiFi-based proxies.

### Passive vs Active Scanning

Passive scanning works for most BLE devices and is sufficient for ongoing operation.
Active scanning requests additional scan response data from devices and is typically
only needed when initially adding new devices to Home Assistant. Active scanning also
increases battery drain on battery-powered BLE devices.

The [ESP32 BLE Tracker](/components/esp32_ble_tracker) component defaults to active scanning
(`active: true`). If you experience overheating, you can try switching to passive scanning
if your devices don't require active scans:

```yaml
esp32_ble_tracker:
  scan_parameters:
    active: false
```

Avoid placing the ESP node in racks, close to routers/switches or other network equipment as EMI
interference will degrade Bluetooth signal reception. For best results put as far away as possible,
at least 3 meters distance from any other such equipment. Place your ESPHome devices close to the
Bluetooth devices that you want to interact with for the best experience.

## Complete sample recommended configuration for a WiFi-connected Bluetooth proxy

Below is a complete sample recommended configuration for a WiFi-connected Bluetooth proxy. If you
experience issues with your proxy, try reducing your configuration to be as similar to this as possible.

```yaml
substitutions:
  name: my-bluetooth-proxy

esphome:
  name: ${name}
  name_add_mac_suffix: true

esp32:
  variant: esp32
  framework:
    type: esp-idf

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

# Enable logging
logger:

# Enable Home Assistant API
api:

ota:
  platform: esphome

esp32_ble_tracker:

bluetooth_proxy:
  active: true
```

## Complete sample recommended configuration for an ethernet-connected Bluetooth proxy

Below is a complete sample recommended configuration for an ethernet-connected Bluetooth proxy. This
configuration is not for a Wi-Fi based proxy. If you experience issues with your proxy, try reducing
your configuration to be as similar to this as possible.

This configuration is for an Olimex ESP32-PoE-ISO board with an Ethernet connection to the network.
If you use a different board, you must change the `board` substitution to match your board.

```yaml
substitutions:
  name: my-bluetooth-proxy
  board: esp32-poe-iso

esphome:
  name: ${name}
  name_add_mac_suffix: true

esp32:
  board: ${board}
  variant: esp32
  framework:
    type: esp-idf

ethernet:
  type: LAN8720
  mdc_pin: GPIO23
  mdio_pin: GPIO18
  clk:
    mode: CLK_OUT
    pin: GPIO17
  phy_addr: 0
  power_pin: GPIO12

# Enable logging
logger:

# Enable Home Assistant API
api:

ota:
  platform: esphome

esp32_ble_tracker:
  # The default scan parameters are recommended.
  # Aggressive scan settings (e.g., interval/window of 1100ms) typically
  # provide no benefit while increasing CPU usage and may cause
  # overheating on some PoE-based proxies.

bluetooth_proxy:
  active: true
  connection_slots: 4
```

## Troubleshooting

### Memory Issues

If you experience memory issues, consider the following:

- **Framework:** The `esp-idf` framework is recommended over `arduino` as it uses less memory.
  When switching frameworks, update the device with a serial cable as the partition table differs.
  [OTA](/components/ota) updates will not change the partition table.
- **Web Server:** The [Web Server](/components/web_server) component uses additional RAM.
  Disabling it can help if you experience memory-related issues.

### Device Compatibility

Not all BLE devices are supported and ESPHome does not decode or keep a list. To find out if your device
is supported, please search for it in the
[Home Assistant Integrations](https://www.home-assistant.io/integrations/) list.

## See Also

- [ESP32 BLE Tracker](/components/esp32_ble_tracker)
- {{< apiref "bluetooth_proxy/bluetooth_proxy.h" "bluetooth_proxy/bluetooth_proxy.h" >}}
- BTHome <https://bthome.io/>
