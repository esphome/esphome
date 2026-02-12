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
Single endpoint requires ZHA or at least Zigbee2MQTT 2.8.0. For older versions of Zigbee2MQTT use multiple endpoints.
Spaces in names require ZHA or at least Zigbee2MQTT 2.8.0. For older versions of Zigbee2MQTT do not use spaces.
The maximum number of supported endpoints is eight.

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

- **wipe_on_boot** (*Optional*): Erases all non-volatile memory data on boot, including
  Zigbee network pairing and preferences (e.g., last switch state). One of:
  - `false` (default): Preserve data across reboots.
  - `true`: Erase all data on every boot. Use only for recovery from boot loops when
    you don't have an SWD programmer.
  - `once`: Erase data only on first boot after flashing new firmware, then preserve.

- **on_join** (*Optional*, [Automation](/automations#automation)): Automation to run when the device joins the network.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The ID to use for this `zigbee` component.

- **power_source** (*Optional*, enum): Indicates what kind of power the device uses. Affects
  sleep behavior. One of `UNKNOWN`, `MAINS_SINGLE_PHASE`, `MAINS_THREE_PHASE`, `BATTERY`,
  `DC_SOURCE`, `EMERGENCY_MAINS_CONST`, or `EMERGENCY_MAINS_TRANSF`. Defaults to `DC_SOURCE`.

- **ieee802154_vendor_oui** (*Optional*, int): Sets the Vendor Organizationally Unique Identifier (OUI).
  This allows replacing Nordic Semiconductor's default company ID with your own.
  The value must be a 24-bit integer in the range `0x000000` to `0xFFFFFF`.
  Alternatively, set to `random` to generate a new random OUI on every firmware compilation.
  This is useful during development to force the coordinator (ZHA/Z2M) to recognize the device
  as new after firmware updates.

> [!WARNING]
> Overusing `random` may exhaust memory in the Zigbee coordinator by creating many "ghost" devices.

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

### Sensor Configuration

All sensors with a `name` are automatically exposed over Zigbee.

```yaml
sensor:
  - platform: template
    name: "Analog 1"
    lambda: return 10.0;
    unit_of_measurement: "°C"
  - platform: template
    name: "Analog 2"
    lambda: return 11.0;
  - platform: template
    id: internal_sensor
    lambda: return 9.0;
  - platform: template
    name: "Another internal sensor"
    internal: true
    lambda: return 8.0;
```

#### Configuration variables

- **name** (**Required**, string): The name for the sensor. This is exposed as the
  Zigbee endpoint description.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed over Zigbee. Only specifying an `id` without a `name` will implicitly set this to true.
  Use this if you run out of Zigbee endpoints.
- **unit_of_measurement** (*Optional*, string): Manually set the unit. By default, values are unitless.
  Only a limited set of units is supported. Unsupported units will revert to unitless.
  This is exposed as the Zigbee endpoint engineering units.

### Switch Configuration

All switches with a `name` are automatically exposed over Zigbee.

```yaml
switch:
  - platform: template
    name: "Template Switch"
    optimistic: true
```

#### Configuration variables

- **name** (**Required**, string): The name for the switch. This is exposed as the
  Zigbee endpoint description.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed over Zigbee. Only specifying an `id` without a `name` will implicitly set this to true.
  Use this if you run out of Zigbee endpoints.

### Number Configuration

All numbers with a `name` are automatically exposed over Zigbee.

```yaml
number:
  - platform: template
    name: "Template Number"
    optimistic: true
    min_value: 2
    max_value: 100
    step: 1
```

#### Configuration variables

- **name** (**Required**, string): The name for the number. This is exposed as the
  Zigbee endpoint description.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed over Zigbee. Only specifying an `id` without a `name` will implicitly set this to true.
  Use this if you run out of Zigbee endpoints.
- **unit_of_measurement** (*Optional*, string): Manually set the unit. By default, values are unitless.
  Only a limited set of units is supported. Unsupported units will revert to unitless.
  This is exposed as the Zigbee endpoint engineering units.
- **min_value** (*Optional*, float): The minimum value this number can be. This is exposed as the
  Zigbee endpoint min present value.
- **max_value** (*Optional*, float): The maximum value this number can be. This is exposed as the
  Zigbee endpoint max present value.
- **step** (*Optional*, float): The granularity with which the number can be set. This is exposed
  as the Zigbee endpoint resolution.

## See Also

- [Zigbee2MQTT](https://www.zigbee2mqtt.io/)
- [Zigbee Home Automation](https://www.home-assistant.io/integrations/zha/)
