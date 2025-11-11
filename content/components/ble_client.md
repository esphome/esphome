---
description: "Configuration of the BLE client on ESP32."
title: "BLE Client"
params:
  seo:
    description: Configuration of the BLE client on ESP32.
    image: bluetooth.svg
---

The `ble_client` component enables connections to Bluetooth Low Energy devices in order to query and
control them. This component does not expose any sensors or output components itself, but merely manages
connections to them for use by other components.

> [!WARNING]
> The BLE software stack on the ESP32 consumes a significant amount of RAM on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. Memory-intensive components such as {{< docref "/components/voice_assistant" >}} and other
> audio components are most likely to cause issues.

> [!NOTE]
> A maximum of three devices is supported due to limitations in the ESP32 BLE stack. If you wish to
> connect more devices, use additional ESP32 boards.
>
> This component supports devices that require a 6 digit PIN code for authentication.
>
> Currently, devices connected with the client cannot be supported by other components based on
> {{< docref "/components/esp32_ble_tracker" >}} as they listen to advertisements which are only sent by devices
> without an active connection.

Despite the last point above, the `ble_client` component requires the `esp32_ble_tracker` component in order
to discover available client devices.

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: itag_black
    auto_connect: true
```

## Configuration variables

- **mac_address** (**Required**, MAC Address): The MAC address of the BLE device to connect to.
- **auto_connect** (*Optional*, boolean): If true the device will be automatically connected when found by the {{< docref "/components/esp32_ble_tracker" >}}. Defaults to true.
- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID to use for code generation, and for reference by dependent components.

Automations:

- **on_connect** (*Optional*, [Automation](/automations)): An automation to perform
  when the client connects to a device. See [`on_connect`](#ble_client-on_connect).

- **on_disconnect** (*Optional*, [Automation](/automations)): An automation to perform
  when the client disconnects from a device. See [`on_disconnect`](#ble_client-on_disconnect).

- **on_passkey_request** (*Optional*, [Automation](/automations)): An automation to enter
  the passkey required by the other BLE device. See [`on_passkey_request`](#ble_client-on_passkey_request).

- **on_passkey_notification** (*Optional*, [Automation](/automations)): An automation to
  display the passkey to the user. See [`on_passkey_notification`](#ble_client-on_passkey_notification).

- **on_numeric_comparison_request** (*Optional*, [Automation](/automations)): An automation to
  compare the passkeys shown on the two BLE devices. See [`on_numeric_comparison_request`](#ble_client-on_numeric_comparison_request).

## BLE Client Automation

{{< anchor "ble_client-on_connect" >}}

### `on_connect`

This automation is triggered when the client connects to the BLE device.

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: ble_itag
    on_connect:
      then:
        - lambda: |-
            ESP_LOGD("ble_client_lambda", "Connected to BLE device");
```

{{< anchor "ble_client-on_disconnect" >}}

### `on_disconnect`

This automation is triggered when the client disconnects from a BLE device.

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: ble_itag
    on_disconnect:
      then:
        - lambda: |-
            ESP_LOGD("ble_client_lambda", "Disconnected from BLE device");
```

{{< anchor "ble_client-on_passkey_request" >}}

### `on_passkey_request`

This automation is triggered when the BLE device requests a passkey for authentication.

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: ble_itag
    on_passkey_request:
      then:
        - ble_client.passkey_reply:
            id: ble_itag
            passkey: 123456
```

{{< anchor "ble_client-on_passkey_notification" >}}

### `on_passkey_notification`

This automation is triggered when a passkey is received from the BLE device.

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: ble_itag
    on_passkey_notification:
      then:
        - logger.log:
            format: "Enter this passkey on your BLE device: %06d"
            args: [ passkey ]
```

{{< anchor "ble_client-on_numeric_comparison_request" >}}

### `on_numeric_comparison_request`

This automation is triggered when a numeric comparison is requested by the BLE device.

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: ble_itag
    on_numeric_comparison_request:
      then:
        - logger.log:
            format: "Compare this passkey with the one on your BLE device: %06d"
            args: [ passkey ]
        - ble_client.numeric_comparison_reply:
            id: ble_itag
            accept: True
```

