---
description: "Instructions for setting up an CAN bus in ESPHome"
title: "CAN Bus"
params:
  seo:
    description: Instructions for setting up an CAN bus in ESPHome
    image: canbus.svg
---

The Controller Area Network (CAN) bus is a serial bus protocol to connect individual systems and sensors
as an alternative to conventional multi-wire looms. It allows automotive components to communicate on a
single or dual-wire data bus at speeds up to 1Mbps.

CAN is an International Standardization Organization (ISO) defined serial communications bus originally
developed for the automotive industry to replace the complex wiring harness with a two-wire bus. The
specification calls for high immunity to electrical interference and the ability to self-diagnose and repair
data errors. These features have led to CAN's popularity in a variety of industries including building
automation, medical, and manufacturing.

The current ESPHome implementation supports single frame data transfer. In this way you may send and
receive data frames up to 8 bytes.
With this you can transmit the press of a button or the feedback from a sensor on the bus.
All other devices on the bus will be able to get this data to switch on/off a light or display the
transmitted data.

The CAN bus itself has only two wires named Can High and Can Low or CanH and CanL. For the ESPHome
CAN bus to work, you need to select the device that has the physical CAN bus implemented.
You can configure multiple buses.

Any CAN bus node can transmit data at any time; any node can both send and/or receive any `can_id` value.
You must determine how to organize the `can_id` values; for example, you can set up a CAN bus network where
each node has a `can_id` it will use to broadcast data about itself. If a given node should (for example) turn
on a light, it can listen to the CAN bus for messages containing its specific `can_id` and react accodingly.
With this architecture, you can have multiple nodes able to control a light connected to a single, specific node.

## Base CAN Bus Configuration

Each `canbus` platform extends the following configuration schema:

```yaml
# Example configuration entry
canbus:
  - platform: ...
    can_id: 4
    on_frame:
    - can_id: 500
      use_extended_id: false
      then:
      - lambda: |-
          std::string b(x.begin(), x.end());
          ESP_LOGD("can id 500", "%s", &b[0] );
```

{{< anchor "config-canbus" >}}

**Configuration variables:**

