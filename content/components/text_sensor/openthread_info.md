---
description: "Instructions for setting up OpenThread info text sensors."
title: "OpenThread Info Text Sensor"
params:
  seo:
    description: Instructions for setting up OpenThread info text sensors.
    image: openthread.png
---

The `openthread_info` text sensor platform exposes different OpenThread network information
via text sensors.

```yaml
# Example configuration entry
text_sensor:
  - platform: openthread_info
    ip_address:
      name: "Thread IP Address"
    channel:
      name: "Thread Channel"
    role:
      name: "Thread Device Role"
    rloc16:
      name: "Thread RLOC16"
    ext_addr:
      name: "Thread Extended Address"
    eui64:
      name: "Thread EUI64"
    network_name:
      name: "Thread Network Name"
    network_key:
      name: "Thread Network Key"
    pan_id:
      name: "Thread PAN ID"
    ext_pan_id:
      name: "Thread Extended PAN ID"
```

## Configuration variables

- **ip_address** (*Optional*): Expose the off-mesh routable IPv6 address of the Thread device as a text sensor.
  This is the address used for communication outside the Thread mesh network.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **channel** (*Optional*): Expose the Thread network channel (11-26) as a text sensor.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **role** (*Optional*): Expose the current device role in the Thread network (Leader, Router, Child, Detached,
  etc.) as a text sensor. All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **rloc16** (*Optional*): Expose the Router Locator (RLOC16) address as a text sensor. This is a 16-bit address
  used for routing within the Thread network. All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **ext_addr** (*Optional*): Expose the IEEE 802.15.4 Extended MAC address as a text sensor.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **eui64** (*Optional*): Expose the EUI-64 address as a text sensor. This is the unique 64-bit identifier for
  the device. All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **network_name** (*Optional*): Expose the Thread network name as a text sensor.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **network_key** (*Optional*): Expose the Thread network key as a text sensor.
  All options from [Text Sensor](/components/text_sensor#config-text_sensor).

  > [!WARNING]
  > The `network_key` sensor exposes sensitive security credentials that could allow unauthorized access to your
  > Thread network. Only enable this sensor if you need it for debugging purposes and understand the security
  > implications.

- **pan_id** (*Optional*): Expose the Personal Area Network ID (PAN ID) as a text sensor. This is a 16-bit
  identifier for the Thread network. All options from [Text Sensor](/components/text_sensor#config-text_sensor).

- **ext_pan_id** (*Optional*): Expose the Extended PAN ID as a text sensor. This is a 64-bit extended identifier
  for the Thread network. All options from [Text Sensor](/components/text_sensor#config-text_sensor).

## See Also

- {{< docref "/components/openthread" >}}
- {{< docref "/components/text_sensor/index" >}}
- {{< apiref "openthread_info/openthread_info_text_sensor.h" "openthread_info/openthread_info_text_sensor.h" >}}
