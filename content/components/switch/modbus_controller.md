---
description: "Instructions for setting up a modbus_controller device sensor."
title: "Modbus Controller Switch"
params:
  seo:
    description: Instructions for setting up a modbus_controller device sensor.
---

The `modbus_controller` switch platform creates a switch from a modbus_controller component
and requires {{< docref "/components/modbus_controller" >}} to be configured.

## Configuration variables

- **register_type** (**Required**): type of the modbus register.

  - `coil`  : Coils are 1-bit registers (on/off values) that are used to control discrete outputs. They may be read and/or written. Modbus *Function Code 1 (Read Coil Status)* will be used.
  - `discrete_input`  : discrete input register (read only coil) are similar to coils but can only be read. Modbus *Function Code 2 (Read Input Status)* will be used.
  - `holding`  : Holding Registers - Holding registers are the most universal 16-bit register. They may be read and/or written. Modbus *Function Code 3 (Read Holding Registers)* will be used.
  - `read`  : Read Input Registers - registers are 16-bit registers used for input, and may only be read. Modbus *Function Code 4 (Read Input Registers)* will be used.

- **address** (**Required**, int): start address of the first register in a range (can be decimal or hexadecimal).
- **assumed_state** (*Optional* boolean): Disables updates (periodic read commands) for this register. Default is `false`.
- **skip_updates** (*Optional*, int): By default, all sensors of a modbus_controller are updated together. For data points that don't change very frequently, updates can be skipped. A value of 5 would only update this sensor range in every 5th update cycle. Note: The modbus_controller groups components by address ranges to reduce number of transactions. All components with the same starting address will be updated in one request. `skip_updates` applies for *all* components in the same range.
- **register_count** (*Optional*, int): The number of consecutive registers this read request should span or skip in a single command. Default is 1. See [Optimizing modbus communications](/components/modbus_controller#modbus_register_count) for more details.
- **use_write_multiple** (*Optional*, boolean): By default the modbus command *Function Code 6 (Preset Single Registers)* is used for setting the holding register if only one register is set. If your device only supports *Function Code 16 (Preset Multiple Registers)* set this option to `true`.
- **bitmask** (*Optional*, int): Some values are packed in a response. The bitmask is used to determined if the result is true or false. See [Bitmasks](/components/modbus_controller#bitmasks).
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated every update interval to read the status of the switch.

- **write_lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): Lambda called before send.
  Lambda is evaluated before the modbus write command is created.

  Parameters passed into the lambda

  - **x** (float): The float value to be sent to the modbus device
  - **payload** (`std::vector<uint8_t>&payload`  ): empty vector for the payload. If payload is set in the lambda it is sent as a custom command and must include all required bytes for a modbus request
    *note:* because the response contains data for all registers in the same range you have to use `data[item->offset]` to get the first response byte for your sensor.

  - **item** (const pointer to a Switch derived object): The sensor object itself.

  Possible return values for the lambda:

  - `return <true / false>;` the new value for the sensor.
  - `return <anything>; and fill payload with data` if the payload is added from the lambda then these bytes will be sent.
  - `return {};` in the case the lambda handles the sending of the value itself.

- **custom_command** (*Optional*, list of bytes): raw bytes for modbus command. This allows using non-standard commands. If `custom_command` is used `address` and `register_type` can't be used.
  Custom data must contain all required bytes including the modbus device address. The CRC is automatically calculated and appended to the command.
  See [Using `custom_command`](/components/modbus_controller#modbus_custom_command) how to use `custom_command`

- **offset** (*Optional*, int): Offset from start address in bytes (only required for uncommon response encodings). If more than one register is written in a command, this value is used to find the start of this datapoint relative to the start address. The component calculates the size of the range based on offset and size of the value type. The value for offset depends on the register type. For holding input registers, the offset is in bytes. For coil and discrete input resisters, the LSB of the first data byte contains the coil addressed in the request. The other coils follow toward the high-order end of this byte and from low order to high order in subsequent bytes. For registers, the offset is the position of the relevant bit. To get the value of the coil register, 2 can be retrieved using `address: 2` / `offset: 0` or `address: 0` / `offset 2`.
- **restore_mode** (*Optional*): See [Switch](/components/switch#config-switch), since this configuration variable is inherited. The default value for this setting is `DISABLED` (recommended).
  `DISABLED` leaves the initial state up to the hardware: usually the state lives in the device and ESPHome does not need to remember it. The switch frontend will show an undetermined
  state until the real state is retrieved from the device on the next refresh. Use any other setting if a reboot of your ESPHome device is tied to a reboot of the modbus device.

## Examples

```yaml
switch:
- platform: modbus_controller
    modbus_controller_id: epever
    id: enable_load_test
    register_type: coil
    address: 2
    name: "enable load test mode"
    bitmask: 1
```

```yaml
switch:
  - platform: modbus_controller
    modbus_controller_id: epever
    id: enable_load_test
    register_type: coil
    address: 2
    name: "enable load test mode"
    write_lambda: |-
      ESP_LOGD("main","Modbus Switch incoming state = %f",x);
      // return false ; // use this to just change the value
      payload.push_back(0x1);  // device address
      payload.push_back(0x5);  // force single coil
      payload.push_back(0x00); // high byte address of the coil
      payload.push_back(0x6);  // low byte address of the coil
      payload.push_back(0xFF); // ON = 0xFF00 OFF=0000
      payload.push_back(0x00);
      return true;
```

Since offset is not zero the read command is part of a range and will be parsed when the range is updated.
The write command to be constructed uses the function code to determine the write command. For a coil it is write single coil.
Because the write command only touches one register start_address and offset have to be corrected.
The final command will be write_single_coil *Function Code 5* address (start_address+offset) value 1 or 0

For holding registers the write command will be write_single_register (*Function Code 6 (Preset Single Registers)*). Because the offset for holding registers is given in bytes and the size of a register is 16 bytes the start_address is calculated as `start_address + offset/2`

```yaml
switch:
- platform: modbus_controller
  modbus_controller_id: ventilation_system
  name: "enable turn off"
  register_type: holding
  address: 25
  bitmask: 1
  entity_category: config
  icon: "mdi:toggle-switch"
```

## See Also

- {{< docref "/components/modbus" >}}
- {{< docref "/components/modbus_controller" >}}
- {{< docref "/components/sensor/modbus_controller" >}}
- {{< docref "/components/binary_sensor/modbus_controller" >}}
- {{< docref "/components/output/modbus_controller" >}}
- {{< docref "/components/number/modbus_controller" >}}
- {{< docref "/components/select/modbus_controller" >}}
- {{< docref "/components/text_sensor/modbus_controller" >}}
- <https://www.modbustools.com/modbus.html>