{{< anchor "ble_client-connect_action" >}}

## `ble_client.connect` Action

This action is useful only for devices with `auto_connect: false` and allows a connection to be made from
within an automation. Once connected other actions like `ble_write` can be used. This is useful where
a BLE server needs only to be interacted with occasionally, and thus does not need a constant
connection held.

The following example updates the time of a Xiaomi MHO-C303 clock once per hour. Note that the BLE tracker must
be stopped during the connect attempt, and restarted afterwards. This would not be necessary if the tracker had
`continuous: false` set. In this example scenario there is another BLE device that does require the scanner to be
on, hence the stop and start of the scan during connect.

```yaml
ble_client:
  - id: ble_clock
    mac_address: XX:XX:XX:XX:XX:XX
    auto_connect: false
  - id: other_device
    mac_address: XX:XX:XX:XX:XX:XX

interval:
  - interval: 60min
    then:
      - esp32_ble_tracker.stop_scan:
      - ble_client.connect: ble_clock
      - ble_client.ble_write:
          id: ble_clock
          service_uuid: EBE0CCB0-7A0A-4B0C-8A1A-6FF2997DA3A6
          characteristic_uuid: EBE0CCB7-7A0A-4B0C-8A1A-6FF2997DA3A6
          value: !lambda |-
              uint32_t t = id(sntp_time).now().timestamp + ESPTime::timezone_offset();
              return {(uint8_t)t, (uint8_t)(t >> 8), (uint8_t)(t >> 16), (uint8_t)(t >> 24), 0};
      - ble_client.disconnect: ble_clock
      - esp32_ble_tracker.start_scan:
```

Any actions after the `connect` action will proceed only after the connect succeeds. If the connect
fails the subsequent actions in the automation block will *not* be executed. This should be considered
if scanning has been stopped - another mechanism may be required to restart it.

{{< anchor "ble_client-disconnect_action" >}}

## `ble_client.disconnect` Action

This action disconnects a device that was connected with the `ble_client.connect` action.
Execution of the automation block sequence resumes after the disconnect has completed.

{{< anchor "ble_client-ble_write_action" >}}

## `ble_client.ble_write` Action

This action triggers a write to a specified BLE characteristic. The write is attempted in
a best-effort fashion and will only succeed if the `ble_client`  's connection has been
established and the peripheral exposes the expected BLE service and characteristic.
Execution of the automation block sequence resumes after the write has completed. A write failure will *not*
stop execution of succeeding actions (this allows a disconnect to be executed, for example.)

Example usage:

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: my_ble_client

switch:
  - platform: template
    name: "My Switch"
    turn_on_action:
      - ble_client.ble_write:
          id: my_ble_client
          service_uuid: F61E3BE9-2826-A81B-970A-4D4DECFABBAE
          characteristic_uuid: 6490FAFE-0734-732C-8705-91B653A081FC
          # List of bytes to write.
          value: [0x01, 0xab, 0xff]
      - ble_client.ble_write:
          id: my_ble_client
          service_uuid: F61E3BE9-2826-A81B-970A-4D4DECFABBAE
          characteristic_uuid: 6490FAFE-0734-732C-8705-91B653A081FC
          # A lambda returning an std::vector<uint8_t>.
          value: !lambda |-
              return {0x13, 0x37};
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): ID of the associated BLE client.
- **service_uuid** (**Required**, UUID): UUID of the service to write to.
- **characteristic_uuid** (**Required**, UUID): UUID of the service's characteristic to write to.
- **value** (**Required**, Array of bytes or [lambda](/automations/templates#config-lambda)): The value to be written.

{{< anchor "ble_client-passkey_reply_action" >}}

## `ble_client.passkey_reply` Action

This action triggers an authentication attempt using the specified `passkey`.

Example usage:

```yaml
on_...:
  then:
    - ble_client.passkey_reply:
        id: my_ble_client
        passkey: 123456
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): ID of the associated BLE client.
- **passkey** (**Required**, int): The 6-digit passkey.

{{< anchor "ble_client-numeric_comparison_reply_action" >}}

## `ble_client.numeric_comparison_reply` Action

This action triggers an authentication attempt after a numeric comparison.

Example usage:

```yaml
on_...:
  then:
    - ble_client.numeric_comparison_reply:
        id: my_ble_client
        accept: True
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): ID of the associated BLE client.
- **accept** (**Required**, boolean): Should be `true` if the passkeys
  displayed on both BLE devices are matching.

{{< anchor "ble_client-remove_bond_action" >}}

## `ble_client.remove_bond` Action

This action removes a device from the security database and manages
unpairing.

Example usage:

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: my_ble_client
    on_connect:
      then:
        - ble_client.remove_bond:
            id: my_ble_client
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): ID of the associated BLE client.

