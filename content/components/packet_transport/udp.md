---
description: "Instructions for setting up a udp packet transport platform on ESPHome"
title: "UDP Packet Transport Platform"
params:
  seo:
    description: Instructions for setting up a udp packet transport platform on ESPHome
    image: udp.svg
---

{{< anchor "udp-packet-transport" >}}

The {{< docref "/components/packet_transport" >}} platform allows ESPHome nodes to directly communicate with each over a communication channel.
The UDP implementation of the platform uses UDP as a communication medium. See the {{< docref "/components/packet_transport" >}} and {{< docref "/components/udp" >}} for more information.

## Example Configuration

```yaml
# Example configuration entry
packet_transport:
  platform: udp
  sensors:
    - dht_temp

udp:

sensor:
  - platform: dht
      id: dht
      pin: GPIOXX
      temperature:
        name: "Temperature"
        id: dht_temp
```

## See Also

- {{< docref "/components/packet_transport" >}}
- {{< docref "/components/udp" >}}
- {{< docref "/components/binary_sensor/packet_transport" >}}
- {{< docref "/components/sensor/packet_transport" >}}
- {{< docref "/automations" >}}
- {{< apiref "packet_transport/packet_transport.h" "packet_transport/packet_transport.h" >}}
