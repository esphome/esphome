---
description: "Instructions for setting up a modbus_controller device binary sensor."
title: "Modbus Controller Binary Sensor"
params:
  seo:
    description: Instructions for setting up a modbus_controller device binary sensor.
    image: modbus.png
---

The `modbus_controller` binary sensor platform creates a binary sensor from a modbus_controller component
and requires {{< docref "/components/modbus_controller" >}} to be configured.

## Configuration variables

- **register_type** (**Required**): type of the modbus register.

  - `coil`  : Coils are 1-bit registers (ON/OFF values) that are used to control discrete outputs. Read and Write access. Modbus *Function Code 1 (Read Coil Status)* will be used.
  - `discrete_input`  : discrete input register (read only coil) are similar to coils but can only be read. Modbus *Function Code 2 (Read Input Status)* will be used.
  - `holding`  : Holding Registers - Holding registers are the most universal 16-bit register. Read and Write access. Modbus *Function Code 3 (Read Holding Registers)* will be used.
  - `read`  : Read Input Registers - registers are 16-bit registers used for input, and may only be read. Modbus *Function Code 4 (Read Input Registers)* will be used.

- **address** (**Required**, int): start address of the first register in a range (can be decimal or hexadecimal).
- **bitmask** (*Optional*, int): sometimes multiple values are packed in a single register's response. The bitmask is used to determined if the result is true or false. See [Bitmasks](/components/modbus_controller#bitmasks).
- **skip_updates** (*Optional*, int): By default all sensors of a modbus_controller are updated together. For data points that don't change very frequently updates can be skipped. A value of 5 would only update this sensor range in every 6th update cycle. Note: The modbus_controller groups components by address ranges to reduce number of transactions. All components with the same starting address will be updated in one request. `skip_updates` applies for *all* components in the same range.
- **register_count** (*Optional*, int): The number of consecutive registers this read request should span or skip in a single command. Default is 1. See [Optimizing modbus communications](/components/modbus_controller#modbus_register_count) for more details.
- **response_size** (*Optional*, int): Size of the response for the register in bytes. Defaults to register_count*2.
- **force_new_range** (*Optional*, boolean): If possible sensors with sequential addresses are grouped together and requested in one range. Setting `force_new_range: true` enforces the start of a new range at that address.
- **custom_command** (*Optional*, list of bytes): raw bytes for modbus command. This allows using non-standard commands. If `custom_command` is used `address` and `register_type` can't be used.
  Custom data must contain all required bytes including the modbus device address. The CRC is automatically calculated and appended to the command.
  See [Using `custom_command`](/components/modbus_controller#modbus_custom_command) how to use `custom_command`.

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated every update interval to get the new value of the sensor. Parameters:

  - **x** (bool): The parsed float value of the modbus data
  - **data** (std::vector<uint8_t>): vector containing the complete raw modbus response bytes for this sensor
  - **item** (const pointer to a ModbusBinarySensor object): The sensor object itself.

  Possible return values for the lambda:

  - `return true/false;` the new value for the sensor.

- **offset** (*Optional*, int): Offset from start address in bytes (only required for uncommon response encodings). If more than one register is written in a command, this value is used to find the start of this datapoint relative to the start address. The component calculates the size of the range based on offset and size of the value type. The value for offset depends on the register type. If a binary_sensor is created from an input register, the offset is in bytes. For coil and discrete input resisters, the LSB of the first data byte contains the coil addressed in the request. The other coils follow toward the high-order end of this byte and from low order to high order in subsequent bytes. For registers, the offset is the position of the relevant bit. To get the value of the coil register, 2 can be retrieved using `address: 2` / `offset: 0` or `address: 0` / `offset 2`.

- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## Example

```yaml
binary_sensor:
- platform: modbus_controller
  modbus_controller_id: modbus1
  name: "Error status"
  register_type: read
  address: 0x3200
  bitmask: 0x80 #(bit 8)
```

## See Also

- {{< docref "/components/modbus" >}}
- {{< docref "/components/modbus_controller" >}}
- {{< docref "/components/sensor/modbus_controller" >}}
- {{< docref "/components/output/modbus_controller" >}}
- {{< docref "/components/switch/modbus_controller" >}}
- {{< docref "/components/number/modbus_controller" >}}
- {{< docref "/components/select/modbus_controller" >}}
- {{< docref "/components/text_sensor/modbus_controller" >}}
- <https://www.modbustools.com/modbus.html>
- {{< apiclass "modbus_controller::ModbusBinarySensor" "modbus_controller::ModbusBinarySensor" >}}
