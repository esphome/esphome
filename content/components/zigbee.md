---
description: "Zigbee End Device for Zigbee2MQTT and ZHA."
title: "Zigbee End Device"
params:
  seo:
    description: Zigbee End Device for Zigbee2MQTT and ZHA.
    image: zigbee.svg
---

The `zigbee` component allows exposing supported ESPHome components over a Zigbee network to Home Assistant
via **Zigbee2MQTT** or **ZHA**. Due to the limitations of the Zigbee protocol, only basic properties are exposed.
Additional properties must be configured manually in Home Assistant. Each ESPHome entity consumes one Zigbee endpoint.
Because of a limitation in Zigbee2MQTT, at least two endpoints are required. The maximum number of supported endpoints
is eight.

Zigbee support is currently available only on `nRF52` platforms.

## Full Configuration

```yaml
# Example configuration entry
zigbee:
  id: my_zigbee
  on_join:
    then:
      - logger.log: "Joined network"

binary_sensor:
  - platform: template
    name: "Door 1"
  - platform: template
    name: "Door 2"
```

## Configuration variables

- **wipe_on_boot** (*Optional*, boolean): Erases all non-volatile memory data on boot.
  Use only if the device is in a boot loop crash. Defaults to `false`.

- **on_join** (*Optional*, [Automation](/automations#automation)): Automation to run when the device joins the network.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID to use for this `zigbee` component.

## Actions

### `factory_reset` Action

This [action](/automations/actions#config-action) triggers a factory reset of the Zigbee device.
It handles leaving the Zigbee network.

```yaml
on_...:
  then:
    - zigbee.factory_reset
```

## Supported Components

### Binary Sensor Configuration

All binary sensors with a `name` are automatically exposed over Zigbee.

```yaml
binary_sensor:
  - platform: template
    name: "Door 1"
  - platform: template
    name: "Door 2"
  - platform: template
    id: internal_sensor
  - platform: template
    name: "Another internal sensor"
    internal: true
```

#### Configuration variables

- **name** (**Required**, string): The name for the binary sensor. This is exposed as the
  Zigbee endpoint description.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed over Zigbee. Only specifying an `id` without a `name` will implicitly set this to true.
  Use this if you run out of Zigbee endpoints.

## See Also

- [Zigbee2MQTT](https://www.zigbee2mqtt.io/)
- [Zigbee Home Automation](https://www.home-assistant.io/integrations/zha/)
