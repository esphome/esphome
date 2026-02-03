---
description: "Instructions for setting up key collector text sensors."
title: "Key Collector Text Sensor"
params:
  seo:
    description: Instructions for setting up key collector text sensors.
---

The `key_collector` text sensor platform publishes the collected key sequence
when the {{< docref "/components/key_collector" "Key Collector" >}} component
successfully completes a key collection (either by reaching `max_length` or
when an `end_key` is pressed).

This is useful for displaying the entered PIN code or passkey on a display,
sending it to Home Assistant, or using it in other automations without having
to write lambda code.

```yaml
# Example configuration entry
key_collector:
  - id: pincode_reader
    source_id: mykeypad
    min_length: 4
    max_length: 4
    end_keys: "#"
    allowed_keys: "0123456789"

text_sensor:
  - platform: key_collector
    name: "Entered PIN Code"
    source_id: pincode_reader
```

## Configuration variables

- **source_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the
  key collector component to monitor.
- All other options from [Text Sensor](/components/text_sensor#config-text_sensor).

## See Also

- {{< docref "/components/key_collector" >}}
- {{< docref "/components/text_sensor" >}}
- {{< apiref "key_collector/text_sensor/key_collector_text_sensor.h" "key_collector/text_sensor/key_collector_text_sensor.h" >}}
