---
description: "Instructions for setting up a modbus_controller device output."
title: "Modbus Controller Output"
params:
  seo:
    description: Instructions for setting up a modbus_controller device output.
---

The `modbus_controller` platform creates an output from a modbus_controller. The goal is to write a value to a modbus register on a device.

## Configuration variables

- **address** (**Required**, int): start address of the first register in a range (can be decimal or hexadecimal).
- **value_type** (**Required**): data type of the modbus register data. The default data type for modbus is a 16 bit integer in big endian format (MSB first).

  - `U_WORD` (unsigned 16 bit integer from 1 register = 16bit)
  - `S_WORD` (signed 16 bit integer from 1 register = 16bit)
  - `U_DWORD` (unsigned 32 bit integer from 2 registers = 32bit)
  - `S_DWORD` (signed 32 bit integer from 2 registers = 32bit)
  - `U_DWORD_R` (unsigned 32 bit integer from 2 registers low word first)
  - `S_DWORD_R` (signed 32 bit integer from 2 registers low word first)
  - `U_QWORD` (unsigned 64 bit integer from 4 registers = 64bit)
  - `S_QWORD` (signed 64 bit integer from 4 registers = 64bit)
  - `U_QWORD_R` (unsigned 64 bit integer from 4 registers low word first)
  - `S_QWORD_R` signed 64 bit integer from 4 registers low word first)
  - `FP32` (32 bit IEEE 754 floating point from 2 registers)
  - `FP32_R` (32 bit IEEE 754 floating point - same as FP32 but low word first)

- **register_type** (*Optional*):
  - `coil`  : Write Coil - Write the ON/OFF status of a discrete coil in the device with *Function Code 5 or 15*. This will create a binary output.
  - `holding`  : Write Holding Registers - write contents of holding registers in the device with *Function Code 6 or 16*. This will create a float output.

- **multiply** (*Optional*, float): multiply the incoming value with this factor before writing it to the device. Ignored if `write_lambda` is defined. Only valid for `register_type: holding`.
- **use_write_multiple** (*Optional*, boolean): By default the modbus command *Function Code 6 (Preset Single Registers)* is used for setting the holding register if only one register is set. If your device only supports *Function Code 16 (Preset Multiple Registers)* set this option to `true`.
- **write_lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda is evaluated before the modbus write command is created. The value is passed in as `float x` and an empty vector is passed in as `std::vector<uint16_t>&payload`.
  You can directly define the payload by adding data to payload then the return value is ignored and the content of payload is used.

  Parameters passed into the lambda

  - **x** (float or bool): The float value to be sent to the modbus device for `register_type: holding` or the boolean value to be sent to the modbus device for `register_type: coil`
  - **payload** (`std::vector<uint16_t>&payload`):

    - for `register_type: holding`  : empty vector for the payload. The lamdba can add 16 bit raw modbus register words.
    - for `register_type: coil`  : empty vector for the payload. If payload is set in the lambda it is sent as a custom command and must include all required bytes for a modbus request
      note: because the response contains data for all registers in the same range you have to use `data[item->offset]` to get the first response byte for your sensor.

  - **item** (const pointer to a SensorItem derived object): The sensor object itself.

  Possible return values for the lambda:

  - `return <FLOATING_POINT_NUMBER / BOOL>;` the new value for the sensor.
  - `return <anything>; and fill payload with data` if the payload is added from the lambda then these 16 bit words will be sent
  - `return {};` if you don't want write the command to the device (or do it from the lambda).

- **offset** (*Optional*, int): Offset from start address in bytes (only required for uncommon response encodings). If more than one register is written in a command this value is used to find the start of this datapoint relative to start address. The component calculates the size of the range based on offset and size of the value type.
- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

All other options from [Output](/components/output#config-output).

## Example

```yaml
output:
  - platform: modbus_controller
    modbus_controller_id: modbus1
    address: 2048
    register_type: holding
    value_type: U_WORD
    multiply: 1000
```

**The same with lambda:**

```yaml
output:
  - platform: modbus_controller
    modbus_controller_id: modbus1
    address: 2048
    value_type: U_WORD
    write_lambda: |-
      ESP_LOGD("main","Modbus Output incoming value = %f",x);
      uint16_t value = x ;
      payload.push_back(value);
      return x * 1000 ;
```

## See Also

- {{< docref "/components/modbus" >}}
- {{< docref "/components/modbus_controller" >}}
- {{< docref "/components/sensor/modbus_controller" >}}
- {{< docref "/components/binary_sensor/modbus_controller" >}}
- {{< docref "/components/switch/modbus_controller" >}}
- {{< docref "/components/number/modbus_controller" >}}
- {{< docref "/components/select/modbus_controller" >}}
- {{< docref "/components/text_sensor/modbus_controller" >}}
- <https://www.modbustools.com/modbus.html>
