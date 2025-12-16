---
description: "Instructions for setting up WiFi info text sensors."
title: "WiFi Info Text Sensor"
params:
  seo:
    description: Instructions for setting up WiFi info text sensors.
    image: network-wifi.svg
---

The `wifi_info` text sensor platform exposes different WiFi information
via text sensors.

```yaml
# Example configuration entry
text_sensor:
  - platform: wifi_info
    ip_address:
      name: Device IP Address
      address_0:
        name: Device IP Address 0
      address_1:
        name: Device IP Address 1
      address_2:
        name: Device IP Address 2
      address_3:
        name: Device IP Address 3
      address_4:
        name: Device IP Address 4
    ssid:
      name: Device Connected SSID
    bssid:
      name: Device Connected BSSID
    mac_address:
      name: Device Mac Wifi Address
    scan_results:
      name: Device Latest Scan Results
    dns_address:
      name: Device DNS Address
    power_save_mode:
      name: Device Wifi Power Save Mode
```

## Configuration variables

- **ip_address** (*Optional*): Expose the IP Address of the device as a text sensor.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **address_0** (*Optional*): With dual stack (IPv4 and IPv6) the device will have at least two IP addresses, often more.
  To report all addresses the configuration may have up to five sub-sensors **address_0** to **address_4**.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **ssid** (*Optional*): Expose the SSID of the currently connected WiFi network as a text sensor. All options from
  [Text Sensor](/components/text_sensor#config-text_sensor).

- **bssid** (*Optional*): Expose the BSSID of the currently connected WiFi network as a text sensor. All options from
  [Text Sensor](/components/text_sensor#config-text_sensor).

- **mac_address** (*Optional*): Expose the Mac Address of the WiFi card. All options from
  [Text Sensor](/components/text_sensor#config-text_sensor).

- **scan_results** (*Optional*): Expose the latest networks found during the latest scan. All options from
  [Text Sensor](/components/text_sensor#config-text_sensor).

- **dns_address** (*Optional*): Expose the DNS Address of the device as text sensor.
  [Text Sensor](/components/text_sensor#config-text_sensor).

- **power_save_mode** (*Optional*) Expose the WiFi Power save mode of the device as a text sensor.

## See Also

- {{< docref "/components/wifi" >}}
- {{< docref "/components/sensor/wifi_signal" >}}
- {{< apiref "wifi_info/wifi_info_text_sensor.h" "wifi_info/wifi_info_text_sensor.h" >}}