## BLE Overview

This section gives a brief overview of the Bluetooth LE architecture
to help with understanding this and the related components. There are
plenty of more detailed references online.

BLE uses the concept of a *server* and a *client*. In simple terms,
the server is implemented on the device providing services, usually
these are the devices such as heart monitors, tags, weather stations,
etc. The client connects to the server and makes use of its services.
The client will often be an app on a phone, or in the case of ESPHome,
it's the ESP32 device.

When a client connects to a server, the client queries for *services*
provided by the server. Services expose categories of functionality
on the server. These might be well defined and supported services,
such as the Battery Level service, Device Information or Heart Rate.
Or they might be custom services designed just for that device. For
example the button on cheap iTags uses a custom service.

Each service then defines one or more *characteristics* which are
typically the discrete values of that service. For example for the
Environmental Sensor service characteristics exposed include the
Wind Speed, Humidity and Rainfall. Each of these may be read-only
or read-write, depending on their functionality.

A characteristic may also expose one or more *descriptors*, which carry
further information about the characteristic. This could be things
like the units, the valid ranges, and whether notifications (see below)
are enabled.

BLE also supports *notifications*. A client continuously polling for
updates could consume a lot of power, which is undesirable for a
protocol that's designed to be low energy. Instead, a server can push
updates to the client only when they change. Depending on their purpose
and design, a characteristic may allow for notifications to be sent. The
client can then enable notifications by setting the configuration
descriptor for the characteristic.

Each service, characteristic, and descriptor is identified by a
unique identifier (UUID) that may be between 16 and 128 bits long.
A client will typically identify a device's capabilities based on
the UUIDs.

Once the connection is established, referencing each
service/characteristic/descriptor by the full UUID would take a
considerable portion of the small (~23 byte) packet. So the
characteristics and descriptors also provide a small 2-byte
*handle* (alias) to maximize available data space.

## Setting Up Devices

Whilst the component can connect to most BLE devices, useful functionality
is only obtained through dependent components, such as {{< docref "/components/sensor/ble_client" >}}.
See the documentation for these components for details on setting up
specific devices.

In order to use the `ble_client` component, you need to enable the
{{< docref "/components/esp32_ble_tracker" >}} component. This will also allow you to discover
the MAC address of the device.

When you have discovered the MAC address of the device, you can add it
to the `ble_client` stanza.

If you then build and upload this configuration, the ESP will listen for
the device and attempt to connect to it when it is discovered. The component
will then query the device for all available services and characteristics and
display them in the log:

