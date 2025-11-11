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
      name: ESP IP Address
      address_0:
        name: ESP IP Address 0
      address_1:
        name: ESP IP Address 1
      address_2:
        name: ESP IP Address 2
      address_3:
        name: ESP IP Address 3
      address_4:
        name: ESP IP Address 4
    ssid:
      name: ESP Connected SSID
    bssid:
      name: ESP Connected BSSID
    mac_address:
      name: ESP Mac Wifi Address
    scan_results:
      name: ESP Latest Scan Results
    dns_address:
      name: ESP DNS Address
```

## Configuration variables

- **ip_address** (*Optional*): Expose the IP Address of the ESP as a text sensor.
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

- **dns_address** (*Optional*): Expose the DNS Address of the ESP as text sensor.
  [Text Sensor](/components/text_sensor#config-text_sensor).

## See Also

- {{< docref "/components/wifi" >}}
- {{< docref "/components/sensor/wifi_signal" >}}
- {{< apiref "wifi_info/wifi_info_text_sensor.h" "wifi_info/wifi_info_text_sensor.h" >}}
