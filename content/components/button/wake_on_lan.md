---
description: "Instructions for setting up buttons that can send wakeup packets to computers on the network."
title: "Wake-on-LAN Button"
params:
  seo:
    description: Instructions for setting up buttons that can send wakeup packets to computers on the network.
    image: radio-tower.svg
---

The `wake_on_lan` button platform allows you to send a Wake-on-LAN magic packet to a computer on the network
by specifying its MAC address.

```yaml
# Example configuration entry
button:
  - platform: wake_on_lan
    name: "Start the Server"
    target_mac_address: XX:XX:XX:XX:XX:XX
```

## Configuration variables

- **target_mac_address** (**Required**, MAC Address): The MAC Address of the target computer.
- All other options from [Button](/components/button#config-button).

## See Also

- {{< docref "template/" >}}
- {{< apiref "wake_on_lan/wake_on_lan.h" "wake_on_lan/wake_on_lan.h" >}}