- **platform** (**Required**, [platform](#platforms-canbus)): One of the supported CAN bus [Platforms](#platforms-canbus).
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **can_id** (**Required**, int): default *CAN ID* used for transmitting frames.
- **use_extended_id** (*Optional*, boolean): Identifies the type of `can_id`  :

  - `false`  : Standard 11-bit IDs *(default)*
  - `true`  : Extended 29-bit IDs

- **bit_rate** (*Optional*, enum): One of the supported bit rates. See [this table](/components/canbus/esp32_can#esp32-can-bit-rate) for a
  list of supported bit rates by the internal CAN (TWAI) controllers of different ESP32 variants. Defaults to `125KBPS`.

  - `1KBPS` - Support by `esp32_can` depends on ESP32 variant
  - `5KBPS` - Support by `esp32_can` depends on ESP32 variant
  - `10KBPS` - Support by `esp32_can` depends on ESP32 variant
  - `12K5BPS` - Support by `esp32_can` depends on ESP32 variant
  - `16KBPS` - Support by `esp32_can` depends on ESP32 variant
  - `20KBPS` - Support by `esp32_can` depends on ESP32 variant
  - `25KBPS`
  - `31K25BPS` - Not supported by `esp32_can`
  - `33KBPS` - Not supported by `esp32_can`
  - `40KBPS` - Not supported by `esp32_can`
  - `50KBPS`
  - `80KBPS` - Not supported by `esp32_can`
  - `83K3BPS` - Not supported by `esp32_can`
  - `95KBPS` - Not supported by `esp32_can`
  - `100KBPS`
  - `125KBPS` - *Default*
  - `200KBPS` - Not supported by `esp32_can`
  - `250KBPS`
  - `500KBPS`
  - `1000KBPS`

- **on_frame** (*Optional*, [Automation](/automations)): An automation to perform when a
  CAN frame is received. See [`on_frame` Trigger](#canbus-on-frame).

{{< anchor "platforms-canbus" >}}

## Platforms

## Automations

{{< anchor "canbus-on-frame" >}}

### `on_frame` Trigger

This automation will be triggered when a CAN frame is received. The variables `x` (of type
`std::vector<uint8_t>`  ) containing the frame data, `can_id` (of type `uint32_t`  ) containing the actual
received CAN ID and `remote_transmission_request` (of type `bool`  ) containing the corresponding field
from the CAN frame are passed to the automation for use in lambdas.

> [!NOTE]
> Messages this node sends to the same ID will not show up as received messages.

```yaml
canbus:
  - platform: ...
    on_frame:
    - can_id: 43  # the received can_id
      then:
        - if:
            condition:
              lambda: 'return (x.size() > 0) ? x[0] == 0x11 : false;'
            then:
              light.toggle: light1
    - can_id:      0b00000000000000000000001000000
      can_id_mask: 0b11111000000000011111111000000
      use_extended_id: true
      remote_transmission_request: false
      then:
        - lambda: |-
            auto pdo_id = can_id >> 14;
            switch (pdo_id)
            {
              case 117:
                ESP_LOGD("canbus", "exhaust_fan_duty");
                break;
              case 118:
                ESP_LOGD("canbus", "supply_fan_duty");
                break;
              case 119:
                ESP_LOGD("canbus", "supply_fan_flow");
                break;
              // to be continued...
            }
```

**Configuration variables:**

- **can_id** (**Required**, int): The CAN ID which, when received, will trigger this automation.
- **can_id_mask** (*Optional*, int): The bit mask to apply to the received CAN ID before trying to match it
  with *can_id*. Defaults to `0x1fffffff` (all bits of received CAN ID are compared with *can_id*).

- **use_extended_id** (*Optional*, boolean): Identifies the type of `can_id` to match on. Defaults to `false`.
- **remote_transmission_request** (*Optional*, boolean): Whether to run for CAN frames with the "remote
  transmission request" bit set or not set. Defaults to not checking (the automation will run for both cases).

### `canbus.send` Action

The CAN bus can transmit frames by means of the `canbus.send` action. There are several ways to use it:

```yaml
on_...:
  - canbus.send:
      data: [ 0x10, 0x20, 0x30 ]
      canbus_id: my_mcp2515 # optional if you only have 1 canbus device
      can_id: 23 # override the can_id configured in the can bus

on_...:
  - canbus.send: [ 0x11, 0x22, 0x33 ]

  - canbus.send: 'hello'

  # Templated; return type must be std::vector<uint8_t>
  - canbus.send: !lambda return {0x00, 0x20, 0x42};
```

**Configuration variables:**

- **data** (**Required**, binary data, [templatable](/automations/templates)): Data to transmit, up to eight
  bytes/characters are supported by CAN bus per frame.

- **canbus_id** (*Optional*): Sets the CAN bus ID to use for transmitting the frame. Required if you are have multiple
  CAN bus platforms defined in your configuration.

- **can_id** (*Optional*, int): Allows overriding the `can_id` configured for the CAN bus device.
- **use_extended_id** (*Optional*, boolean): Identifies the type of `can_id`  :

  - `false`  : Standard 11-bit IDs *(default)*
  - `true`  : Extended 29-bit IDs

- **remote_transmission_request** (*Optional*, boolean): Set to send CAN bus frame to request data from another node.
  If a certain data length code needs to be sent, include the necessary (dummy) bytes in `data`. Defaults to `false`.

## Extended ID

Standard IDs and Extended IDs can coexist on the same segment.

> [!NOTE]
> It is important to know that "standard" and "extended" addresses denote different addresses. For example,
> Standard `0x123` and Extended `0x123` are, in fact, different addresses.

Decimal or hexadecimal notation may be used for IDs:

- Standard IDs use `0x000` to `0x7ff` (hexadecimal) or `0` to `2047` (decimal)
- Extended IDs use `0x00000000` to `0x1fffffff` (hexadecimal) or `0` to `536870911` (decimal)

This example illustrates how different ID types may be used in your configuration for both transmitting and receiving.

```yaml
# Transmission of extended and standard ID 0x100 every second
time:
  - platform: sntp
    on_time:
      - seconds: /1
        then:
          - canbus.send:
              # Extended ID explicit
              use_extended_id: true
              can_id: 0x100
              data: [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]
          - canbus.send:
              # Standard ID by default
              can_id: 0x100
              data: [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]

canbus:
  - platform: ...
    can_id: 0x1fff
    use_extended_id: true
    bit_rate: 125kbps
    on_frame:
    - can_id: 0x123
      use_extended_id: true
      then:
        - lambda: |-
            std::string b(x.begin(), x.end());
            ESP_LOGD("CAN extended ID 0x123", "%s", &b[0]);
    - can_id: 0x123
      then:
        - lambda: |-
            std::string b(x.begin(), x.end());
            ESP_LOGD("CAN standard ID 0x123", "%s", &b[0]);
```

## Binary Sensor Example

Given that we have a button connected to a remote CAN node which will send a message to ID `0x100` with the payload
`0x1` for contact closed and `0x0` for contact open, this example will look for this message and update the state
of its `binary_sensor` accordingly.

```yaml
binary_sensor:
  - platform: template
    name: CAN Bus Button
    id: can_bus_button

canbus:
  - platform: ...
    can_id: 4
    bit_rate: 125kbps
    on_frame:
      - can_id: ${0x100}
        then:
          - lambda: |-
              if(x.size() > 0) {
                switch(x[0]) {
                  case 0x0:  // button release
                    id(can_bus_button).publish_state(false);
                    break;
                  case 0x1:  // button press
                    id(can_bus_button).publish_state(true);
                    break;
                }
              }
```

## Cover Example

In this example, three nodes are connected to the CAN bus:

- Node 1 sends a one-byte payload to ID `0x50B`
- Node 2 sends a one-byte payload to ID `0x50C`

  These nodes send the following one-byte payload which is based on the state of a button connected to each of them:

  - 0: Button release
  - 1: Button press
  - 2: Long press
  - 3: Long release
  - 4: Double-click

- Node 3 controls a motor connected to it. It expects a message to ID `0x51A` where the one-byte payload is:

  - 0: Off
  - 1: Open
  - 2: Close

```yaml
canbus:
  - platform: ...
    id: my_canbus
    can_id: 4
    bit_rate: 125kbps
    on_frame:
      - can_id: 0x50c
        then:
          - lambda: |-
              if(x.size() > 0) {
                auto call = id(TestCover).make_call();
                switch(x[0]) {
                  case 0x2: call.set_command_open(); call.perform(); break; // long press
                  case 0x1:                                                 // button press
                  case 0x3: call.set_command_stop(); call.perform(); break; // long release
                  case 0x4: call.set_position(1.0); call.perform(); break;  // double-click
                }
              }
      - can_id: 0x50b
        then:
          - lambda: |-
              if(x.size() > 0) {
                auto call = id(TestCover).make_call();
                switch(x[0]) {
                  case 0x2: call.set_command_close(); call.perform(); break; // long press
                  case 0x1:                                                  // button press
                  case 0x3: call.set_command_stop(); call.perform(); break;  // long release
                  case 0x4: call.set_position(0.0); call.perform(); break;   // double-click
                }
              }

cover:
  - platform: time_based
    name: Canbus Test Cover
    id: TestCover
    device_class: shutter
    has_built_in_endstop: true
    open_action:
      - canbus.send:
          data: [ 0x01 ]
          canbus_id: my_canbus
          can_id: 0x51A
    open_duration: 2min
    close_action:
      - canbus.send:
          data: [ 0x02 ]
          canbus_id: my_canbus
          can_id: 0x51A
    close_duration: 2min
    stop_action:
      - canbus.send:
          data: [ 0x00 ]
          canbus_id: my_canbus
          can_id: 0x51A
```

## See Also

- {{< apiref "canbus/canbus.h" "canbus/canbus.h" >}}
