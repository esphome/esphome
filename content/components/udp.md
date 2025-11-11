---
description: "Instructions for setting up a UDP component on ESPHome"
title: "UDP Component"
params:
  seo:
    description: Instructions for setting up a UDP component on ESPHome
    image: udp.svg
---

{{< anchor "udp" >}}

This component allows reception and transmission of data over a network using the [User Datagram Protocol (UDP)](https://en.wikipedia.org/wiki/User_Datagram_Protocol).
In conjunction with the [Packet Transport Component](/components/packet_transport#packet-transport) it can be used to broadcast sensor data.

```yaml
# Example configuration entry
udp:
    listen_address: 239.0.60.53
    addresses: ["255.255.255.255", "208.87.135.110"]
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **port** (*Optional*, int): The destination UDP port number to use. Defaults to `18511`. Different listen and broadcast ports can be specified via a map instead of a single port number.:
  - **listen_port** (**Required**, int): The port to listen on for received packets.
  - **broadcast_port** (**Required**, int): The port to send packets to.
- **addresses** (*Optional*, list of IPv4 addresses): One or more IP addresses to broadcast data to. Defaults to `255.255.255.255`
  which is the local network broadcast address.

- **listen_address** (*Optional*, IPv4 address): Changes to multicast, adding an address to listen to. Defaults to no multicast address, just
  local network broadcast address `255.255.255.255`. **NOTE**: Adding a multicast address stops it from listening on the broadcast address.

## Reliability

UDP, like any other network protocol, does not provide a guarantee that data will be delivered, but unlike TCP it does not
even provide any indication whether data has been successfully delivered or not.

## `udp.write` Action

To write data to the UDP port, use the `udp.write` action. This action takes a single argument, the data to write to the UDP port.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The id of the UDP component to use. If there is only one UDP component, this can be omitted.
- **data** (**Required**, [templatable](/automations/templates), string or list of bytes): The data to write to the UDP port.

## On Receive Trigger

To trigger an action when data is received on the UDP port, use the `on_receive` trigger. The trigger is called with a single argument `data` representing a `std::vector<uint8_t>` of the received data.

```yaml
udp:
    on_receive:
      then:
        - logger.log:
            format: "Received %s"
            args: [format_hex_pretty(data).c_str()]
```

## Examples

See the [Packet Transport Component](/components/packet_transport#packet-transport) for examples of how to use this component.

A more complex example is shown below:

The example below shows a provider device separating data sent to different consumers. There are two provider confgurations, with different IDs.
The `transport_internal` provider broadcasts the selected sensor states in plain text every 10 seconds to all the network members, while the `transport_external`
provider sends other sensors data to an external IP address and port, with encryption. The node also listens to data from a `remote-node` through
the port specified in the `transport_external` configuration:

```yaml
udp:
 - id: udp_internal
 - id: udp_external
    port:
      listen_port: 18511
      broadcast_port: 18512
    addresses:
      - 10.87.135.110

packet_transport:
  - id: transport_internal
    udp_id: udp_internal
    update_interval: 10s
    sensors:
      - temp_outdoor
      - temp_rooma
      - temp_roomb
      - temp_roomc
      - temp_garage
      - temp_water
      - humi_rooma
      - humi_roomb
      - humi_roomc

  - id: transport_external
    udp_id: udp_external
    update_interval: 60s
    encryption: "Muddy Waters"
    ping_pong_enable: true
    rolling_code_enable: true
    binary_sensors:
      - binary_sensor_door
    sensors:
      - temp_outdoor

binary_sensor:
  - platform: packet_transport
    id: binary_sensor_unlock
    transport_id: transport_external
    provider: remote-node
    remote_id: binary_sensor_unlock_me
    on_press:
      - lambda: |-
          ESP_LOGI("main", "d command to binary_sensor_unlock");
```

## See Also

- {{< docref "/components/packet_transport/udp" >}}
- {{< docref "/components/binary_sensor/packet_transport" >}}
- {{< docref "/components/sensor/packet_transport" >}}
- [Automation](/automations)
- {{< apiref "udp/udp_component.h" "udp/udp_component.h" >}}
