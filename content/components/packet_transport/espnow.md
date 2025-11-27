---
description: "Instructions for setting up an ESP-NOW packet transport platform on ESPHome"
title: "ESP-NOW Packet Transport Platform"
params:
  seo:
    description: Instructions for setting up an ESP-NOW packet transport platform on ESPHome
    image: espnow.svg
---

{{< anchor "espnow-packet-transport" >}}

The [Packet Transport Component](/components/packet_transport) platform allows ESPHome nodes to directly communicate with each
over a communication channel. The ESP-NOW implementation of the platform uses ESP-NOW as a communication medium.
See the [Packet Transport Component](/components/packet_transport) and {{< docref "/components/espnow" >}} for more information.

ESP-NOW provides low-latency, low-power wireless communication between ESP32 devices without requiring a Wi-Fi
connection. This makes it ideal for battery-powered sensors or applications where Wi-Fi overhead would impact
performance.

> **Note:**  
> ESP-NOW communication occurs independently of Wi-Fi. Devices can communicate via ESP-NOW even when Wi-Fi is
> disabled, making it suitable for power-sensitive applications.

## Example Configuration

```yaml
# Example configuration entry
espnow:
  id: espnow_component

packet_transport:
  - platform: espnow
    id: transport_unicast
    espnow_id: espnow_component
    peer_address: "AA:BB:CC:DD:EE:FF"
    encryption:
      key: "0123456789abcdef0123456789abcdef"
    sensors:
      - temp_sensor

sensor:
  - platform: internal_temperature
    id: temp_sensor
    name: "Test Temperature"
```

## Configuration Variables

- **espnow_id** (**Required**, [ID](/guides/configuration-types#config-id)): The esp-now ID to use for transport.
- **peer_address** (*Optional*, MAC Address): MAC address to send packets to. This can be either a specific
  peer address for point-to-point communication, or the broadcast address. Default FF:FF:FF:FF:FF:FF
- All other options from the [Packet Transport Component](/components/packet_transport)

> **Note:**  
> Peers must be registered with the {{< docref "/components/espnow" >}} component before
> they can receive packets. The `peer_address` only controls which peer(s) receive transmitted data;
> incoming packets are accepted from all registered peers.

## Broadcast vs Unicast

The `peer_address` configuration determines the transmission mode.

### Broadcast Mode (default)

```yaml
packet_transport:
  - platform: espnow
    sensors:
      - sensor_id
```

All devices with the broadcast address (`FF:FF:FF:FF:FF:FF`) registered as a peer will receive the packets.
This is useful for hub-and-spoke topologies where multiple devices monitor a single sensor source.

> **Warning:**  
> Using broadcast mode increases ESP-NOW traffic on the radio channel, which may impact performance of other
> ESP-NOW devices in range. Use specific peer addresses whenever possible to minimize interference.

### Unicast Mode

```yaml
packet_transport:
  - platform: espnow
    peer_address: "AA:BB:CC:DD:EE:FF"
    sensors:
      - sensor_id
```

Only the specified peer receives the packets. This is more efficient for point-to-point communication and reduces
radio channel congestion for neighboring ESP-NOW devices.

## Simple Example

This example shows two devices exchanging sensor data over ESP-NOW with encryption enabled.

### Temperature Provider

```yaml
espnow:
  peers:
    - "AA:BB:CC:DD:EE:01"  # Consumer mac address

packet_transport:
  - platform: espnow
    peer_address: "AA:BB:CC:DD:EE:01"  # Consumer mac address
    encryption: "MySecretKey123"
    sensors:
      - outdoor_temp

sensor:
  - platform: ...
    temperature:
      name: "Outdoor Temperature"
      id: outdoor_temp
```

### Temperature Consumer

```yaml
espnow:
  peers:
    - "AA:BB:CC:DD:EE:00"  # Provider mac address

packet_transport:
  - platform: espnow
    encryption: "MySecretKey123"
    providers:
      - name: temp-sensor  # Provider device name

sensor:
  - platform: packet_transport
    provider: temp-sensor
    id: remote_temp
    remote_id: outdoor_temp
    name: "Remote Outdoor Temperature"
```

## Multi-Device Hub Example

This example shows a central hub receiving sensor data from multiple remote devices.

### Hub Device

```yaml
espnow:
  peers:
    - "AA:BB:CC:DD:EE:01"  # room-sensor-1 mac address
    - "AA:BB:CC:DD:EE:02"  # room-sensor-2 mac address
    - "AA:BB:CC:DD:EE:03"  # outdoor-sensor mac address

packet_transport:
  - platform: espnow
    encryption: "HubSecret123"
    providers:
      - name: room-sensor-1
      - name: room-sensor-2
      - name: outdoor-sensor

sensor:
  - platform: packet_transport
    provider: room-sensor-1
    remote_id: temperature
    name: "Room 1 Temperature"

  - platform: packet_transport
    provider: room-sensor-2
    remote_id: temperature
    name: "Room 2 Temperature"

  - platform: packet_transport
    provider: outdoor-sensor
    remote_id: temperature
    name: "Outdoor Temperature"
```

### Remote Sensors

```yaml
espnow:

packet_transport:
  - platform: espnow
    encryption: "HubSecret123"
    sensors:
      - temperature

sensor:
  - platform: ...
    temperature:
      id: temperature
```

## See Also

- [Packet Transport Component](/components/packet_transport)
- {{< docref "/components/espnow" >}}
- {{< docref "/components/binary_sensor/packet_transport" >}}
- {{< docref "/components/sensor/packet_transport" >}}
- [UDP Packet Transport](/components/packet_transport/udp#udp-packet-transport)
- [Automation](/automations#automation)
- {{< apiref "packet_transport/espnow_transport.h" "packet_transport/espnow_transport.h" >}}
