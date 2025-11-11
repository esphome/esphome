---
description: "Instructions for setting up a packet transport sensor."
title: "Packet Transport Sensor"
params:
  seo:
    description: Instructions for setting up a packet transport sensor.
    image: packet_transport.svg
---

The `packet_transport` sensor platform allows you to receive numeric sensor data directly from another ESPHome node.
It requires a `packet_transport` component to be configured.

```yaml
# Example configuration entry
sensor:
  - platform: packet_transport
    id: temperature_id
    provider: thermometer
    remote_id: temp_id

 packet_transport:
   - platform: ...
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **provider** (**Required**, string): The name of the provider node.
- **remote_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of the original sensor in the provider node. If not specified defaults to the ID configured with `id:`.
- **name** (*Optional*, string): The name of the sensor.
- **internal** (*Optional*, boolean): Whether the sensor should be exposed via API (e.g. to Home Assistant.) Defaults to `true` if name is not set, required if name is provided.
- All other options from [Sensor](/components/sensor).

At least one of `id` and `remote_id` must be configured.

## Publishing to Home Assistant

Typically this type of sensor would be used for internal automation purposes rather than having it published back to
Home Assistant, since it would be a duplicate of the original sensor.

If it *is* desired to expose the sensor to Home Assistant, then the `internal:` configuration setting needs to be explicitly
set to `false` and a name provided.
Only the state (i.e. numeric value) of the remote sensor is received by the consumer, so any other attributes must be explicitly
configured.

## See Also

- {{< docref "/components/packet_transport" >}}
- {{< docref "/components/binary_sensor" >}}
- {{< docref "/components/udp" >}}
- [Automation](/automations)
