---
description: "Instructions for setting up a modbus_controller device sensor."
title: "Modbus Controller Sensor"
params:
  seo:
    description: Instructions for setting up a modbus_controller device sensor.
    image: modbus.png
---

The `modbus_controller` sensor platform creates a sensor from a modbus_controller component
and requires {{< docref "/components/modbus_controller" >}} to be configured.

## Configuration variables

- **register_type** (**Required**): type of the modbus register.

  - `coil`  : Coils are 1-bit registers (ON/OFF values) that are used to control discrete outputs. They may be read and/or written. Modbus *Function Code 1 (Read Coil Status)* will be used.
  - `discrete_input`  : discrete input register (read only coil) are similar to coils but can only be read. Modbus *Function Code 2 (Read Input Status)* will be used.
  - `holding`  : Holding Registers - Holding registers are the most universal 16-bit register. They may be read and/or written. Modbus *Function Code 3 (Read Holding Registers)* will be used.
  - `read`  : Read Input Registers - registers are 16-bit registers used for input, and may only be read. Modbus *Function Code 4 (Read Input Registers)* will be used.

- **address** (**Required**, int): start address of the first register in a range (can be decimal or hexadecimal).
- **value_type** (**Required**): data type of the modbus register data. The default data type for modbus is a 16 bit integer in big endian format (MSB first).

  - `U_WORD`  : unsigned 16 bit integer from 1 register = 16bit
  - `S_WORD`  : signed 16 bit integer from 1 register = 16bit
  - `U_DWORD`  : unsigned 32 bit integer from 2 registers = 32bit
  - `S_DWORD`  : signed 32 bit integer from 2 registers = 32bit
  - `U_DWORD_R`  : unsigned 32 bit integer from 2 registers low word first
  - `S_DWORD_R`  : signed 32 bit integer from 2 registers low word first
  - `U_QWORD`  : unsigned 64 bit integer from 4 registers = 64bit
  - `S_QWORD`  : signed 64 bit integer from 4 registers = 64bit
  - `U_QWORD_R`  : unsigned 64 bit integer from 4 registers low word first
  - `S_QWORD_R`  : signed 64 bit integer from 4 registers low word first
  - `FP32`  : 32 bit IEEE 754 floating point from 2 registers
  - `FP32_R`  : 32 bit IEEE 754 floating point - same as FP32 but low word first

- **bitmask** (*Optional*, int): sometimes multiple values are packed in a single register's response. The bitmask can be used to extract a value from the response. See [Bitmasks](/components/modbus_controller#bitmasks).
- **skip_updates** (*Optional*, int): By default, all sensors of a modbus_controller are updated together. For data points that don't change very frequently, updates can be skipped. A value of 5 would only update this sensor range in every 6th update cycle. Note: The modbus_controller groups components by address ranges to reduce number of transactions. All components with the same starting address will be updated in one request. `skip_updates` applies for *all* components in the same range.
- **register_count** (*Optional*, int): The number of consecutive registers this read request should span or skip in a single command. Default is 1. See [Optimizing modbus communications](/components/modbus_controller#modbus_register_count) for more details.
- **response_size** (*Optional*, int): Size of the response for the register in bytes. Defaults to register_count*2.
- **force_new_range** (*Optional*, boolean): If possible sensors with sequential addresses are grouped together and requested in one range. Setting `force_new_range: true` enforces the start of a new range at that address.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)):
  Lambda to be evaluated every update interval to get the new value of the sensor.

  Parameters passed into the lambda

  - **x** (float): The parsed float value of the modbus data
  - **data** (std::vector<uint8_t): vector containing the complete raw modbus response bytes for this sensor
    *note:* because the response contains data for all registers in the same range you have to use `data[item->offset]` to get the first response byte for your sensor.

  - **item** (const pointer to a SensorItem derived object): The sensor object itself.

  Possible return values for the lambda:

  - `return <FLOATING_POINT_NUMBER>;` the new value for the sensor.
  - `return NAN;` if the state should be considered invalid to indicate an error (advanced).

- **custom_command** (*Optional*, list of bytes): raw bytes for modbus command. This allows using non-standard commands. If `custom_command` is used `address` and `register_type` can't be used.
  Custom data must contain all required bytes including the modbus device address. The CRC is automatically calculated and appended to the command.
  See [Using `custom_command`](/components/modbus_controller#modbus_custom_command) how to use `custom_command`

- **offset** (*Optional*, int): Offset from start address in bytes (only required for uncommon response encodings). If more than one register is written in a command this value is used to find the start of this datapoint relative to start address. The component calculates the size of the range based on offset and size of the value type. For `coil` or `discrete_input` registers offset is the position of the coil/register because these registers encode 8 coils in one byte.

- All other options from [Sensor](/components/sensor).

## Examples

The example below will send 2 modbus commands (device address 1 assumed):

`0x1 0x4 0x31 0x0 0x0 0x02 x7f 0x37` (read 2 registers starting at 0x3100)

`0x1 0x3 0x90 0x1 0x0 0x1 0xf8 0xca` (read 1 holding resister from 0x9001)

```yaml
- platform: modbus_controller
  modbus_controller_id: modbus1
  id: pv_input_voltage
  name: "PV array input voltage"
  address: 0x3100
  unit_of_measurement: "V" ## for any other unit the value is returned in minutes
  register_type: read
  value_type: U_WORD
  accuracy_decimals: 1
  filters:
    - multiply: 0.01

- platform: modbus_controller
  modbus_controller_id: modbus1
  name: "Battery Capacity"
  id: battery_capacity
  register_type: holding
  address: 0x9001
  unit_of_measurement: "AH"
  value_type: U_WORD
```

The `modbus` sensor platform allows you use a lambda that gets called before data is published
using [lambdas](/automations/templates#config-lambda).

The example below logs the value as parsed and the raw modbus bytes received for this register range:

```yaml
# Example configuration entry
sensor:
  - platform: modbus_controller
    modbus_controller_id: modbus1
    id: battery_capacity
    address: 0x9001
    name: "Battery Capacity"
    register_type: holding
    value_type: U_WORD
    lambda: |-
        ESP_LOGI("","Lambda incoming value=%f - data array size is %d",x,data.size());
        ESP_LOGI("","Sensor properties: adress = 0x%X, offset = 0x%X value type=%d",item->start_address,item->offset,item->sensor_value_type);
        int i=0 ;
        for (auto val : data) {
          ESP_LOGI("","data[%d]=0x%02X (%d)",i,data[i],data[i]);
          i++;
        }
        return x ;
```

## See Also

- {{< docref "/components/modbus" >}}
- {{< docref "/components/modbus_controller" >}}
- {{< docref "/components/binary_sensor/modbus_controller" >}}
- {{< docref "/components/output/modbus_controller" >}}
- {{< docref "/components/switch/modbus_controller" >}}
- {{< docref "/components/number/modbus_controller" >}}
- {{< docref "/components/select/modbus_controller" >}}
- {{< docref "/components/text_sensor/modbus_controller" >}}
- [EPEVER MPPT Solar Charge Controller (Tracer-AN Series)](https://devices.esphome.io/devices/epever_mptt_tracer_an)
