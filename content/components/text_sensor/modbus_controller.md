---
description: "Instructions for setting up a modbus_controller modbus text sensor."
title: "Modbus Controller Text Sensor"
params:
  seo:
    description: Instructions for setting up a modbus_controller modbus text sensor.
---

The `modbus_controller` sensor platform creates a text sensor from a modbus_controller component
and requires {{< docref "/components/modbus_controller" >}} to be configured.

## Configuration variables

- **register_type** (**Required**): type of the modbus register.

  - `coil`  : Coils are 1-bit registers (on/off values) that are used to control discrete outputs. They may be read and/or written. Modbus *Function Code 1 (Read Coil Status)* will be used.
  - `discrete_input`  : discrete input register (read only coil) are similar to coils but can only be read. Modbus *Function Code 2 (Read Input Status)* will be used.
  - `holding`  : Holding Registers - Holding registers are the most universal 16-bit register. They may be read and/or written. Modbus *Function Code 3 (Read Holding Registers)* will be used.
  - `read`  : Read Input Registers - registers are 16-bit registers used for input, and may only be read. Modbus *Function Code 4 (Read Input Registers)* will be used.

- **address** (**Required**, int): start address of the first register in a range (can be decimal or hexadecimal).
- **skip_updates** (*Optional*, int): By default all sensors of a modbus_controller are updated together. For data points that don't change very frequently updates can be skipped. A value of 5 would only update this sensor range in every 6th update cycle
- **register_count** (*Optional*, int): The number of consecutive registers this read request should span or skip in a single command. Default is 1. See [Optimizing modbus communications](/components/modbus_controller#modbus_register_count) for more details.
- **response_size** (**Required**): Number of bytes of the response.
- **raw_encode** (*Optional*, enum): If the response is binary it can't be published directly. Since a text sensor only publishes strings the binary data can be encoded. Defaults to `ANSI`. Possible encodings are:

  - `NONE`  : Don't encode data.
  - `HEXBYTES`  : 2 byte hex string. 0x2011 will be sent as "2011".
  - `COMMA`  : Byte values as integers, delimited by a coma. 0x2011 will be sent as "32,17".
  - `ANSI`  : Each byte is treated as an `ANSI` character. All control characters are ignored.

> [!NOTE]
> From version 2024.7, default encoding is `ANSI`. Thus, all control characters are now ignored. If you need to receive all characters, use `NONE` encoding.

- **force_new_range** (*Optional*, boolean): If possible sensors with sequential addresses are grouped together and requested in one range. Setting `force_new_range: true` enforces the start of a new range at that address.
- **custom_command** (*Optional*, list of bytes): raw bytes for modbus command. This allows using non-standard commands. If `custom_command` is used `address` and `register_type` can't be used.
  custom command must contain all required bytes including the modbus device address. The crc is automatically calculated and appended to the command.
  See [Using `custom_command`](/components/modbus_controller#modbus_custom_command) how to use `custom_command`

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated every update interval to get the new value of the sensor. It is called after the encoding according to **raw_encode**.

  Parameters passed into the lambda

  - **x** (std:string): The parsed value of the modbus data according to **raw_encode**
  - **data** (std::vector<uint8_t>): vector containing the complete raw modbus response bytes for this sensor
    *note:* because the response contains data for all registers in the same range you have to use `data[item->offset]` to get the first response byte for your sensor.

  - **item** (const pointer to a SensorItem derived object): The sensor object itself.

  Possible return values for the lambda:

  - `return <std::string>;` the new value for the sensor.
  - `return {};` uses the parsed value for the state (same as `return x;`  ).

- **offset** (*Optional*, int): Offset from start address in bytes (only required for uncommon response encodings). If more than one register is written in a command this value is used to find the start of this datapoint relative to start address. The component calculates the size of the range based on offset and size of the value type. The value for offset depends on the register type.
- All options from [Text Sensor](/components/text_sensor#config-text_sensor).

## Example

```yaml
text_sensor:
  - platform: modbus_controller
    modbus_controller_id: modbus_device
    id: reg_1002_text
    bitmask: 0
    register_type: holding
    address: 1002
    raw_encode: HEXBYTES
    name: Register 1002 (Text)
    lambda: |-
      uint16_t value = modbus_controller::word_from_hex_str(x, 0);
      switch (value) {
        case 1: return std::string("ready");
        case 2: return std::string("EV is present");
        case 3: return std::string("charging");
        case 4: return std::string("charging with ventilation");
        default: return std::string("Unknown");
      }
      return x;
```

## See Also

- {{< docref "/components/modbus" >}}
- {{< docref "/components/modbus_controller" >}}
- {{< docref "/components/sensor/modbus_controller" >}}
- {{< docref "/components/binary_sensor/modbus_controller" >}}
- {{< docref "/components/output/modbus_controller" >}}
- {{< docref "/components/switch/modbus_controller" >}}
- {{< docref "/components/number/modbus_controller" >}}
- {{< docref "/components/select/modbus_controller" >}}
- {{< docref "/components/text_sensor/modbus_controller" >}}
- <https://www.modbustools.com/modbus.html>
