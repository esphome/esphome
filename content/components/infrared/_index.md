---
description: "Instructions for setting up infrared components in ESPHome."
title: "Infrared Component"
params:
  seo:
    description: Instructions for setting up infrared components in ESPHome.
    image: folder-open.svg
---

> [!IMPORTANT]
> This component is EXPERIMENTAL. The API may change at any time
> without following the normal breaking changes policy. Use at your own risk.
> Once the API is considered stable, this warning will be removed.

ESPHome has support for components to create infrared entities that provide a standardized API for
transmitting and receiving raw infrared (and RF) signals. An infrared entity is represented in ESPHome
as a stateless component (similar to buttons) that can transmit raw timing sequences or receive and
forward timing sequences as events to API clients like Home Assistant.

The infrared platform provides base infrastructure for IR/RF communication, establishing a unified
interface between ESPHome devices and API clients. This enables runtime signal transmission without
recompiling firmware, making it ideal for learning and replaying IR/RF commands.

{{< anchor "config-infrared" >}}

## Base Infrared Configuration

All infrared components in ESPHome have a platform and a name. The component operates as a stateless
entity, supporting actions (API commands to device transmissions) and events (device receptions to
API client broadcasts).

```yaml
# Example infrared configuration
infrared:
  - platform: ...
    name: Living Room IR Transmitter
    id: my_ir_transmitter
```

Configuration variables:

- **id** (*Optional*, string): Manually specify the ID for code generation. At least one of **id** and **name** must be specified.
- **name** (*Optional*, string): The name for the infrared entity. At least one of **id** and **name** must be specified.

> [!NOTE]
> If you have a [friendly_name](/components/esphome#esphome-configuration_variables) set for your device and
> you want the infrared entity to use that name, you can set `name: None`.

- **icon** (*Optional*, icon): Manually set the icon to use for the infrared entity in the frontend.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options. Set to `""` to remove the default entity category.

## How It Works

The infrared component operates using raw timing sequences, which represent alternating mark (signal on)
and space (signal off) periods in microseconds. This provides maximum flexibility and can support virtually
any IR or RF protocol.

### Transmitting Signals

When an infrared entity supports transmit capability, it can send raw timing sequences through the API.
Transmission parameters include:

- **Raw timings array**: Alternating mark/space durations in microseconds
- **Carrier frequency**: Optional carrier frequency in Hz (0 = no carrier modulation, typical for RF)
- **Repeat count**: Number of times to transmit the signal (defaults to 1)

The raw timings format allows API clients like Home Assistant to encode protocol-specific commands into
raw timings and transmit them through the infrared entity.

### Receiving Signals

When an infrared entity supports receive capability, it captures raw timing sequences and sends them
to API clients as events. This enables:

- Learning IR/RF commands from existing remotes
- Analyzing unknown protocols
- Creating universal remote controls

Reception is non-blocking and can operate alongside other signal processing components.

## Platform Components

- [IR/RF Proxy](/components/ir_rf_proxy) - Bridges ESPHome's remote_transmitter/remote_receiver components
  with the infrared API for runtime signal control

## See Also

- [Remote Transmitter](/components/remote_transmitter)
- [Remote Receiver](/components/remote_receiver)
- {{< apiref "infrared/infrared.h" "infrared/infrared.h" >}}
