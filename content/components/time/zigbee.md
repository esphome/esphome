---
description: "Zigbee Time Source"
title: "Zigbee Time Source"
---

The `zigbee` time platform synchronizes time from the Zigbee coordinator.
When the device joins the network, it requests the current time from the coordinator.

This component is only available on **nRF52** platforms.

You first need to set up the {{< docref "/components/zigbee" "Zigbee" >}} component.

```yaml
# Example configuration entry
time:
  - platform: zigbee
```

## Configuration variables

- **update_interval** (*Optional*, {{< docref "/guides/configuration-types" "Time" >}}):
  How often to update the time attribute. Defaults to `1s`.
- All other options from [Base Time Configuration](/components/time#base_time_config).

## See Also

- {{< docref "/components/zigbee" "Zigbee Component" >}}
- {{< docref "/components/time/" "Time Component" >}}
