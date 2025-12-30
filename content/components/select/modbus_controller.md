---
description: "Instructions for setting up Modbus Controller Select(s) with ESPHome."
title: "Modbus Controller Select"
params:
  seo:
    description: Instructions for setting up Modbus Controller Select(s) with ESPHome.
---

The `modbus_controller` Select platform allows you to create a Select from modbus
registers.

## Configuration variables

- **address** (**Required**, int): The start address of the first or only register
  of the Select (can be decimal or hexadecimal).

- **optionsmap** (**Required**, Map[str, int]): Provide a mapping from options (str) of
  this Select to values (int) of the modbus register and vice versa. All options and
  all values have to be unique.

- **value_type** (*Optional*): The datatype of the modbus data. Defaults to `U_WORD`.

  - `U_WORD` (unsigned 16 bit integer from 1 register = 16bit)
  - `S_WORD` (signed 16 bit integer from 1 register = 16bit)
  - `U_DWORD` (unsigned 32 bit integer from 2 registers = 32bit)
  - `S_DWORD` (signed 32 bit integer from 2 registers = 32bit)
  - `U_DWORD_R` (unsigned 32 bit integer from 2 registers low word first)
  - `S_DWORD_R` (signed 32 bit integer from 2 registers low word first)
  - `U_QWORD` (unsigned 64 bit integer from 4 registers = 64bit)
  - `S_QWORD` (signed 64 bit integer from 4 registers = 64bit)
  - `U_QWORD_R` (unsigned 64 bit integer from 4 registers low word first)
  - `U_QWORD_R` (signed 64 bit integer from 4 registers low word first)

- **register_count** (*Optional*): The number of registers which are used for this Select. Only
  required for uncommon response encodings or to
  [optimize modbus communications](/components/modbus_controller#modbus_register_count). Overrides the defaults determined
  by `value_type`.

- **skip_updates** (*Optional*, int): By default, all sensors of a modbus_controller are updated together. For data points that don't change very frequently, updates can be skipped. A value of 5 would only update this sensor range in every 6th update cycle. Note: The modbus_controller groups components by address ranges to reduce number of transactions. All components with the same starting address will be updated in one request. `skip_updates` applies for *all* components in the same range.

- **force_new_range** (*Optional*, boolean): If possible sensors with sequential addresses are
  grouped together and requested in one range. Setting this to `true` enforces the start of a new
  range at that address.

- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): Lambda to be evaluated every update interval
  to get the current option of the select.

  Parameters passed into lambda

  - **x** (`int64_t`  ): The parsed integer value of the modbus data.
  - **data** (`const std::vector<uint8_t>&`  ): vector containing the complete raw modbus response bytes for this
    sensor. Note: because the response contains data for all registers in the same range you have to
    use `data[item->offset]` to get the first response byte for your sensor.

  - **item** (`ModbusSelect*const`  ): The sensor object itself.

  Possible return values for the lambda:

  - `return <std::string>;` The new option for this Select.
  - `return {};` Use default mapping (see `optionsmap`  ).

- **write_lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): Lambda to be evaluated on every update
  of the Sensor, before the new value is written to the modbus registers.

- **use_write_multiple** (*Optional*, boolean): By default the modbus command *Function Code 6 (Preset Single Registers)*
  is used for setting the holding register if only one register is set. If your device only supports *Function Code 16 (Preset Multiple Registers)* set this option to `true`.

- **optimistic** (*Optional*, boolean): Whether to operate in optimistic mode - when in this mode,
  any command sent to the Modbus Select will immediately update the reported state. Defaults
  to `false`.

- All other options from [Select](/components/select#config-select).

```yaml
# example
lambda: |-
  ESP_LOGD("Reg1000", "Received value %lld", x);
  ESP_LOGD("Reg1000", "Parsed from bytes 0x%x;0x%x", data[item->offset], data[item->offset + 1]);
  if (x > 3) {
    return std::string("Three");
  }
```

## Parameters passed into `write_lambda`

- **x** (`const std::string&`  ): The option value to set for this Select.
- **value** (`int64_t`  ): The mapping value of `x` using `optionsmap`.
- **payload** (`std::vector<uint16_t>& payload`  ): Empty vector for the payload. The lamdba can add
  16 bit raw modbus register words which are send to the modbus device.

- **item** (`ModbusSelect*const`  ): The sensor object itself.

Possible return values for the lambda:

- `return <int64_t>;` the value which should be written to the configured modbus registers. If there were data written to `payload` this value is ignored.
- `return {};` Skip updating the register.

```yaml
# example
write_lambda: |-
  ESP_LOGD("Reg1000", "Set option to %s (%lld)", x.c_str(), value);

  // re-use default option value from optionsmap
  if (value == 0) {
    return value;
  }

  // return own option value
  if (x == "One") {
    return 2;
  }

  // write payload
  if (x == "Two") {
    payload.push_back(0x0001);
    return 0; // any value will do
  }

  // ignore update
  return {};
```

## Example

```yaml
# Example configuration entry
select:
  - platform: modbus_controller
    name: "Modbus Select Register 1000"
    address: 1000
    value_type: U_WORD
    optionsmap:
      "Zero": 0
      "One": 1
      "Two": 2
      "Three": 3
```

## See Also

- {{< docref "/components/modbus" >}}
- {{< docref "/components/modbus_controller" >}}
- {{< docref "/components/sensor/modbus_controller" >}}
- {{< docref "/components/binary_sensor/modbus_controller" >}}
- {{< docref "/components/output/modbus_controller" >}}
- {{< docref "/components/switch/modbus_controller" >}}
- {{< docref "/components/number/modbus_controller" >}}
- {{< docref "/components/text_sensor/modbus_controller" >}}
- [Automation](/automations)
- <https://www.modbustools.com/modbus.html>
