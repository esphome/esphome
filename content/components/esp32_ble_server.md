---
description: "Instructions for setting up Bluetooth LE GATT Server in ESPHome."
title: "BLE Server"
params:
  seo:
    description: Instructions for setting up Bluetooth LE GATT Server in ESPHome.
    image: bluetooth.svg
---

The `esp32_ble_server` component in ESPHome sets up a BLE GATT server that exposes the device name,
manufacturer and board. BLE GATT services and characteristics can be added to the server to expose data and control.

> [!WARNING]
> The BLE software stack on the ESP32 consumes a significant amount of RAM on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. Memory-intensive components such as {{< docref "/components/voice_assistant" >}} and other
> audio components are most likely to cause issues.

```yaml
# Example configuration

esp32_ble_server:
  manufacturer: "Orange"
  manufacturer_data: [0x4C, 0, 0x23, 77, 0xF0 ]
  on_connect:
    - lambda: |-
        ESP_LOGD("BLE", "Connection from %d", id);
  on_disconnect:
    - lambda: |-
        ESP_LOGD("BLE", "Disconnection from %d", id);
```

## Configuration variables

- **manufacturer** (*Optional*, [Value Configuration](#esp32_ble_server-value)): The name of the manufacturer/firmware creator. Defaults to `ESPHome`.
- **model** (*Optional*, [Value Configuration](#esp32_ble_server-value)): The model name of the device. Defaults to the project's name defined in the [core configuration](/components/esphome#esphome-creators_project) if present, otherwise to the friendly name of the `board` chosen in the [core configuration](/components/esphome#esphome-configuration_variables).
- **appearance** (*Optional*, int): Sets the [appearance](https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/core/appearance_values.yaml) of the device (included in advertising data.) Defaults to `0`.
- **firmware_version** (*Optional*, [Value Configuration](#esp32_ble_server-value)): The firmware version of the device. Defaults to the project's version defined in the [core configuration](/components/esphome#esphome-creators_project) if present, otherwise to the ESPHome version.
- **manufacturer_data** (*Optional*, list of bytes): The manufacturer-specific data to include in the advertising
  packet. Should be a list of bytes, where the first two are the little-endian representation of the 16-bit
  manufacturer ID as assigned by the Bluetooth SIG.

- **on_connect** (*Optional*, [Automation](/automations)): An action to be performed when a client connects to the BLE server. It provides the `id` variable which contains the ID of the client that connected.
- **on_disconnect** (*Optional*, [Automation](/automations)): An action to be performed when a client disconnects from the BLE server. It provides the `id` variable which contains the ID of the client that disconnected.
- **services** (*Optional*, list of [Service Configuration](#esp32_ble_server-service)): A list of services to expose on the BLE GATT server.

{{< anchor "esp32_ble_server-service" >}}

## Service Configuration

Services are the main way to expose data and control over BLE. Services communicate with the client through characteristics. Each service can have multiple characteristics.

```yaml
esp32_ble_server:
  services:
    - uuid: 2a24b789-7aab-4535-af3e-ee76a35cc42d
      advertise: false
      characteristics:
        - uuid: cad48e28-7fbe-41cf-bae9-d77a6c233423
          read: true
          value:
            value: "Hello, World!"
```

### Configuration variables

- **uuid** (**Required**, string, int): The UUID of the service.
- **advertise** (*Optional*, boolean): If the service should be advertised. Defaults to `false`.
- **characteristics** (*Optional*, list of [Characteristic Configuration](#esp32_ble_server-characteristic)): A list of characteristics to expose in this service.

{{< anchor "esp32_ble_server-characteristic" >}}

## Characteristic Configuration

Characteristics expose data and control for a BLE service. Each characteristic has a value that may be readable and or writable, and may permit a client to subscribe to notifications.
Characteristics can also have multiple descriptors to provide additional information about the characteristic.

```yaml
esp32_ble_server:
  services:
    # ...
    advertise: true
    characteristics:
      - id: test_characteristic
        uuid: cad48e28-7fbe-41cf-bae9-d77a6c233423
        description: "Sample description"
        read: true
        value:
            data: "123.1"
            type: float
            endianness: BIG
        descriptors:
          - uuid: cad48e28-7fbe-41cf-bae9-d77a6c211423
            value: "Hello, World Descriptor!"
```

### Configuration variables

- **id** (*Optional*, string): An ID to refer to this characteristic in automations.
- **uuid** (**Required**, string, int): The UUID of the characteristic.
- **description** (*Optional*, [Value Configuration](#esp32_ble_server-value)): The description of the characteristic - not templatable. It will add a `CUD` descriptor (0x2901) to the characteristic with the value of the description.
- **read** (*Optional*, boolean): If the characteristic should be readable. Defaults to `false`.
- **write** (*Optional*, boolean): If the characteristic should be writable. Defaults to `false`.
- **broadcast** (*Optional*, boolean): If the characteristic should be broadcast. Defaults to `false`.
- **notify** (*Optional*, boolean): If the characteristic supports notifications. If `true`, a `CCCD` descriptor will be automatically added to the characteristic. Defaults to `false`.
- **indicate** (*Optional*, boolean): If the characteristic supports indications. If `true`, a `CCCD` descriptor will be automatically added to the characteristic. Defaults to `false`.
- **write_no_response** (*Optional*, boolean): If the characteristic should be writable without a response. Defaults to `false`.
- **value** (*Optional*, [Value Configuration](#esp32_ble_server-value)): The value of the characteristic.
- **descriptors** (*Optional*, list of [Descriptor Configuration](#esp32_ble_server-descriptor)): A list of descriptors to expose in this characteristic.
- **on_write** (*Optional*, [Automation](/automations)): An action to be performed when the characteristic is written to. The characteristic must have the `write` property. See [`on_write` Trigger](#esp32_ble_server-characteristic-on_write).

{{< anchor "esp32_ble_server-descriptor" >}}

## Descriptor Configuration

Descriptors are optional and are used to provide additional information (metadata) about a characteristic.

```yaml
esp32_ble_server:
  services:
    - uuid: # ...
      characteristics:
        - uuid: # ...
          descriptors:
            - uuid: 2901
              value:
                value: "Hello, World Descriptor!"
```

### Configuration variables

- **id** (*Optional*, string): An ID to refer to this descriptor in automations.
- **uuid** (**Required**, string, int): The UUID of the descriptor.
- **value** (**Required**, [Value Configuration](#esp32_ble_server-value)): The value of the descriptor. [templatable](/automations/templates) values are not allowed. In order to set the value of a descriptor dynamically, use the [`ble_server.descriptor.set_value` Action](#esp32_ble_server-descriptor-set_value) action.

{{< anchor "esp32_ble_server-value" >}}

## Value Configuration

Values can be of different types and are used to define the value of a characteristic or descriptor.
The value of a characteristic is templatable. If the value is templated, the template will be evaluated each time the characteristic is read, or a notification is triggered. The value of a descriptor is not templatable as it is expected to be static.

```yaml
esp32_ble_server:
  services:
    - uuid: # ...
      characteristics:
        - uuid: # ...
          # Simple value (auto-detect type)
          value: "Hello, World!"
        - uuid: # ...
          # String value
          value:
            data: "Hello, World!"
            type: string
            string_encoding: utf-8
        - uuid: # ...
          # Integer value
          value:
            data: "123"
            type: uint16_t
            endianness: LITTLE
        - uuid: # ...
          # Array of bytes value
          value:
            data: [9, 9, 9]
        - uuid: # ...
          # Lambda value
          value:
            data: !lambda 'return std::vector<uint8_t>({9, 9, 9});'
        - uuid: # ...
          # Lambda value using ByteBuffer
          value:
            data: !lambda 'return bytebuffer::ByteBuffer::wrap(0.182).get_data();'
```

### Configuration variables

- **data** (**Required**, string, int, float, boolean, list of bytes, [templatable](/automations/templates)): The value of the characteristic or descriptor. For [templatable](/automations/templates) values, the lambda function must return a `std::vector<uint8_t>` (you may use the `bytebuffer::ByteBuffer` helper class to transform different data types into a byte array). The value is computed each time the characteristic is read.
- **type** (*Optional*, string): The C++ type of the value. The available values are `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `int8_t`, `int16_t`, `int32_t`, `int64_t`, `float`, `double` and `string`. It must be defined if the value is not [templatable](/automations/templates).
- **endianness** (*Optional*, string): The endianness of the value. Can be `BIG` or `LITTLE`. Defaults to `LITTLE`.
- **string_encoding** (*Optional*, string): The encoding of the string. Only applicable if the type is `string`. The conversion is done in Python before compilation, so the encoding must be a valid [Python encoding](https://docs.python.org/3/library/codecs.html#standard-encodings). Defaults to `utf-8`.

{{< anchor "esp32_ble_server-characteristic-on_write" >}}

## `on_write` Trigger

With this configuration option you can write complex automations that are triggered when a characteristic is written to. It provides the `x` variable which contains the new value of the characteristic as a `std::vector<uint8_t>` and the `id` variable which contains the ID of the client that wrote to the characteristic.

```yaml
esp32_ble_server:
  services:
    - uuid: # ...
      characteristics:
        # ...
        write: true
        on_write:
          then:
            - lambda: |-
                ESP_LOGD("BLE", "Descriptor received: %s from %d", std::string(x.begin(), x.end()).c_str(), id);
```

## `ble_server.characteristic.set_value` Action

This action sets the value of a characteristic. A characteristic may not have a set_value action if it also has a templated value in its configuration.

```yaml
on_...:
  then:
    - ble_server.characteristic.set_value:
        id: test_write_characteristic
        value: [0, 1, 2]
```

### Configuration variables

- **id** (**Required**, string): The ID of the characteristic to set the value of.
- **value** (**Required**, [Value Configuration](#esp32_ble_server-value)): The new value of the characteristic.

## `ble_server.characteristic.notify` Action

This action triggers a notification to the client. The value sent will be the current value of the characteristic, or the value from evaluation of the template, if present.

```yaml
on_...:
  then:
    - ble_server.characteristic.notify:
        id: test_notify_characteristic
```

### Configuration variables

- **id** (**Required**, string): The ID of the characteristic to notify the client about (must have the `notify` property).

{{< anchor "esp32_ble_server-descriptor-set_value" >}}

## `ble_server.descriptor.set_value` Action

This action sets the value of a descriptor.

```yaml
on_...:
  then:
    - ble_server.descriptor.set_value:
        id: test_write_descriptor
        value: [0, 1, 2]
```

### Configuration variables

- **id** (**Required**, string): The ID of the descriptor to set the value of.
- **value** (**Required**, [Value Configuration](#esp32_ble_server-value)): The new value of the descriptor.

## See Also

- {{< docref "esp32_ble/" >}}
- {{< docref "esp32_improv/" >}}
- {{< apiref "esp32_ble/ble.h" "esp32_ble/ble.h" >}}
