---
description: "Instructions for setting up Modbus in ESPHome."
title: "Modbus Component"
params:
  seo:
    description: Instructions for setting up Modbus in ESPHome.
---

{{< anchor "modbus" >}}

The Modbus protocol is used by many consumer and industrial devices for communication.
This component allows components in ESPHome to communicate to those devices via RTU protocol. You can access the coils, inputs, holding, read registers from your devices as sensors, switches, selects, numbers or various other ESPHome components and present them to your favorite Home Automation system. You can even write them as binary or float ouptputs from ESPHome.

The various sub-components implement some of the Modbus functions below (depending on their required functionality):

| Function Code | Description                |
| ------------- | -------------------------- |
| 1             | Read Coil Status           |
| 2             | Read Discrete input Status |
| 3 | Read Holding Registers |
| 4 | Read Input Registers |
| 5 | Write Single Coil |
| 6 | Write Single Register |
| 15 | Write Multiple Coils |
| 16 | Write Multiple Registers |

Modbus RTU requires a [UART Bus](/components/uart) to communicate.

```yaml
# Example configuration entry
uart:
  ...

modbus:
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

- **flow_control_pin** (*Optional*, [Pin](/guides/configuration-types#pin)): The pin used to switch flow control.
  This is useful for RS485 transceivers that do not have automatic flow control switching,
  like the common MAX485.

- **send_wait_time** (*Optional*, [Time](/guides/configuration-types#time)): Time in milliseconds before the next ModBUS command is sent when an answer from a previous command has not yet started (i.e. when to timeout and assume no response is coming). Defaults to 250 ms.
  Set this value to the maximum time required for the slowest device on the bus to begin responding (time to first byte).
  If a device starts responding within this time, the next command will be queued and sent after the response is finished, no matter how long the response.

- **disable_crc** (*Optional*, boolean): Ignores a bad CRC if set to `true`. Defaults to `false`

- **role** (*Optional*, string): The role of this component, `client` or `server`. Defaults to `client`.

## See Also

- {{< docref "/components/modbus_controller" >}}
- {{< docref "/components/sensor/modbus_controller" >}}
- {{< docref "/components/binary_sensor/modbus_controller" >}}
- {{< docref "/components/output/modbus_controller" >}}
- {{< docref "/components/switch/modbus_controller" >}}
- {{< docref "/components/number/modbus_controller" >}}
- {{< docref "/components/select/modbus_controller" >}}
- {{< docref "/components/text_sensor/modbus_controller" >}}
- [Modbus RTU Protocol Description](https://www.modbustools.com/modbus.html)
- [UART Bus](/components/uart)
- {{< apiref "modbus/modbus.h" "modbus/modbus.h" >}}