```text
[18:24:56][D][ble_client:043]: Found device at MAC address [XX:XX:XX:XX:XX:XX]
[18:24:56][I][ble_client:072]: Attempting BLE connection to XX:XX:XX:XX:XX:XX
[18:24:56][I][ble_client:097]: [XX:XX:XX:XX:XX:XX] ESP_GATTC_OPEN_EVT
[18:24:57][I][ble_client:143]: Service UUID: 0x1800
[18:24:57][I][ble_client:144]:   start_handle: 0x1  end_handle: 0x5
[18:24:57][I][ble_client:305]:  characteristic 0x2A00, handle 0x3, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A01, handle 0x5, properties 0x2
[18:24:57][I][ble_client:143]: Service UUID: 0x1801
[18:24:57][I][ble_client:144]:   start_handle: 0x6  end_handle: 0x6
[18:24:57][I][ble_client:143]: Service UUID: 0x180A
[18:24:57][I][ble_client:144]:   start_handle: 0x7  end_handle: 0x19
[18:24:57][I][ble_client:305]:  characteristic 0x2A29, handle 0x9, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A24, handle 0xb, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A25, handle 0xd, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A27, handle 0xf, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A26, handle 0x11, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A28, handle 0x13, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A23, handle 0x15, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A2A, handle 0x17, properties 0x2
[18:24:57][I][ble_client:305]:  characteristic 0x2A50, handle 0x19, properties 0x2
[18:24:57][I][ble_client:143]: Service UUID: F000FFC0045140-00B0-0000-0000-000000
[18:24:57][I][ble_client:144]:   start_handle: 0x1a  end_handle: 0x22
[18:24:57][I][ble_client:305]:  characteristic F000FFC1045140-00B0-0000-0000-000000, handle 0x1c, properties 0x1c
[18:24:57][I][ble_client:343]:    descriptor 0x2902, handle 0x1d
[18:24:57][I][ble_client:343]:    descriptor 0x2901, handle 0x1e
[18:24:57][I][ble_client:305]:  characteristic F000FFC2045140-00B0-0000-0000-000000, handle 0x20, properties 0x1c
[18:24:57][I][ble_client:343]:    descriptor 0x2902, handle 0x21
[18:24:57][I][ble_client:343]:    descriptor 0x2901, handle 0x22
[18:24:57][I][ble_client:143]: Service UUID: 0xFFE0
[18:24:57][I][ble_client:144]:   start_handle: 0x23  end_handle: 0x26
[18:24:57][I][ble_client:305]:  characteristic 0xFFE1, handle 0x25, properties 0x10
[18:24:57][I][ble_client:343]:    descriptor 0x2902, handle 0x26
[18:24:57][I][ble_client:143]: Service UUID: 0x1802
[18:24:57][I][ble_client:144]:   start_handle: 0x27  end_handle: 0x29
[18:24:57][I][ble_client:305]:  characteristic 0x2A06, handle 0x29, properties 0x4
```

The discovered services can then be used to enable and configure other
ESPHome components, for example Service UUID 0xFFE0 is used for iTag style
keychain button events, used by the {{< docref "/components/sensor/ble_client" >}} component.

## Passkey examples

Secure connection with a fixed passkey:

```yaml
esp32_ble:
  io_capability: keyboard_only

esp32_ble_tracker:

ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: pvvx_ble_display
    on_passkey_request:
      then:
        - logger.log: "Authenticating with passkey"
        - ble_client.passkey_reply:
            id: pvvx_ble_display
            passkey: 123456
```

Secure connection with a dynamically generated passkey:

```yaml
api:
  actions:
    - action: passkey_reply
      variables:
        passkey: int
      then:
        - logger.log: "Authenticating with passkey"
        - ble_client.passkey_reply:
            id: my_ble_client
            passkey: !lambda return passkey;
    - action: numeric_comparison_reply
      variables:
        accept: bool
      then:
        - logger.log: "Authenticating with numeric comparison"
        - ble_client.numeric_comparison_reply:
            id: my_ble_client
            accept: !lambda return accept;

esp32_ble:
  io_capability: keyboard_display

esp32_ble_tracker:

ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: my_ble_client
    on_passkey_request:
      then:
        - logger.log: "Enter the passkey displayed on your BLE device"
        - logger.log: " Go to https://my.home-assistant.io/redirect/developer_services/ and select passkey_reply"
    on_passkey_notification:
      then:
        - logger.log:
            format: "Enter this passkey on your BLE device: %06d"
            args: [ passkey ]
    on_numeric_comparison_request:
      then:
        - logger.log:
            format: "Compare this passkey with the one on your BLE device: %06d"
            args: [ passkey ]
        - logger.log: " Go to https://my.home-assistant.io/redirect/developer_services/ and select numeric_comparison_reply"
    on_connect:
      then:
        - logger.log: "Connected"
```

## See Also

- {{< docref "/components/sensor/ble_client" >}}
- [Automation](/automations)
- {{< apiref "ble_client/ble_client.h" "ble_client/ble_client.h" >}}
